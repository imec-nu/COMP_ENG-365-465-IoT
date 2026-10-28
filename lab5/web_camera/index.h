#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"LAB5HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Lab 5 Camera</title>
  <link rel="stylesheet" href="/style.css">
  <script src="/script.js" defer></script>
</head>
<body>
  <main class="card">
    <h1>ESP32 Camera</h1>
    <p class="subheading">Take a photo with XIAO ESP32S3 Sense.</p>
    <div class="preview">
      <img id="photo" alt="Captured photo" hidden>
      <p id="placeholder">No photo yet. Tap Capture Photo to take one.</p>
    </div>
    <p id="status" role="status" aria-live="polite">Ready.</p>
    <div class="actions">
      <button id="captureBtn" type="button">Capture Photo</button>
      <a id="downloadBtn" class="button disabled" aria-disabled="true">Download JPEG</a>
    </div>
    <p class="hint">The photo stays in this browser until you choose to download it.</p>
  </main>
</body>
</html>
)LAB5HTML";
