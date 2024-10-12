#include "bluetooth_hid.h"

BluetoothHID bluetoothHID;

BluetoothHID::BluetoothHID() : pServer(nullptr), pHIDDevice(nullptr), pInputCharacteristic(nullptr), pOutputCharacteristic(nullptr) {}

void BluetoothHID::setup()
{
    NimBLEDevice::init("ESP32 Media Controller");
    pServer = NimBLEDevice::createServer();
    pHIDDevice = new NimBLEHIDDevice(pServer);

    pInputCharacteristic = pHIDDevice->inputReport(1);   // Report ID 1
    pOutputCharacteristic = pHIDDevice->outputReport(1); // Report ID 1

    pHIDDevice->manufacturer()->setValue("Espressif");
    pHIDDevice->pnp(0x02, 0xe502, 0xa111, 0x0210);
    pHIDDevice->hidInfo(0x00, 0x01);

    const uint8_t reportMap[] = {
        0x05, 0x0C, // Usage Page (Consumer)
        0x09, 0x01, // Usage (Consumer Control)
        0xA1, 0x01, // Collection (Application)
        0x85, 0x01, //   Report ID (1)
        0x15, 0x00, //   Logical Minimum (0)
        0x25, 0x01, //   Logical Maximum (1)
        0x75, 0x01, //   Report Size (1)
        0x95, 0x07, //   Report Count (7)
        0x09, 0xB5, //   Usage (Scan Next Track)
        0x09, 0xB6, //   Usage (Scan Previous Track)
        0x09, 0xB7, //   Usage (Stop)
        0x09, 0xCD, //   Usage (Play/Pause)
        0x09, 0xE2, //   Usage (Mute)
        0x09, 0xE9, //   Usage (Volume Increment)
        0x09, 0xEA, //   Usage (Volume Decrement)
        0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x95, 0x01, //   Report Count (1)
        0x75, 0x01, //   Report Size (1)
        0x81, 0x01, //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xC0        // End Collection
    };

    pHIDDevice->reportMap((uint8_t *)reportMap, sizeof(reportMap));
    pHIDDevice->startServices();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD);
    pAdvertising->addServiceUUID(pHIDDevice->hidService()->getUUID());
    pAdvertising->start();

    Serial.println("Bluetooth HID device advertising started");
}

void BluetoothHID::sendMediaKeyPress(uint8_t key)
{
    uint8_t mediaKeyReport[2] = {key, 0};
    pInputCharacteristic->setValue(mediaKeyReport, 2);
    pInputCharacteristic->notify();

    // Release the key
    mediaKeyReport[0] = 0;
    pInputCharacteristic->setValue(mediaKeyReport, 2);
    pInputCharacteristic->notify();
}

bool BluetoothHID::isConnected()
{
    return pServer->getConnectedCount() > 0;
}
