#pragma once
#include <pgmspace.h>

const char STYLE_CSS[] PROGMEM = R"CSSEDGE(
* { box-sizing: border-box; }
body { margin: 0; padding: 20px; background: #edf2f6; color: #172332; font-family: system-ui, Arial, sans-serif; }
.panel { max-width: 780px; margin: 0 auto; background: white; padding: 24px; border-radius: 16px; box-shadow: 0 4px 16px #0001; }
h1 { margin: 0 0 10px; font-size: 1.6rem; }
.status { font-weight: bold; color: #9b5520; }
.status.connected { color: #09774c; }
canvas { display: block; width: 100%; height: auto; background: #f8fafb; border: 1px solid #d4dfe8; border-radius: 8px; }
.axis { display: flex; justify-content: space-between; color: #566575; font-size: 0.85rem; margin-top: 6px; }
.controls { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 24px; }
button, .download { border: 0; border-radius: 8px; padding: 13px 18px; background: #087c70; color: white; font-size: 1rem; text-decoration: none; cursor: pointer; }
button.recording { background: #b91c1c; }
button:disabled { background: #9ca3af; cursor: not-allowed; }
.download { background: #334e8a; }
.hidden { display: none; }
.hint { color: #5e6c7c; font-size: 0.86rem; line-height: 1.5; }
)CSSEDGE";
