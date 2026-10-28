#pragma once
#include <pgmspace.h>

const char STYLE_CSS[] PROGMEM = R"LAB5CSS(
* { box-sizing: border-box; }
body { margin: 0; padding: 24px 14px; font-family: system-ui,-apple-system,Arial,sans-serif; background: #f0f4f8; color: #17233b; }
.card { max-width: 720px; margin: auto; padding: 24px; background: #fff; border-radius: 18px; box-shadow: 0 8px 30px #14223a14; }
h1 { margin: 0 0 6px; font-size: 1.65rem; }
.subheading,.hint { color: #5f6b7b; }
.preview { background: #e8edf2; border: 1px solid #d8e0e8; border-radius: 12px; min-height: 240px; display: flex; justify-content: center; align-items: center; overflow: hidden; }
.preview img { display: block; max-width: 100%; height: auto; }
.preview img[hidden] { display: none; }
#placeholder { padding: 20px; text-align: center; color: #667588; }
#status { min-height: 1.5em; margin: 12px 0; font-size: .95rem; }
.actions { display: flex; gap: 12px; flex-wrap: wrap; }
button,.button { flex: 1; min-width: 140px; border: 0; border-radius: 9px; padding: 13px 15px; font: inherit; font-weight: 600; text-decoration: none; text-align: center; cursor: pointer; background: #087a6a; color: white; }
.button { background: #314d6b; }
button:disabled,.button.disabled { opacity: .45; cursor: not-allowed; pointer-events: none; }
.hint { font-size: .85rem; }
)LAB5CSS";
