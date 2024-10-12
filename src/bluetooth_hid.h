#ifndef BLUETOOTH_HID_H
#define BLUETOOTH_HID_H

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLECharacteristic.h>

class BluetoothHID
{
public:
    BluetoothHID();
    void setup();
    void sendMediaKeyPress(uint8_t key);
    bool isConnected();

private:
    NimBLEServer *pServer;
    NimBLEHIDDevice *pHIDDevice;
    NimBLECharacteristic *pInputCharacteristic;
    NimBLECharacteristic *pOutputCharacteristic;
};

extern BluetoothHID bluetoothHID;

#endif // BLUETOOTH_HID_H
