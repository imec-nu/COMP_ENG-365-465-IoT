Lab 3 Reading and Processing Sensor Data

Use a XIAO ESP32S3 with the Sense expansion board.
Arduino board package: esp32 by Espressif Systems, version 3.3.12.
Board: XIAO_ESP32S3. USB CDC On Boot: Enabled. Serial Monitor: 115200.
Task 4 requires Tools > PSRAM > OPI PSRAM.

Open one sketch at a time:
  snap_light/snap_light.ino       Task 1: sound RMS and LED timing
  touch_stop/touch_stop.ino       Task 2: hardware interrupt and sampling stop
  audio_logger/audio_logger.ino   Task 3: buffer ownership and audio files
  touch_camera/touch_camera.ino   Task 4: one photo per touch and JPEG storage

Keep the matching support header with each .ino file. Complete the TODO blocks
in the .ino files; the support headers contain supplied hardware/helper code.
Follow the Lab 3 student guide for calibration, API hints, tests and checkpoints.

Tasks 2 and 4 use a touch wire on D0 (GPIO1).
Task 2 interrupt threshold is an INCREASE above baseline in core 3.3.12.
Use its calibration mode before choosing the delta threshold.
Tasks 3 and 4 use a FAT32 microSD card in the Sense slot.
Do not control the built-in LED during SD use: both use GPIO21.

To copy Task 3 recordings or Task 4 photos, disconnect USB before removing the
microSD card, then use a card reader. If you do not have a reader or adapter,
leave the card in the Sense slot and use sd_usb_export/sd_usb_export.ino with
tools/sd_download.py. See sd_usb_export/README.txt for macOS and Windows commands.
This supplied utility has no TODOs and is not an additional checkpoint.

The audio converter needs Python 3 and no additional packages:
  python3 tools/txt_to_wav.py recordings --join
Put completed Task 3 TXT files into a recordings folder first. On Windows,
use py instead of python3 if needed. WAV files are written into recordings/wav.
Existing WAV output is preserved; choose a new --output-dir for another conversion.
