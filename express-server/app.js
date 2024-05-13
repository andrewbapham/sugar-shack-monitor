import WebSocket, { WebSocketServer } from "ws";
import express from "express";
import serialport from "serialport-gsm";
import readline from "readline";

/*
 * WEBSOCKET CODE
 * Interacts directly with ESP32 via websocket. Receives threshold reached messages from ESP32.
 */

// Set to desired target phone number before starting server. needs area code (+XXX) followed by 10 digit phone number.
const SMS_TARGET_NUMBER = "+11234567890";

const wss = new WebSocketServer({ port: 8080 });

console.log(`Server started on port 8080, address: ${wss.address()}`);
console.log(wss.address());
wss.on("connection", (ws) => {
  console.log("Client connected");

  ws.on("error", (err) => {
    console.log(`Error: ${err}`);
  });

  ws.on("message", (message) => {
    console.log(`Received message => ${message} at ${new Date()}`);
    console.log(`Threshold Reached!!`);
    modem.sendSMS(SMS_TARGET_NUMBER, "Barrel Threshold Reached!", 0, null);

    //send request to server to send notification to client that threshold exceeded
  });

  ws.on("close", () => {
    console.log("Client disconnected");
  });
});
/*
 * SERIAL PORT CONNECTION CODE
 * Connects to AT modem via serial port
 */

const modem = serialport.Modem();
const options = {
  baudRate: 115200,
  dataBits: 8,
  stopBits: 1,
  parity: "none",
  rtscts: false,
  xon: false,
  xoff: false,
  xany: false,
  autoDeleteOnReceive: true,
  enableConcatenation: true,
  incomingCallIndication: true,
  incomingSMSIndication: true,
  pin: "",
  customInitCommand: "",
  logger: console,
};

serialport.list().then((ports) => {
  console.log("Available serial ports:");
  console.log(ports);
  if (ports.length == 0) {
    throw new Error("No serial devices found!");
  }
});

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
});

const ac = new AbortController();
const signal = ac.signal;

// length of time to wait for user to enter desired serial port before defaulting to first in list
const SP_PROMPT_TIMEOUT = 20_000;

let portName = await new Promise((resolve) => {
  rl.question(
    `Which serial port would you like to use? The first option will be selected by default in ${
      SP_PROMPT_TIMEOUT / 1000
    } seconds.\n`,
    { signal },
    resolve
  );
  signal.addEventListener(
    "abort",
    async () => {
      console.log(
        "Serial port selection timeout expired. Defaulting to first available serial port."
      );
      const availablePorts = await serialport.list();
      resolve(availablePorts[0].path);
    },
    { once: true }
  );
  setTimeout(() => ac.abort(), SP_PROMPT_TIMEOUT);
});

console.log(`opening ${portName}`);
modem.open(portName, options, (err) => {
  if (err) {
    console.log(err);
  } else {
    console.log("Modem opened successfully");
  }
});

modem.on("open", () => {
  console.log("Modem opened");
  modem.setModemMode(() => {}, "SMS");
  modem.initializeModem();
});

/*
 * EXPRESS SERVER CODE
 * Express server running on this machine for control via network HTTP requests
 */

const app = express();
const port = 8000;

app.post("/set-threshold", (req, res) => {
  threshold = req.query.threshold;
  try {
    wss.clients.forEach((client) => {
      client.send(req.query.threshold);
      console.log(req.query.threshold);
    });
  } catch (error) {
    res.send(error);
  }
  res.send("Threshold set");
});

app.get("/acknowledge", (req, res) => {
  try {
    wss.clients.forEach((client) => {
      client.send("ack");
    });
  } catch (error) {
    res.send(error);
  }
  res.send("Sent acknowledge to ESP");
});

app.listen(port, () => {
  console.log(`Server started on port ${port}`);
});
