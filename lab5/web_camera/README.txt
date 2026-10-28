Lab 5 Task 3: web camera

Open web_camera.ino. Set your group Wi-Fi name and password in config.h.
Select XIAO_ESP32S3 and Tools > PSRAM > OPI PSRAM.
The Sense camera plugs into the expansion board; this task needs no jumper wires or SD card.

Student code: TODOs 3.1/3.2 are in web_camera.ino; 3.3/3.4 are in script.h.
Camera setup, network setup, and the webpage layout are provided.

Upload, open Serial Monitor at 115200 baud, and connect your phone to the printed
Wi-Fi network. Open the printed http:// address in a browser. Stay connected if
the phone says that this Wi-Fi network has no internet access.

Tap Capture Photo to preview an image. Change the scene and capture again.
Tap Download JPEG to save the displayed image. The browser may open a download
or save dialog; use it to save the JPEG.

Editing script.h changes JavaScript stored on the board. Upload again and reload
the page after making changes. Keep all files in this sketch folder together.
