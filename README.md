# ESP32-LoRa-Bridge

This repository contains code for the ESP32 microcontroller, functioning as a communication bridge between a server and another controller.

# Key Activities:
Receiving Data: The ESP32 retrieves data packets from the ESP32-Soil-Moisture-Control and forwards them to the server using WiFi.
Server Interaction: It continuously monitors the server for updates. Upon detecting any changes, it promptly sends the relevant data packets back to the ESP32-Soil-Moisture-Control.
This seamless interaction ensures efficient data exchange and control between the systems.
