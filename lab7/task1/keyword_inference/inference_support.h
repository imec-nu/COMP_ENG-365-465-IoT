#pragma once
// Hardware, model setup, timing, and recording commands are provided here.
// Lab 7: 2-second on-device keyword spotting, INT8 TFLite Micro.
// Copy YOUR trained_model/model_data.h and model_config.h into this folder.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <arduinoFFT.h>
#include <esp_heap_caps.h>
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "frontend.h"
#if !__has_include("model_data.h") || !__has_include("model_config.h")
#error "Train your model, then copy model_data.h and model_config.h into keyword_inference."
#endif
#include "model_data.h"
#include "model_config.h"

constexpr int CLASS_COUNT = sizeof(CLASS_LABELS) / sizeof(CLASS_LABELS[0]);
I2SClass mic;
int16_t *pcm = nullptr;
float *features = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *inputTensor = nullptr, *outputTensor = nullptr;
bool automatic = false;
uint32_t trial = 0, nextCapture = 0;
double sumInferenceMs = 0;
double sumFrontendMs = 0;

bool prepareModelInput();
const char *interpretOutput(float *scores, int &best);

void fatal(const char *message) {
  Serial.print("# FATAL "); Serial.println(message);
  while (true) delay(1000);
}

bool readExact(void *dst, size_t size) {
  size_t done = 0;
  uint32_t start = millis();
  while (done < size && millis() - start < READ_TIMEOUT_MS) {
    size_t n = mic.readBytes((char*)dst + done, size - done);
    if (!n) return false;
    done += n;
  }
  return done == size;
}

bool recordClip(float &captureMs) {
  // Reinitialize RX to avoid stale audio accumulated while processing.
  mic.setPinsPdmRx(MIC_CLOCK_PIN, MIC_DATA_PIN);
  if (!mic.begin(I2S_MODE_PDM_RX, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("# ERROR microphone initialization failed"); return false;
  }
  int16_t warmup[512];
  for (int i=0; i<8; ++i) {
    if (!readExact(warmup, sizeof(warmup))) {
      mic.end(); Serial.println("# ERROR microphone warmup failed"); return false;
    }
  }
  Serial.println("# RECORDING 2 seconds; speak now (LED on)");
  digitalWrite(LED_BUILTIN, LOW);
  uint32_t t = micros();
  bool ok = readExact(pcm, NSAMPLES * sizeof(int16_t));
  captureMs = (uint32_t)(micros() - t) / 1000.0f;
  digitalWrite(LED_BUILTIN, HIGH);
  mic.end();
  if (!ok) Serial.println("# ERROR incomplete audio; inference skipped");
  return ok;
}

void runTrial() {
  uint32_t cycleStart = micros();
  float captureMs = 0;
  if (!recordClip(captureMs)) return;
  Serial.println("# PROCESSING");
  uint32_t t = micros();
  extractFeatures(pcm, features);
  float frontendMs = (uint32_t)(micros() - t) / 1000.0f;

  // Same train-only scalar normalization and INT8 transform as export.
  // frontend output is [time,mel]; tensor is [1,time,mel,1].
  t = micros();
  if (!prepareModelInput()) {
    Serial.println("# ERROR input preparation failed; check TODO 1.6");
    return;
  }
  float inputMs = (uint32_t)(micros() - t) / 1000.0f;
  t = micros();
  TfLiteStatus status = interpreter->Invoke();
  float inferenceMs = (uint32_t)(micros() - t) / 1000.0f;
  if (status != kTfLiteOk) {Serial.println("# ERROR Invoke failed"); return;}

  int best = 0;
  float scores[CLASS_COUNT];
  const char *predicted = interpretOutput(scores, best);
  if (predicted == nullptr) {
    Serial.println("# ERROR output interpretation unfinished; check TODO 1.7");
    return;
  }
  ++trial; sumInferenceMs += inferenceMs; sumFrontendMs += frontendMs;
  float cycleMs = (uint32_t)(micros() - cycleStart) / 1000.0f;
  Serial.printf("# RESULT trial=%lu candidate=%s predicted=%s score=%.4f threshold=%.4f\n",
                (unsigned long)trial, CLASS_LABELS[best], predicted, scores[best], ACCEPT_THRESHOLD);
  Serial.print("# SCORES");
  for (int i=0; i<CLASS_COUNT; ++i) Serial.printf(" %s=%.4f", CLASS_LABELS[i], scores[i]);
  Serial.println();
  Serial.printf("# TIMING capture_ms=%.2f frontend_ms=%.2f input_ms=%.2f inference_ms=%.2f mean_frontend_ms=%.2f mean_inference_ms=%.2f cycle_ms=%.2f\n",
                captureMs, frontendMs, inputMs, inferenceMs, sumFrontendMs/trial, sumInferenceMs/trial, cycleMs);
  // Human-readable timing excludes result-printing. Cycle includes microphone startup/warmup.
  Serial.println("# READY r=record, a=auto, s=stop-auto");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("# INIT keyword inference / logmel_v1");
  if (!psramFound()) fatal("Enable OPI PSRAM in Tools, then upload again.");
  pcm = (int16_t*)heap_caps_malloc(NSAMPLES*sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  features = (float*)heap_caps_malloc(NFRAMES*NMELS*sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  // Keep backing allocation alive and align arena explicitly to 16 bytes.
  uint8_t *backing = (uint8_t*)heap_caps_malloc(ARENA_BYTES+15, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pcm || !features || !backing) fatal("PSRAM allocation failed.");
  uint8_t *arena = (uint8_t*)((uintptr_t(backing)+15) & ~uintptr_t(15));
  initFrontend();
  const tflite::Model *model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) fatal("TFLite schema mismatch.");
  static tflite::MicroMutableOpResolver<8> resolver;
  if (resolver.AddConv2D()!=kTfLiteOk || resolver.AddMaxPool2D()!=kTfLiteOk ||
      resolver.AddMean()!=kTfLiteOk || resolver.AddFullyConnected()!=kTfLiteOk ||
      resolver.AddSoftmax()!=kTfLiteOk || resolver.AddReshape()!=kTfLiteOk ||
      resolver.AddQuantize()!=kTfLiteOk || resolver.AddDequantize()!=kTfLiteOk)
    fatal("Operator registration failed.");
  static tflite::MicroInterpreter staticInterpreter(model, resolver, arena, ARENA_BYTES);
  interpreter = &staticInterpreter;
  if (interpreter->AllocateTensors()!=kTfLiteOk)
    fatal("AllocateTensors failed: inspect preceding operator/arena error. Arena capacity is 256 KiB.");
  inputTensor = interpreter->input(0); outputTensor = interpreter->output(0);
  if (inputTensor->type!=kTfLiteInt8 || outputTensor->type!=kTfLiteInt8)
    fatal("Use model_data.h exported from the INT8 model.");
  if (inputTensor->dims->size!=4 || inputTensor->dims->data[0]!=1 ||
      inputTensor->dims->data[1]!=NFRAMES || inputTensor->dims->data[2]!=NMELS ||
      inputTensor->dims->data[3]!=1 || inputTensor->bytes!=NFRAMES*NMELS)
    fatal("Expected input [1,124,32,1].");
  if (outputTensor->dims->size!=2 || outputTensor->dims->data[0]!=1 ||
      outputTensor->dims->data[1]!=CLASS_COUNT || outputTensor->bytes!=CLASS_COUNT)
    fatal("Output shape does not match CLASS_LABELS.");
  if (UNKNOWN_INDEX<0 || UNKNOWN_INDEX>=CLASS_COUNT || !isfinite(INPUT_MEAN) ||
      !isfinite(INPUT_STD) || INPUT_STD<=0 || !isfinite(INPUT_SCALE) || INPUT_SCALE<=0 ||
      !isfinite(ACCEPT_THRESHOLD) || ACCEPT_THRESHOLD<0 || ACCEPT_THRESHOLD>1)
    fatal("Invalid exported model configuration.");
  // Detect accidentally mixing headers from different training runs.
  if (inputTensor->params.zero_point!=INPUT_ZERO_POINT ||
      fabsf(inputTensor->params.scale-INPUT_SCALE)>fmaxf(1e-9f,INPUT_SCALE*1e-5f) ||
      outputTensor->params.zero_point!=OUTPUT_ZERO_POINT ||
      fabsf(outputTensor->params.scale-OUTPUT_SCALE)>fmaxf(1e-9f,OUTPUT_SCALE*1e-5f))
    fatal("model_data.h and model_config.h quantization parameters disagree.");
  Serial.printf("# MODEL bytes=%u arena_capacity=%u arena_used=%u classes=%d threshold=%.4f\n",
                g_model_len, (unsigned)ARENA_BYTES, (unsigned)interpreter->arena_used_bytes(), CLASS_COUNT, ACCEPT_THRESHOLD);
  Serial.print("# LABELS");
  for(int i=0;i<CLASS_COUNT;++i) Serial.printf(" %d:%s",i,CLASS_LABELS[i]);
  Serial.println();
  Serial.println("# READY Send r to record once; a for repeated windows; s to stop auto.");
}

void loop() {
  bool once = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (c=='r' || c=='R') once=true;
    if (c=='a' || c=='A') {automatic=true;nextCapture=millis();}
    if (c=='s' || c=='S') {automatic=false;once=false;Serial.println("# AUTO stopped");}
  }
  if (once || (automatic && (int32_t)(millis()-nextCapture)>=0)) {
    runTrial(); nextCapture=millis()+AUTO_GAP_MS; // Gap, not continuous streaming.
  }
  delay(5);
}
