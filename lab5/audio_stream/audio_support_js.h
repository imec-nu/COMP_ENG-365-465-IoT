#pragma once
#include <pgmspace.h>

const char AUDIO_SUPPORT_JS[] PROGMEM = R"LAB5JS(
"use strict";

// Page controls, waveform drawing, and MP3 encoding are supplied.
const SAMPLE_RATE = 16000;
const WINDOW_SAMPLES = 2 * SAMPLE_RATE;
const RECORD_LIMIT = 60 * SAMPLE_RATE;
const statusEl = document.getElementById("connection");
const recordingEl = document.getElementById("recordStatus");
const recordBtn = document.getElementById("record");
const downloadLink = document.getElementById("download");
const canvas = document.getElementById("waveform");
const ctx = canvas.getContext("2d");
const circular = new Int16Array(WINDOW_SAMPLES);
let writePos = 0;
let totalReceived = 0;
let ws = null;
let recording = false;
let recordedChunks = [];
let recordedCount = 0;
let lastDownloadUrl = null;
let encoding = false;

function setConnected(ok) {
  statusEl.textContent = ok ? "Connected to ESP32" : "Disconnected — reconnecting…";
  statusEl.classList.toggle("connected", ok);
  recordBtn.disabled = !ok || encoding;
}

function receiveAudio(buffer) {
  if (!(buffer instanceof ArrayBuffer) || buffer.byteLength % 2 !== 0) return;
  const samples = decodeAndStore(buffer);
  if (recording) {
    const keep = Math.min(samples.length, RECORD_LIMIT - recordedCount);
    appendRecording(samples.subarray(0, keep));
    recordingEl.textContent = `Recording: ${(recordedCount / SAMPLE_RATE).toFixed(1)} s`;
    if (recordedCount >= RECORD_LIMIT) stopRecording("60-second limit reached");
  }
}

function connectWebSocket() {
  ws = createAudioSocket();
  if (!ws) {
    statusEl.textContent = "Complete TODO 2.3 to connect audio.";
    recordBtn.disabled = true;
    return;
  }
  ws.onopen = () => {
    circular.fill(0);
    writePos = 0;
    totalReceived = 0;
    setConnected(true);
  };
  ws.onclose = () => {
    setConnected(false);
    if (recording) stopRecording("Connection lost; recording stopped");
    setTimeout(connectWebSocket, 1500);
  };
  ws.onerror = () => ws.close();
  ws.onmessage = event => receiveAudio(event.data);
}

function draw() {
  const w = canvas.width, h = canvas.height, center = h / 2;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = "#d3dee5";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, center); ctx.lineTo(w, center);
  ctx.stroke();
  const available = Math.min(totalReceived, WINDOW_SAMPLES);
  const blankSamples = WINDOW_SAMPLES - available;
  const oldestIndex = (writePos - available + WINDOW_SAMPLES) % WINDOW_SAMPLES;
  ctx.strokeStyle = "#087c70";
  ctx.beginPath();
  for (let x = 0; x < w; x++) {
    const start = Math.max(blankSamples, Math.floor(x * WINDOW_SAMPLES / w));
    const stop = Math.floor((x + 1) * WINDOW_SAMPLES / w);
    let low = 0, high = 0;
    if (start < stop) {
      low = 32767; high = -32768;
      for (let s = start; s < stop; s++) {
        const value = circular[(oldestIndex + s - blankSamples) % WINDOW_SAMPLES];
        low = Math.min(low, value);
        high = Math.max(high, value);
      }
    }
    ctx.moveTo(x, center - (high / 32768) * (center - 8));
    ctx.lineTo(x, center - (low / 32768) * (center - 8));
  }
  ctx.stroke();
  requestAnimationFrame(draw);
}

function showRecordingStarted() {
  if (lastDownloadUrl) URL.revokeObjectURL(lastDownloadUrl);
  lastDownloadUrl = null;
  downloadLink.classList.add("hidden");
  recordBtn.textContent = "Stop Recording";
  recordBtn.classList.add("recording");
  recordingEl.textContent = "Recording: 0.0 s";
}

async function finishRecording(chunks, sampleCount, note) {
  encoding = true;
  recordBtn.disabled = true;
  recordBtn.textContent = "Start Recording";
  recordBtn.classList.remove("recording");
  recordingEl.textContent = note ? `${note}; encoding MP3…` : "Encoding MP3…";
  await new Promise(resolve => setTimeout(resolve, 0));
  try {
    if (sampleCount === 0) {
      recordingEl.textContent = "No samples recorded. Wait for the waveform, then try again.";
      return;
    }
    if (typeof lamejs === "undefined") throw new Error("Reload the page to load the MP3 encoder.");
    const encoder = new lamejs.Mp3Encoder(1, SAMPLE_RATE, 64);
    const mp3Parts = [];
    const frame = new Int16Array(1152);
    let frameFill = 0;
    let samplesSinceYield = 0;
    for (const chunk of chunks) {
      for (const sample of chunk) {
        frame[frameFill++] = sample;
        if (frameFill === frame.length) {
          const out = encoder.encodeBuffer(frame);
          if (out.length) mp3Parts.push(new Uint8Array(out));
          frameFill = 0;
        }
      }
      samplesSinceYield += chunk.length;
      if (samplesSinceYield >= SAMPLE_RATE) {
        // Let the page receive audio and redraw while a long recording encodes.
        await new Promise(resolve => setTimeout(resolve, 0));
        samplesSinceYield = 0;
      }
    }
    if (frameFill > 0) {
      const out = encoder.encodeBuffer(frame.subarray(0, frameFill));
      if (out.length) mp3Parts.push(new Uint8Array(out));
    }
    const tail = encoder.flush();
    if (tail.length) mp3Parts.push(new Uint8Array(tail));
    const blob = new Blob(mp3Parts, { type: "audio/mpeg" });
    lastDownloadUrl = URL.createObjectURL(blob);
    downloadLink.href = lastDownloadUrl;
    downloadLink.download = `esp32_audio_${Date.now()}.mp3`;
    downloadLink.classList.remove("hidden");
    recordingEl.textContent = `Saved ${(sampleCount / SAMPLE_RATE).toFixed(1)} s (${Math.round(blob.size / 1024)} KB). Tap Download MP3.`;
  } catch (error) {
    recordingEl.textContent = `MP3 encoding failed: ${error.message}`;
  } finally {
    encoding = false;
    recordBtn.disabled = !(ws && ws.readyState === WebSocket.OPEN);
  }
}

function startPage() {
  recordBtn.addEventListener("click", () => recording ? stopRecording() : startRecording());
  window.addEventListener("beforeunload", () => {
    if (lastDownloadUrl) URL.revokeObjectURL(lastDownloadUrl);
  });
  connectWebSocket();
  requestAnimationFrame(draw);
}
)LAB5JS";
