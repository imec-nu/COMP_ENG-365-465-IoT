// Lab 6 Task 2. Complete TODOs 2.1-2.3 in this file.
// config.h contains settings; feature_support.h captures and transfers audio.
#include "feature_support.h"

void applyHannWindow(const int16_t *input) {
  // TODO 2.1: YOUR CODE HERE
  // Find the mean of the FRAME_SAMPLES input values. For each n, store
  // (input[n] - mean) * hann[n] in fftReal[n] and set fftImag[n] to zero.
  // Use float arithmetic. The Hann coefficients are already in hann[].
  (void)input;
  for (size_t n = 0; n < FRAME_SAMPLES; ++n) {
    fftReal[n] = 0.0f;  // Replace with your windowed samples.
    fftImag[n] = 0.0f;
  }
}

void computeMagnitudeSpectrum() {
  // TODO 2.2: YOUR CODE HERE
  // Run fft.compute(FFTDirection::Forward). It updates fftReal/fftImag.
  // For bins 0 through FFT_BINS - 1, store sqrt(re*re + im*im) in magnitude.
  // Complete peakFrequencyHz() below as the second part of this TODO.
  for (uint16_t k = 0; k < FFT_BINS; ++k) magnitude[k] = 0.0f;
}

float peakFrequencyHz(const float *spectrum) {
  // Part of TODO 2.2: search spectrum[1] through spectrum[SPECTRUM_BINS - 1].
  // Skip bin 0 (DC). Return peakBin * SAMPLE_RATE / FRAME_SAMPLES as a float.
  // If all searched magnitudes are zero, return 0. The viewer displays this
  // result for the mean spectrum of the clip.
  (void)spectrum;
  return 0.0f;  // Replace with your peak search and frequency conversion.
}

void extractLogBandFeatures(float out[FEATURES_PER_FRAME]) {
  // TODO 2.3: YOUR CODE HERE
  // For each band, sum magnitude[k]^2 for bins whose frequencies fall in
  // [BAND_EDGES_HZ[band], BAND_EDGES_HZ[band + 1]). Include 4000 Hz in the
  // final band. A bin's frequency is k * SAMPLE_RATE / FRAME_SAMPLES.
  // Store log1pf(power) in out[band]. Reset power before starting each band.
  for (uint8_t band = 0; band < FEATURES_PER_FRAME; ++band) out[band] = 0.0f;
}
