# Web Server Code

## How it works

This web server is built using Express, and handles the websocket connection with the ESP32 and communicating with the end user.

The web server listens for messages from the ESP32 on the websocket, and when it receives a message, it sends a text alert using a GSM module. The web server also listens for HTTP requests from the user to change the threshold value of the sensor.
