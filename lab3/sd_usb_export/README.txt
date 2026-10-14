Copy files from the Sense microSD card

You can disconnect USB, remove the card, and copy files with a microSD card
reader or adapter. If you do not have one, use the USB method below. It copies
the files through the XIAO's existing USB cable; no extra wiring is needed.

The same Arduino sketch and Python script work on macOS and Windows. This is
a supplied utility for Tasks 3 and 4, with no TODOs or additional checkpoint.

1. Upload the export sketch

Keep the microSD card in the Sense slot. For Task 3, note which recordings
were reported as complete (SAVED) before stopping the recorder. Uploading a
different sketch interrupts recording, so the last TXT file may be partial.

Open sd_usb_export/sd_usb_export.ino in Arduino IDE. Use:
  Board: XIAO_ESP32S3
  esp32 by Espressif Systems: 3.3.12
  USB CDC On Boot: Enabled
  USB Mode: USB-OTG (TinyUSB)
  Core Debug Level: None

This utility does not require PSRAM; you can leave its current setting.
Upload, then close Serial Monitor and Serial Plotter. The export sketch is
silent while it waits for a Python command.

Uploading replaces the running recorder or camera program. It does not erase
the SD card. After copying your files, upload the task sketch again to resume
recording or taking photos.

2. Check the Python dependency

Use Python 3.9 or newer. Open Terminal on macOS or PowerShell on Windows, and
change to your downloaded lab3 folder. All commands below run from lab3.
If you used the course download instructions, run:
  cd ~/iot-labs/COMP_ENG-365-465-IoT/lab3

Run the dependency check for your operating system, then read its output.

macOS Terminal:
  python3 -m pip show pyserial

Windows PowerShell:
  py -m pip show pyserial

If the output shows Name: pyserial and Version: 3.5, skip installation and use
that same Python command for downloading. An existing environment, including
a Conda environment, is fine if this check finds version 3.5.

If pyserial is missing or has a different version, create a local environment
inside lab3. Run only the command for your operating system:

macOS Terminal:
  python3 -m venv .venv

Windows PowerShell:
  py -m venv .venv

After that command finishes, install the dependency in the new environment.

macOS Terminal:
  .venv/bin/python -m pip install -r tools/requirements.txt

Windows PowerShell:
  .\.venv\Scripts\python.exe -m pip install -r tools/requirements.txt

No activation command is needed. If you created this environment, use its
Python executable for every downloader command below: .venv/bin/python on
macOS or .\.venv\Scripts\python.exe on Windows.

If your Windows installation uses python instead of py, replace py with
python throughout these instructions.

3. Find the XIAO port

The commands below use python3 on macOS and py on Windows. If you installed
into .venv, replace that first word with .venv/bin/python on macOS or
.\.venv\Scripts\python.exe on Windows. Keep using that same interpreter.

Run the ports command for your operating system. It lists serial ports on
your computer; it does not list or download SD files.

macOS Terminal:
  python3 tools/sd_download.py ports

Windows PowerShell:
  py tools/sd_download.py ports

Stop here and identify the port belonging to your XIAO. Read the device
descriptions beside the port names. If the description is unclear, disconnect
the XIAO and run ports again. Reconnect it and run ports once more. The port
that appears when the board is connected is the one to use.

Copy its complete name: on macOS this is the full /dev/cu... path; on Windows
it is COM followed by a number. Port names can change after an upload.

4. List the SD files

Replace YOUR_PORT in the command below with the exact port name you just
found. YOUR_PORT is a placeholder, not a port to type literally.

Run list to see what is on the SD card. It prints each file's size in bytes
and its path on the card. It does not download anything.

macOS Terminal:
  python3 tools/sd_download.py --port YOUR_PORT list

Windows PowerShell:
  py tools/sd_download.py --port YOUR_PORT list

Read the list and check that your recordings or photos are present before
continuing. If the command reports an error, use the notes at the end.

5. Download the files

Use the same port and Python interpreter as in the list command. Replace
YOUR_PORT before running pull. This command copies the non-hidden SD files
into a folder named sd_download inside your current lab3 folder.

macOS Terminal:
  python3 tools/sd_download.py --port YOUR_PORT pull --output sd_download

Windows PowerShell:
  py tools/sd_download.py --port YOUR_PORT pull --output sd_download

The files appear in lab3/sd_download, with their SD folders preserved. For
example, /rawtxt/rec_000000.txt becomes sd_download/rawtxt/rec_000000.txt.
Photos such as /image_0001.jpg appear directly inside sd_download.

Use an output folder on your computer's local disk. If you download again,
choose a new folder, such as --output sd_download_2. The script stops if a
destination file already exists. A completed download reports CRC32 verified.

list and pull skip files and folders whose names start with a dot, including
everything inside a hidden folder. To include those files, add
--include-hidden to the list or pull command above.

6. Use the downloaded files

Task 3: copy two adjacent, completed TXT recordings from sd_download/rawtxt
into a recordings folder inside lab3. Convert them as described in the lab:
  macOS:   python3 tools/txt_to_wav.py recordings --join
  Windows: py tools/txt_to_wav.py recordings --join

Task 4: open the downloaded JPG files on your computer and check the photos.

The download script copies file contents without changing their format. It
does not convert TXT to WAV or delete anything from the card.

If something goes wrong

- Port not found: run ports again and use the current name.
- Port busy or access denied: close Serial Monitor, Serial Plotter, and any
  other program using the port.
- SD_UNAVAILABLE: check that the FAT32 card is seated in the Sense slot.
  Disconnect USB before removing or reinserting it.
- Timeout or CRC error: press Reset and try again with a new output folder.
- Audio readings instead of file responses: upload sd_usb_export.ino first.
