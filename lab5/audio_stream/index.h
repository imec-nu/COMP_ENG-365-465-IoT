#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTMLEDGE(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Live Audio Monitor</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <main class="panel">
    <h1>ESP32 Audio Monitor</h1>
    <p id="connection" class="status">Connecting…</p>
    <p>Last 2 seconds of microphone audio (16 kHz, mono)</p>
    <canvas id="waveform" width="800" height="260" aria-label="Live audio waveform"></canvas>
    <div class="axis"><span>−2.0 s</span><span>−1.0 s</span><span>Now</span></div>
    <div class="controls">
      <button id="record" disabled>Start Recording</button>
      <a id="download" class="download hidden" href="#">Download MP3</a>
    </div>
    <p id="recordStatus">Not recording</p>
    <p class="hint">Audio is captured by the ESP32, not the phone microphone. Keep this page open while recording. Maximum recording: 60 seconds.</p>
  </main>
  <script src="/lame.min.js"></script>
  <script src="/support.js"></script>
  <script src="/script.js"></script>
</body>
</html>
)HTMLEDGE";
