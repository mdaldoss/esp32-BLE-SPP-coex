/*
Author: Marco Daldoss 2021

# What is it 
  A simple example showing how to instantiate the ESP32 in order to expose:
   - one BT Classic SPP (Serial Port Profile) full duplex channel
   - one BLE Server with few characteristics
 
# How 
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

  Author: Marco Daldoss 
  Based on work from Neil Kolban, Evandro Copercini and chegewara
    Ref: 
    - https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
   
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include"esp_gap_bt_api.h"
#include "esp_err.h"

// Bluetooth definition Undboning 
#define REMOVE_BONDED_DEVICES 1   // <- Set to 0 to view all bonded devices addresses, set to 1 to remove
#define PAIR_MAX_DEVICES 20
#define RUN_BLE 1

uint8_t pairedDeviceBtAddr[PAIR_MAX_DEVICES][6];
char bda_str[18];

BluetoothSerial SerialBT;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;


/****  User definitions *************/

// If you have any Bluetooth PIN to add
const char* BT_PIN = "7290";

// Target SPP MAC address
uint8_t targetAddress[6]  = {0x00, 0x07, 0x80, 0x59, 0x94, 0x87};
//String targetName = "myAmazingBTDeviceName";

// Here put your data to send through SPP
uint8_t data_to_send[] = {0x1,0x42}; 



bool initBluetooth()
{
  if(!btStart()) {
    Serial.println("Failed to initialize controller");
    return false;
  }
 
  if(esp_bluedroid_init() != ESP_OK) {
    Serial.println("Failed to initialize bluedroid");
    return false;
  }
 
  if(esp_bluedroid_enable() != ESP_OK) {
    Serial.println("Failed to enable bluedroid");
    return false;
  }
  return true;
}

char *bda2str(const uint8_t* bda, char *str, size_t size)
{
  if (bda == NULL || str == NULL || size < 18) {
    return NULL;
  }
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

// Function that remove previously bounded devices
void unBoundBTDevices(){
   initBluetooth();
  Serial.print("ESP32 bluetooth address: "); Serial.println(bda2str(esp_bt_dev_get_address(), bda_str, 18));
  // Get the numbers of bonded/paired devices in the BT module
  int count = esp_bt_gap_get_bond_device_num();
  if(!count) {
    Serial.println("No bonded device found.");
  } else {
    Serial.print("Bonded device count: "); Serial.println(count);
    if(PAIR_MAX_DEVICES < count) {
      count = PAIR_MAX_DEVICES; 
      Serial.print("Reset bonded device count: "); Serial.println(count);
    }
    esp_err_t tError =  esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);
    if(ESP_OK == tError) {
      for(int i = 0; i < count; i++) {
        Serial.print("Found bonded device # "); Serial.print(i); Serial.print(" -> ");
        Serial.println(bda2str(pairedDeviceBtAddr[i], bda_str, 18));     
        if(REMOVE_BONDED_DEVICES) {
          esp_err_t tError = esp_bt_gap_remove_bond_device(pairedDeviceBtAddr[i]);
          if(ESP_OK == tError) {
            Serial.print("Removed bonded device # "); 
          } else {
            Serial.print("Failed to remove bonded device # ");
          }
          Serial.println(i);
        }
      }        
    }
  }
}

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};
/* ----- end Bluetooth utilities ------ */


/*----- Utilities ----- */
// CRC-8 Algorithm. Not needed by useful 
// Dallas/Maxim GNU GPL 3.0
byte CRC8(const byte *data, byte len) {
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}

// Routine to send data to SPP
void sendDataPacket(uint8_t * data_to_send){
    // Sending data
    for (int i=0;i<(sizeof(data_to_send)/sizeof(uint8_t));i++){
      SerialBT.write(data_to_send[i]);
    }
}

void setup() {
  Serial.begin(115200);
  unBoundBTDevices();
  SerialBT.begin("Host_SPP_Marco", true); 
  SerialBT.setPin(BT_PIN);
  // Create the BLE Device
  BLEDevice::init("Host_BLE_Marco");

  if(RUN_BLE){
      // Create the BLE Server
      pServer = BLEDevice::createServer();
      pServer->setCallbacks(new MyServerCallbacks());
    
      // Create the BLE Service
      BLEService *pService = pServer->createService(SERVICE_UUID);
    
      // Create a BLE Characteristic
      pCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID,
                          BLECharacteristic::PROPERTY_READ   |
                          BLECharacteristic::PROPERTY_WRITE  |
                          BLECharacteristic::PROPERTY_NOTIFY |
                          BLECharacteristic::PROPERTY_INDICATE
                        );
    
      // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
      // Create a BLE Descriptor
      pCharacteristic->addDescriptor(new BLE2902());
    
      // Start the service
      pService->start();
    
      // Start advertising
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      pAdvertising->addServiceUUID(SERVICE_UUID);
      pAdvertising->setScanResponse(false);
      pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
      BLEDevice::startAdvertising();
      Serial.println("Waiting a client connection to notify...");
  }  

    
  Serial.println("Spp Connecting...");

  bool connected = SerialBT.connect(targetAddress);
  uint8_t i=10;
  while(i-- and not connected){   
    Serial.println("Error,tring reconnecting... "+String(i));
    connected = SerialBT.connect(targetAddress); 
  }
  if(connected){
    Serial.println("Connected Succesfully!");
  }
}


int prev = millis();
int tdelay = millis()-prev;

void loop() {
  if(RUN_BLE){
    // notify changed value
    if (deviceConnected) {
        pCharacteristic->setValue((uint8_t*)&value, 4);
        pCharacteristic->notify();
        value++;
        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on a BLE connection
        oldDeviceConnected = deviceConnected;
      }
    }


    delay(100); // just some delay

    // Transmit data
    prev = micros();
    sendDataPacket(data_to_send);
    tdelay = micros()-prev;
    Serial.print("SPP Trasmission done in [us]: ");
    Serial.println(tdelay);
    
    // Print Received data
    prev = micros();
    while(SerialBT.available()) {
        Serial.write(SerialBT.read());
        delay(5);
     }
    tdelay = micros()-prev;
    Serial.println();
    Serial.print("SPP Reception done in [us]: ");
    Serial.println(tdelay);

}
