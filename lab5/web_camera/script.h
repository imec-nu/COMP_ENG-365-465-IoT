#pragma once
#include <pgmspace.h>

// JavaScript runs in the phone's browser. Upload again after editing this file.
const char SCRIPT_JS[] PROGMEM = R"LAB5JS(
const captureBtn = document.getElementById('captureBtn');
const downloadBtn = document.getElementById('downloadBtn');
const photo = document.getElementById('photo');
const placeholder = document.getElementById('placeholder');
const statusText = document.getElementById('status');
let currentPhotoURL = null;

async function requestJpeg(signal) {
  // TODO 3.3: YOUR CODE HERE
  // Save the response from await fetch('/capture', { cache: 'no-store', signal })
  // in response. The supplied signal cancels requests that take too long.
  // If response.ok is false, throw new Error(`HTTP ${response.status}`).
  // Read the body with await response.blob(). Return that Blob.
  throw new Error('Complete TODO 3.3.');
}

// PROVIDED SUPPORT CODE
// Show the received photo and make it available through Download JPEG.
function showPhoto(jpeg, filename) {
  const nextURL = URL.createObjectURL(jpeg);
  const previousURL = currentPhotoURL;
  photo.src = nextURL;
  downloadBtn.href = nextURL;
  downloadBtn.download = filename;
  currentPhotoURL = nextURL;
  if (previousURL) URL.revokeObjectURL(previousURL);
}

// Provided: button events, error messages, and download controls.
let captureInProgress = false;
let activeRequest = null;
captureBtn.addEventListener('click', async () => {
  if (captureInProgress) return;
  captureInProgress = true;
  captureBtn.disabled = true;
  statusText.textContent = 'Capturing photo...';
  const controller = new AbortController();
  activeRequest = controller;
  const timeout = setTimeout(() => controller.abort(), 15000);
  try {
    const jpeg = await requestJpeg(controller.signal);
    if (!(jpeg instanceof Blob) || jpeg.size === 0 || jpeg.type !== 'image/jpeg') {
      throw new Error('The board did not return a JPEG. Check TODOs 3.1-3.3.');
    }
    const filename = `ESP32_photo_${new Date().toISOString().replace(/[:.]/g,'-')}.jpg`;
    showPhoto(jpeg, filename);
    photo.hidden = false;
    placeholder.hidden = true;
    downloadBtn.classList.remove('disabled');
    downloadBtn.removeAttribute('aria-disabled');
    statusText.textContent = `Photo ready (${Math.round(jpeg.size / 1024)} KB). Download is optional.`;
  } catch (error) {
    statusText.textContent = error.name === 'AbortError'
      ? 'The photo timed out. Check the Wi-Fi connection and try again.'
      : `Capture failed: ${error.message}`;
  } finally {
    clearTimeout(timeout);
    activeRequest = null;
    captureInProgress = false;
    captureBtn.disabled = false;
  }
});

window.addEventListener('pagehide', (event) => {
  if (activeRequest) activeRequest.abort();
  if (!event.persisted && currentPhotoURL) URL.revokeObjectURL(currentPhotoURL);
});
)LAB5JS";
