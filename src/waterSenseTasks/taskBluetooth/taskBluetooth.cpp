#include <Arduino.h>
#include <ArduinoBLE.h>

#include "sharedData.h"
#include "taskBluetooth.h"
#include "waterSenseLibs/bluetooth/bluetooth.h"

void taskBluetooth(void *) {
  const EventBits_t startupBits = EVENT_CLOCK_READY | EVENT_STORAGE_READY;
  while ((xEventGroupGetBits(lifecycleEvents) & startupBits) != startupBits) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST) {
      xEventGroupSetBits(lifecycleEvents, EVENT_BLUETOOTH_STOPPED);
      vTaskSuspend(nullptr);
    }
    reportHeartbeat(TaskId::Bluetooth);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  BLEService service("12345678-1234-5678-1234-56789abcdef0");
  BLEStringCharacteristic request(
      "12345678-1234-5678-1234-56789abcdef2", BLEWrite, 96);
  BLECharacteristic chunk(
      "12345678-1234-5678-1234-56789abcdef3", BLENotify, 110);
  BLEStringCharacteristic checksum(
      "12345678-1234-5678-1234-56789abcdef4", BLERead | BLEWrite, 50);
  BLEStringCharacteristic status(
      "12345678-1234-5678-1234-56789abcdef5", BLENotify, 50);
  BLEStringCharacteristic batteryPercent(
      "12345678-1234-5678-1234-56789abcdef6", BLERead | BLENotify, 20);

  if (!BLE.begin() || !bluetoothFileManager.begin()) {
    signalFatalError("bluetooth", "initialization failed");
    xEventGroupSetBits(lifecycleEvents, EVENT_BLUETOOTH_STOPPED);
    vTaskSuspend(nullptr);
  }

  BLE.setLocalName("WaterSense");
  BLE.setAdvertisedServiceUuid(service.uuid());
  service.addCharacteristic(request);
  service.addCharacteristic(chunk);
  service.addCharacteristic(checksum);
  service.addCharacteristic(status);
  service.addCharacteristic(batteryPercent);
  BLE.addService(service);
  BLE.advertise();

  bool transferring = false;
  bool awaitingChecksum = false;
  uint32_t expectedChecksum = 0;

  while (!(xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST)) {
    BLE.poll();
    BLEDevice central = BLE.central();
    const bool connected = central && central.connected();

    if (!connected) {
      xEventGroupClearBits(lifecycleEvents, EVENT_BLE_CONNECTED);
      transferring = false;
      awaitingChecksum = false;
      bluetoothFileManager.clearFile();
      reportHeartbeat(TaskId::Bluetooth);
      vTaskDelay(pdMS_TO_TICKS(BLE_POLLING_FREQ));
      continue;
    }

    xEventGroupSetBits(lifecycleEvents, EVENT_BLE_CONNECTED);
    const BatterySnapshot battery = getBatterySnapshot();
    if (battery.valid) {
      char value[20];
      snprintf(value, sizeof(value), "%.2f",
               static_cast<double>(battery.percent));
      batteryPercent.writeValue(value);
    }

    if (request.written()) {
      const String requestedFile = request.value();
      transferring = false;
      awaitingChecksum = false;
      bluetoothFileManager.clearFile();

      if (requestedFile == "filelist.txt") {
        if (bluetoothFileManager.generateFileList()) {
          status.writeValue(
              "FILELISTS_MADE " +
              String(bluetoothFileManager.getFileListCount()));
        } else {
          status.writeValue("FILELISTS_FAILED");
        }
      } else if (bluetoothFileManager.loadFile(requestedFile)) {
        expectedChecksum = bluetoothFileManager.getCurrentChecksum();
        transferring = true;
        status.writeValue("TRANSFER_STARTED");
      } else {
        status.writeValue("FILE_LOAD_FAILED");
      }
    }

    if (transferring) {
      uint8_t data[100];
      const size_t length =
          bluetoothFileManager.readChunk(data, sizeof(data));
      if (length > 0) {
        chunk.writeValue(data, length);
      }
      if (bluetoothFileManager.transferFinished()) {
        checksum.writeValue(String(expectedChecksum));
        status.writeValue("TRANSFER_COMPLETE");
        transferring = false;
        awaitingChecksum = true;
      }
    } else if (awaitingChecksum && checksum.written()) {
      char *end = nullptr;
      const String receivedText = checksum.value();
      const uint32_t received =
          strtoul(receivedText.c_str(), &end, 10);
      if (end && *end == '\0' && received == expectedChecksum) {
        status.writeValue("TRANSFER_SUCCESS");
      } else {
        status.writeValue("CHECKSUM_MISMATCH");
      }
      awaitingChecksum = false;
      bluetoothFileManager.clearFile();
    }

    reportHeartbeat(TaskId::Bluetooth);
    vTaskDelay(pdMS_TO_TICKS(BLE_POLLING_FREQ));
  }

  bluetoothFileManager.clearFile();
  xEventGroupClearBits(lifecycleEvents, EVENT_BLE_CONNECTED);
  BLE.stopAdvertise();
  BLE.end();
  xEventGroupSetBits(lifecycleEvents, EVENT_BLUETOOTH_STOPPED);
  reportHeartbeat(TaskId::Bluetooth);
  vTaskSuspend(nullptr);
}
