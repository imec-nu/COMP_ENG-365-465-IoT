#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include "img_converters.h"
#include <esp_heap_caps.h>
#include "config.h"
#include "image_transform.h"
// Camera setup and JPEG decoding are supplied.
static uint8_t *decodedRGB=nullptr;
static uint8_t grayInput[INPUT_W*INPUT_H];
static bool initCameraFrontend() {
  if (!psramFound()) return false;
  camera_config_t c={};
  c.ledc_channel=LEDC_CHANNEL_0;c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=15;c.pin_d1=17;c.pin_d2=18;c.pin_d3=16;
  c.pin_d4=14;c.pin_d5=12;c.pin_d6=11;c.pin_d7=48;
  c.pin_xclk=10;c.pin_pclk=13;c.pin_vsync=38;c.pin_href=47;
  c.pin_sccb_sda=40;c.pin_sccb_scl=39;c.pin_pwdn=-1;c.pin_reset=-1;
  c.xclk_freq_hz=20000000;c.pixel_format=PIXFORMAT_JPEG;
  c.frame_size=FRAMESIZE_QVGA;c.jpeg_quality=12;c.fb_count=1;
  c.fb_location=CAMERA_FB_IN_PSRAM;c.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
  esp_err_t err=esp_camera_init(&c);
  if(err!=ESP_OK){Serial.printf("# ERROR camera init 0x%x\n",err);return false;}
  decodedRGB=(uint8_t*)heap_caps_malloc(RAW_W*RAW_H*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!decodedRGB)return false;
  // Let auto-exposure settle by consuming frames. No mirroring/rotation applied.
  for(int i=0;i<10;i++){
    camera_fb_t *fb=esp_camera_fb_get();if(!fb)return false;
    esp_camera_fb_return(fb);delay(30);
  }
  return true;
}
static camera_fb_t *captureFresh(){
  // Discard queued/older frames after idle time before retaining one frame.
  for(int i=0;i<2;i++){
    camera_fb_t *old=esp_camera_fb_get();if(!old)return nullptr;
    esp_camera_fb_return(old);
  }
  return esp_camera_fb_get();
}
static bool prepareImage(camera_fb_t *fb){
  if(!fb || fb->format!=PIXFORMAT_JPEG || fb->width!=RAW_W || fb->height!=RAW_H)return false;
  if(!fmt2rgb888(fb->buf,fb->len,fb->format,decodedRGB))return false;
  return makeModelInput(decodedRGB, grayInput);
}
