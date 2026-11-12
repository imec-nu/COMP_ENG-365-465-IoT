#pragma once
// Microphone setup, feature extraction, USB transfer, and sketch entry points.
// XIAO ESP32S3 Sense; Arduino-ESP32 3.3.12; arduinoFFT 2.0.4.
// USB CDC On Boot: Enabled. PSRAM: OPI PSRAM.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <arduinoFFT.h>
#include "frontend.h"
I2SClass mic;
int16_t *audio;
float *features;
uint32_t sequence = 0;

bool readExact(void *destination, size_t bytes);

uint32_t crcUpdate(uint32_t c, const uint8_t *p, size_t n) {
  while (n--) { c ^= *p++; for (int j=0;j<8;j++) c=(c>>1)^((c&1)?0xedb88320UL:0); }
  return c;
}
bool sendBytes(const void *p, size_t n) {
  const uint8_t *b=(const uint8_t*)p;
  uint32_t last=millis();
  while(n) {
    size_t sent=Serial.write(b, n>1024?1024:n);
    if(sent) {b+=sent;n-=sent;last=millis();}
    else if(millis()-last>10000) return false;
    delay(1);
  }
  return true;
}
void capture() {
  // Restart RX for each request: never capture stale queued DMA audio.
  mic.setPinsPdmRx(MIC_CLOCK_PIN, MIC_DATA_PIN);
  if (!mic.begin(I2S_MODE_PDM_RX, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("# ERROR microphone begin failed"); return;
  }
  int16_t warmup[512];
  for(int i=0;i<8;i++) if(!readExact(warmup,sizeof(warmup))) {
    mic.end(); Serial.println("# ERROR microphone warmup failed"); return;
  }
  Serial.println("# RECORDING 2 seconds; speak now");
  digitalWrite(LED_BUILTIN, LOW);
  bool ok=readExact(audio, NSAMPLES*sizeof(int16_t));
  mic.end(); digitalWrite(LED_BUILTIN,HIGH);
  if(!ok) {Serial.println("# ERROR incomplete recording");return;}
  Serial.println("# PROCESSING");
  uint32_t t=micros(); extractFeatures(audio,features); uint32_t us=micros()-t;
  const uint32_t ab=NSAMPLES*2, fb=NFRAMES*NMELS*4;
  uint32_t crc=crcUpdate(0xffffffff,(uint8_t*)audio,ab);
  crc=crcUpdate(crc,(uint8_t*)features,fb)^0xffffffff;
  // Explicit uint32 little-endian fields on ESP32; no packed-struct padding.
  uint32_t header[]={1,++sequence,SR,NSAMPLES,NFFT,HOP,NFRAMES,NMELS,ab,fb,us,crc};
  if(!sendBytes("KWS7",4) || !sendBytes(header,sizeof(header)) ||
     !sendBytes(audio,ab) || !sendBytes(features,fb)) return;
  Serial.println("# DONE");
}
void setup() {
  Serial.begin(115200); Serial.setTimeout(1000);
  pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,HIGH);
  audio=(int16_t*)ps_malloc(NSAMPLES*sizeof(int16_t));
  features=(float*)ps_malloc(NFRAMES*NMELS*sizeof(float));
  while(!Serial) delay(10);
  if(!audio || !features) {
    while(true){Serial.println("# ERROR enable OPI PSRAM");delay(2000);}
  }
  initFrontend(); Serial.println("# READY KWS7 frontend=logmel_v1");
}
void loop() {
  if(!Serial.available()){delay(5);return;}
  String cmd=Serial.readStringUntil('\n');cmd.trim();
  if(cmd=="CAPTURE") capture();
  else if(cmd=="PING") Serial.println("# READY KWS7 frontend=logmel_v1");
  else Serial.println("# ERROR expected CAPTURE or PING");
}
