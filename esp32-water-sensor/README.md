# ESP32 Water Sensor

This directory contains the code for the ESP32 microcontroller, meant to be mounted near or on the barrel with a moisture sensor to detect the level of liquid in the barrel.

## How it works

The capacitive moisture sensor is connected to the ESP32 microcontroller. The ESP32 reads the analog value from the senso on an ADC (analog to digital converter) pin, and if the value exceeds the set threshold, it sends a message containing the current sensor value to the web server over a WebSocket connection. The web server then processes the data and sends alerts if the liquid level is too high.

The `main/main.c` file contains the code used to run the ESP32. It includes code to connect to the Wi-Fi and to the WebSocket, as well as handling sending messages to the WebSocket when the sensor value exceeds the threshold, and receiving messages over the WebSocket to change the threshold value. It also contains a system of acknowledgement messages to prevent messages from being sent too frequently. By changing the `SEND_INFO_INTERVAL` and the `RESEND_NO_ACK_INTERVAL` values, the user can control how frequently the messages are sent. By default, it's every 15 minutes if an acknowledgement is sent, and if no acknowledgement is sent, it sends a message every 5 minutes. Currently, this function is not
implemented on the web server side.
