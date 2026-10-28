# Lab 5 Interfacing with a Smartphone

Use the Lab 5 student handout for the steps and checkpoints.

Open one sketch at a time in Arduino IDE:

- `web_led/web_led.ino`: control the LED from a phone webpage.
- `audio_stream/audio_stream.ino`: stream microphone samples and record an MP3 in the browser. Also edit the TODOs in `script.h`.
- `web_camera/web_camera.ino`: capture a JPEG, show it, and download it. Also edit the TODOs in `script.h`.

Keep each sketch folder intact. The support headers, webpage styling, camera pin map, and MP3 encoder are supplied. The unfinished sketches compile; complete their TODOs to make the tasks work.

Use ESP32 board package 3.3.12, board XIAO_ESP32S3, USB CDC On Boot enabled, and OPI PSRAM enabled. Task 2 also needs WebSockets by Markus Sattler 2.7.2. Use 115200 baud in Serial Monitor.

Each sketch creates its own Wi-Fi network. Set your group's name and password in its `config.h`, upload, then connect your phone to that network. Open the `http://` URL printed in Serial Monitor. Stay connected when your phone reports that the network has no internet.

JavaScript inside `script.h` runs in the browser, but Arduino uploads it with the sketch. Upload again after editing, then reload the webpage.
