#pragma once
#include "camera_frontend.h"
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#if __has_include("model_data.h") && __has_include("model_config.h")
#include "model_data.h"
#include "model_config.h"
#else
#error "Train the gesture model, then copy its model_data.h and model_config.h into gesture_inference."
#endif

bool fillModelInput();
int decodePrediction(float *scores, const char *&prediction);
constexpr int CLASSES=sizeof(CLASS_LABELS)/sizeof(CLASS_LABELS[0]);
tflite::MicroInterpreter *interpreter=nullptr;
TfLiteTensor *inputTensor=nullptr,*outputTensor=nullptr;
bool automatic=false;uint32_t nextCapture=0,trials=0;double sumMs=0;
void fatal(const char *msg){Serial.print("# FATAL ");Serial.println(msg);while(true)delay(1000);}
void infer(){
  digitalWrite(LED_BUILTIN,LOW);uint32_t t=micros();camera_fb_t *fb=captureFresh();
  float captureMs=(uint32_t)(micros()-t)/1000.f;
  if(!fb){digitalWrite(LED_BUILTIN,HIGH);Serial.println("# ERROR capture");return;}
  t=micros();bool ok=prepareImage(fb);float prepMs=(uint32_t)(micros()-t)/1000.f;
  esp_camera_fb_return(fb);digitalWrite(LED_BUILTIN,HIGH);
  if(!ok){Serial.println("# ERROR preprocessing");return;}
  t=micros();
  if (!fillModelInput()) { Serial.println("# ERROR complete TODO 2.5"); return; }
  float inputMs=(uint32_t)(micros()-t)/1000.f;
  t=micros();TfLiteStatus result=interpreter->Invoke();float inferenceMs=(uint32_t)(micros()-t)/1000.f;
  if(result!=kTfLiteOk){Serial.println("# ERROR Invoke failed");return;}
  float scores[CLASSES] = {};
  const char *pred = nullptr;
  const int best = decodePrediction(scores, pred);
  if (best < 0 || best >= CLASSES || !pred) {
    Serial.println("# ERROR complete TODO 2.6");
    return;
  }
  trials++;sumMs+=inferenceMs;
  Serial.printf("# RESULT trial=%lu candidate=%s predicted=%s score=%.4f threshold=%.4f\n",(unsigned long)trials,CLASS_LABELS[best],pred,scores[best],ACCEPT_THRESHOLD);
  Serial.print("# SCORES");for(int i=0;i<CLASSES;i++)Serial.printf(" %s=%.4f",CLASS_LABELS[i],scores[i]);Serial.println();
  Serial.printf("# TIMING capture_ms=%.2f preprocess_ms=%.2f input_ms=%.2f inference_ms=%.2f mean_inference_ms=%.2f\n",captureMs,prepMs,inputMs,inferenceMs,sumMs/trials);
}
static void beginInference(){
  Serial.begin(115200);while(!Serial)delay(10);pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,HIGH);
  if(!initCameraFrontend())fatal("Camera init failed: check OPI PSRAM, board and camera connection.");
  auto *backing=(uint8_t*)heap_caps_malloc(ARENA_BYTES+15,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!backing)fatal("Arena allocation failed");
  auto *arena=(uint8_t*)((uintptr_t(backing)+15)&~uintptr_t(15));
  const tflite::Model *model=tflite::GetModel(g_model);
  if(model->version()!=TFLITE_SCHEMA_VERSION)fatal("Model schema mismatch");
  static tflite::MicroMutableOpResolver<8> resolver;
  if(resolver.AddConv2D()!=kTfLiteOk || resolver.AddMaxPool2D()!=kTfLiteOk ||
     resolver.AddFullyConnected()!=kTfLiteOk || resolver.AddSoftmax()!=kTfLiteOk ||
     resolver.AddReshape()!=kTfLiteOk || resolver.AddQuantize()!=kTfLiteOk ||
     resolver.AddDequantize()!=kTfLiteOk || resolver.AddMean()!=kTfLiteOk)fatal("Operator registration failed");
  static tflite::MicroInterpreter instance(model,resolver,arena,ARENA_BYTES);interpreter=&instance;
  if(interpreter->AllocateTensors()!=kTfLiteOk)fatal("AllocateTensors failed; read preceding runtime error");
  inputTensor=interpreter->input(0);outputTensor=interpreter->output(0);
  if(inputTensor->type!=kTfLiteInt8 || outputTensor->type!=kTfLiteInt8)fatal("Expected INT8 model");
  if(inputTensor->dims->size!=4 || inputTensor->dims->data[0]!=1 || inputTensor->dims->data[1]!=64 ||
     inputTensor->dims->data[2]!=64 || inputTensor->dims->data[3]!=1 || inputTensor->bytes!=4096)fatal("Expected [1,64,64,1]");
  if(CLASSES!=4 || outputTensor->dims->size!=2 || outputTensor->dims->data[0]!=1 ||
     outputTensor->dims->data[1]!=CLASSES || outputTensor->bytes!=CLASSES)fatal("Expected four-class output");
  if(BACKGROUND_INDEX<0 || BACKGROUND_INDEX>=CLASSES || !isfinite(INPUT_SCALE) || INPUT_SCALE<=0 ||
     !isfinite(OUTPUT_SCALE) || OUTPUT_SCALE<=0 || !isfinite(ACCEPT_THRESHOLD) || ACCEPT_THRESHOLD<0 || ACCEPT_THRESHOLD>1)fatal("Invalid model config");
  if(inputTensor->params.zero_point!=INPUT_ZERO_POINT || outputTensor->params.zero_point!=OUTPUT_ZERO_POINT ||
     fabsf(inputTensor->params.scale-INPUT_SCALE)>fmaxf(1e-9f,INPUT_SCALE*1e-5f) ||
     fabsf(outputTensor->params.scale-OUTPUT_SCALE)>fmaxf(1e-9f,OUTPUT_SCALE*1e-5f))fatal("Model headers disagree");
  Serial.printf("# MODEL bytes=%u arena_capacity=%u arena_used=%u\n",g_model_len,(unsigned)ARENA_BYTES,(unsigned)interpreter->arena_used_bytes());
  Serial.println("# READY r=one image, a=automatic, s=stop. Center your hand in the same crop used for training.");
}
static void pollInference(){
  bool once=false;
  while(Serial.available()){
    char c=Serial.read();
    if(c=='r'||c=='R')once=true;
    if(c=='a'||c=='A'){automatic=true;nextCapture=millis();}
    if(c=='s'||c=='S'){automatic=false;once=false;Serial.println("# AUTO stopped");}
  }
  if(once||(automatic&&(int32_t)(millis()-nextCapture)>=0)){infer();nextCapture=millis()+AUTO_INTERVAL_MS;}
  delay(5);
}
