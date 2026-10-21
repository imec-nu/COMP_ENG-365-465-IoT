Lab 4 quick start

Use the revised DOCX handout for the full instructions and submission details.
Complete the marked TODOs to make each task work.

Setup
- Select XIAO_ESP32S3 with Espressif Arduino core 3.3.12. Enable USB CDC On
  Boot and use Serial Monitor at 115200 baud. Open one sketch at a time;
  keep each .ino in its matching folder with all supplied .h files.
- For Tasks 3/4, install ArduinoJson 7.4.3 and U8g2 2.36.19 from Library
  Manager. Use the OLED wiring from the handout.
- Use Python 3.9+ and install bleak==1.1.1 in your chosen environment.
  Task 2 uses the standard library.
- Keep *_support.h files as provided. Configuration values are setup work,
  separate from the programming exercises listed below.

Files to edit
Task 1: ble_link/ble_link.ino (1.1, 1.2), ble_link/ble_logic.h (1.3),
        tools/ble_client.py (1.4, 1.5).
Task 2: udp_sender/udp_sender.ino (2.1), tools/udp_receiver.py (2.2, 2.3).
Task 3: weather_station/weather_station.ino (3.1, 3.4),
        weather_station/weather_logic.h (3.2, 3.3).
Task 4: weather_station/weather_station.ino (4.1, 4.2).

Configuration
- ble_link/config.h: replace BOARD_NAME with your own short ASCII name,
  such as XIAO-L4-yourid with your actual ID. Use that exact name below.
- Task 2: enable your personal phone hotspot in 2.4 GHz or compatibility
  mode, then connect both the computer and board to it. In udp_sender/config.h,
  enter the hotspot SSID/password and COMPUTER_IPV4. Check the computer's
  Wi-Fi IPv4 AFTER joining the hotspot; recheck it whenever the network
  changes. Keep port 4210 unless changed on both ends. The computer address
  is not 127.0.0.1 or a BLE identifier.
- weather_station/config.h: fill in Wi-Fi credentials and your WeatherAPI
  key. Keep ENABLE_NOTIFICATIONS=false for Task 3. For Task 4, choose a
  unique hard-to-guess NTFY_TOPIC, subscribe at https://ntfy.sh/<your-topic>,
  and then enable notifications. Public topics are not private.
- Replace passwords, API keys, and personal topic names with placeholders
  before submitting or sharing your files.

Run the Python tools
Use the same terminal/environment where you installed Bleak, with this lab4
folder as the current directory. Run the command for the current task:
  python tools/ble_client.py --name YOUR_BOARD_NAME
  python tools/udp_receiver.py --esp-ip YOUR_BOARD_IPV4

Replace YOUR_BOARD_NAME with the exact BOARD_NAME in ble_link/config.h.
Replace YOUR_BOARD_IPV4 with the board's Wi-Fi IPv4 from Serial Monitor.
Use python3 instead if that is your Python command.
Allow Bluetooth access for Task 1 and incoming UDP port 4210 for Task 2.
If the board restarts or the BLE link disconnects, run the client again.
The hotspot must allow device-to-device traffic. If it is unavailable or
blocks that traffic, ask the TA/PM for an alternative.

Checkpoints
1. Demonstrate ten matched BLE exchanges. Record the mean RTT from the
   board's Serial Monitor; the duration of a PC write is not board RTT.
2. Show ten consecutive UDP packets and the average of their nine arrival
   intervals. Demonstrate that a sequence break restarts the run.
3. Show approximate location in Serial and current weather on Serial and
   the OLED.
4. Show the fresh weather message arriving at your subscribed ntfy topic.
Submit the files requested in the handout.
