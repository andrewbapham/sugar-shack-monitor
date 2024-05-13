# Sugar Shack Monitor

## Motivations

This goal of this project is to provide a simple service to allow the monitoring of the liquid levels of a container. Originally, this project was meant to be used in a sugar shack, a building where maple syrup is made. In this sugar shack, tree sap was pumped into a barrel, but there was no system in place to monitor the level of sap in the barrel to ensure it was not overflowing. This project was created to solve this problem.

## Description

This repository contains two parts: Code for the ESP32 microcontroller, meant to be mounted near or on the barrel with a moisture sensor to detect the level of liquid in the barrel, and a web server to make interaction with the microcontroller easier, and handle sending alerts over SMS with a GSM module when the liquid level is too high. More detailed descriptions of what each part does can be found in the READMEs of the respective directories.

## Usage

### Required Parts

- ESP32 microcontroller
- Capacitive Soil Moisture Sensor
- Secondary computer on the same network as the ESP32 to run the web server
- (optional) GSM module with enabled SIM card to send SMS alerts

### Required Software

- Flashing tool for ESP32. I used the ESP-IDF toolchain.
- Node.js for running the web server

### Setup

1. Obtain the IP address of the web server computer
2. In the `esp32/main/main.c` file, change the DEFAULT_SSID, DEFAULT_PWD, and WS_ADDRESS to the desired values.
   - DEFAULT_SSID: The SSID (name) of the network the ESP32 will connect to
   - DEFAULT_PWD: The password of the network the ESP32 will connect to
   - WS_ADDRESS: The address of the websocket running on the web server computer. It should follow the format: `ws://<IP_ADDRESS>:<PORT>`. The default port is 8080, and should be used unless manually configured otherwise in the web server code. The IP_ADDRESS should be the IP address of the web server computer obtained in step 1.
3. in the `web-server` directory, run `npm install` to install the required packages
4. Run the web server code in the `web-server` directory using `node app.js` when inside the directory
5. Build the C source code and flash the ESP32 with the code in the `esp32` directory.

The ESP32 should now connect to the network and begin running.

### Adjusting the Sensitivity

The default sensitivity of the moisture sensor can be adjusted by changing the `sensor_threshold` value in the `esp32/main/main.c` file. This value is the threshold at which the ESP32 will send a message to the web server indicating that the liquid level is too high. The value should be between 0 and 4095, with 0 being the most sensitive and 4095 being the least sensitive. The value can also be changed while the program is running, by sending an HTTP POST request to the `/set-threshold` endpoint of the web server with the new value as a query parameter. The target URL should look like `http://<IP_ADDRESS>:<PORT>/set-threshold?value=<NEW_VALUE>`, where `<IP_ADDRESS>` is the IP address of the web server computer, `<PORT>` is the port the web server is running on (by default it is 8000, unless changed in `/express-server/app.js`), and `<NEW_VALUE>` is the new threshold value.
