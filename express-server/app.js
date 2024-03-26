import WebSocket, { WebSocketServer } from "ws";

let threshold = 1700;

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
    //send request to server to send notification to client that threshold exceeded
  });

  ws.on("close", () => {
    console.log("Client disconnected");
  });
});

import express from "express";
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
