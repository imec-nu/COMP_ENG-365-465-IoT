#pragma once

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>ESP32 LED Control</title>

  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #f4f6f8;
      padding: 30px;
    }

    .container {
      background-color: white;
      padding: 25px;
      border-radius: 15px;
      max-width: 350px;
      margin: auto;
      box-shadow: 0 3px 10px #ccc;
    }

    button {
      font-size: 18px;
      padding: 15px 30px;
      margin: 10px;
      border: none;
      border-radius: 8px;
      color: white;
      cursor: pointer;
    }

    .on {
      background-color: #28a745;
    }

    .off {
      background-color: #dc3545;
    }
  </style>
</head>

<body>

  <div class="container">

    <h2>ESP32 LED Control</h2>

    <p>Control the ESP32 built-in LED</p>

    <h3>LED Status: %LED_STATUS%</h3>

    <form action="/on" method="GET">
      <button class="on" type="submit">
        LED ON
      </button>
    </form>

    <form action="/off" method="GET">
      <button class="off" type="submit">
        LED OFF
      </button>
    </form>

  </div>

</body>
</html>
)HTML";
