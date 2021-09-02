# ESP32: BLE Server and Bluetooth Classic (SPP) coexistence 

This is an example showing how to instantiate an ESP32 to offer at the same time a BLE Server and a Bluetooth classic SPP (Serial Port Profile).
Marco Daldoss 2021

 
## What is it 
  A simple example showing how to instantiate the ESP32 in order to expose:
   - one BT Classic SPP (Serial Port Profile) full duplex channel
   - one BLE Server with few characteristics
 
## How 
  - The code start by removing previous paired devices -> this has shown more stability while starting a connection   
  - Instantiate SPP profile

  - Create a BLE server that, once we receive a connection, will send periodic notifications.
    The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
    And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

    The design of the cration of the BLE server is:
    1. Create a BLE Server
    2. Create a BLE Service
    3. Create a BLE Characteristic on the Service
    4. Create a BLE Descriptor on the characteristic
    5. Start the service.
    6. Start advertising.

    A connect hander associated with the server starts a background task that performs notification
    every couple of seconds.

  - Sample data is sent to SPP port and measured trasmission, wait and reception time

## About 
**Author:**   [Marco Daldoss](https://www.linkedin.com/in/marcodaldoss/) 
  
Based on work from Neil Kolban, Evandro Copercini and chegewara
Ref: 
 https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
   
