# Lab 6 Voice Control and Speech Recognition

Use the Lab 6 handout for the steps and checkpoints. Complete the TODOs in the three `.ino` files:

- `voice_activity/voice_activity.ino`: detect speech activity and save short clips.
- `audio_features/audio_features.ino`: calculate FFT magnitudes and eight band features.
- `voice_commands/voice_commands.ino`: store voice templates, recognize commands, and control the LED.

Keep each sketch folder intact. `config.h` holds its settings; the support headers provide hardware setup, buffers, and USB communication. The Python tools are complete.

Use XIAO ESP32S3 Sense with the microphone expansion board. No jumper wires, SD card, or Wi-Fi connection are needed. In Arduino IDE, select esp32 3.3.12, XIAO_ESP32S3, USB CDC On Boot Enabled, and OPI PSRAM. Install arduinoFFT 2.0.4 for Tasks 2 and 3.

Use Python 3.9 or later in your chosen environment. If the packages are missing, install them with `pip install -r tools/requirements.txt`. The scripts use pyserial; the feature viewer also uses numpy and matplotlib.

Run commands from this `lab6` folder. The commands use `python`; use `python3` if needed for your environment. Close Arduino Serial Monitor and Plotter before opening the port in Python. Stop Python with Ctrl+C before uploading another sketch.

First, find your board's port:

```sh
python tools/receive_audio.py --list-ports
```

Replace `YOUR_PORT` in the following commands with the port from that list. Upload the matching task before running its tool.

For Task 1, receive speech clips as WAV files:

```sh
python tools/receive_audio.py --port YOUR_PORT --output recordings
```

Files are saved in `lab6/recordings`. Stay quiet during startup calibration. Wait for `[SAVED]` before saying the next sentence.

For Task 2, send the first test tone to the board:

```sh
python tools/feature_viewer.py --port YOUR_PORT --tone test_audio/tone_440hz_10s.wav
```

Then test the other tone:

```sh
python tools/feature_viewer.py --port YOUR_PORT --tone test_audio/tone_1200hz_10s.wav
```

The tool saves spectrum and feature heatmap PNGs and a CSV in `lab6/results`. The expected peaks are 437.5 Hz and 1187.5 Hz. The tone samples travel over USB; you do not need to play them through a speaker.

To analyze microphone recordings instead:

```sh
python tools/feature_viewer.py --port YOUR_PORT --listen
```

For Task 3, start the console:

```sh
python tools/voice_console.py --port YOUR_PORT
```

Wait for each prompt and record three Wake examples, then three Sleep examples. Recognition starts after all six are saved. Starting this tool again begins a fresh enrollment. Templates disappear when the board resets. Results are logged in `lab6/results`.
