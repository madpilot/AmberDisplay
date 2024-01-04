# Amber Display

Arduino project to show the current Amber Price on a TTGO T-Display.

## Requirements

- Arduino 1.8.x IDE
- [Button2](https://github.com/LennartHennigs/Button2) library by Lennart Hennings
- [HttpClient](http://github.com/amcewen/HttpClient) library by Adrian McEwen
- [ArduinoJson](https://arduinojson.org/) library by Beniot Elanchon

## Setup

### Generating an Amber API key

1. Navigate to http://app.amber.com.au/developers/
2. Click "Generate a new Token"
3. Enter a name for the token
4. Copy the generated token and save it somewhere for later.

### Find your site id

1. Click the "Authorize" button
2. Enter the API key
3. Click "Authorize"
4. Click "Close"
5. Click the "Sites" tab
6. Click "Try it out"
7. Click "Execute"
8. Copy the `id` from the site that has a status of `active` and save it somewhere for later

### Setting up the project

1. Download the zip file from Github
2. Copy `config.h.sample` to `config.h`
3. Enter your WiFi SSID and Passkey
4. Enter the API key you generated
5. Enter the Site ID you copied.

### Building the project

1. Download the project from Github
1. Open the project in the Arduino IDE
1. Add the ESP32 board definitions. Search for "ESP32" and install "esp32" from Espessif.
1. Select the "TTGO T1" board
1. Select the "Minimal SPIFFS" partition scheme
1. Click Upload
