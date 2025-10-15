# STM32-Based Infusion Pump with 5G Connectivity

## Overview
This project implements an **infusion pump** system using an STM32 microcontroller. The pump monitors and transmits patient infusion data to a remote server over a **5G modem** connection. 

The firmware is built with **HAL libraries** and manages multiple UART interfaces for communication between the pump, terminal, and modem.

## Features
- **STM32 MCU control**: Manages infusion pump hardware and UART peripherals.
- **5G modem integration**: Sends infusion data to a remote FHIR server endpoint (`HTTP POST/PUT` requests).
- **Real-time UART processing**: Handles asynchronous data from pump, terminal, and modem.
- **Pump data conversion**: Converts raw infusion pump data to hexadecimal string for server transmission.
- **State-machine-based modem control**: Manages AT commands for initialization, network check, HTTP operations, and error handling.

## Endpoints
- FHIR Server: `http://87.121.228.83:1888/fhir/Observation/216`  
  Data is sent as JSON with hexadecimal infusion values.
