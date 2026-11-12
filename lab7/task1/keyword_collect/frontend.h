#pragma once
#include <Arduino.h>
#include <arduinoFFT.h>
#include "config.h"
// Reuse this exact frontend for subsequent on-board inference.
// logmel_v1: PCM /32768, frame DC removal, symmetric Hann, unnormalized
// FFT power, continuous-Hz HTK triangular mel filters, natural log(max(E,1e-12)).
static float re[NFFT], im[NFFT], windowFn[NFFT], edges[NMELS+2];
static ArduinoFFT<float> fft(re,im,NFFT,SR);
static float hzToMel(float hz){return 2595.0f*log10f(1.0f+hz/700.0f);}
static void initFrontend(){
  for(int i=0;i<NFFT;i++)windowFn[i]=0.5f-0.5f*cosf(2.0f*PI*i/(NFFT-1));
  float lo=hzToMel(80),hi=hzToMel(7600);
  for(int m=0;m<NMELS+2;m++)edges[m]=700*(powf(10,(lo+(hi-lo)*m/(NMELS+1))/2595)-1);
}
static void extractFeatures(const int16_t *pcm,float *out){
  for(int t=0;t<NFRAMES;t++){
    float mean=0;
    for(int i=0;i<NFFT;i++)mean+=pcm[t*HOP+i]/32768.0f;
    mean/=NFFT;
    for(int i=0;i<NFFT;i++){re[i]=(pcm[t*HOP+i]/32768.0f-mean)*windowFn[i];im[i]=0;}
    fft.compute(FFTDirection::Forward);
    for(int m=0;m<NMELS;m++){
      float energy=0;
      for(int k=0;k<=NFFT/2;k++){
        float hz=k*(float)SR/NFFT,w=0;
        if(hz>=edges[m] && hz<=edges[m+1])w=(hz-edges[m])/(edges[m+1]-edges[m]);
        else if(hz>edges[m+1] && hz<=edges[m+2])w=(edges[m+2]-hz)/(edges[m+2]-edges[m+1]);
        energy+=w*(re[k]*re[k]+im[k]*im[k]);
      }
      out[t*NMELS+m]=logf(fmaxf(energy,1e-12f));
    }
  }
}
