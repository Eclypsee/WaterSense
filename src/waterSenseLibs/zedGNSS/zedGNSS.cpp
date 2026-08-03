#include "zedGNSS.h"

#include <Wire.h>

namespace {
uint32_t callbackSfrbxCount = 0;
uint32_t callbackRawxCount = 0;

void onSfrbx(UBX_RXM_SFRBX_data_t *) {
  ++callbackSfrbxCount;
}

void onRawx(UBX_RXM_RAWX_data_t *) {
  ++callbackRawxCount;
}
}  // namespace

bool GNSS::beginIdle(){
  device_.setFileBufferSize(fileBufferSize);
  bool connected = false;
  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      connected = device_.begin(Wire, 0x42);
      xSemaphoreGive(i2cMutex);
    }
    if (connected) {
      Serial.printf("[GNSS] Detected on attempt %u\n", attempt + 1);
      break;
    }
    Serial.printf("[GNSS] Detection attempt %u failed\n", attempt + 1);
    vTaskDelay(pdMS_TO_TICKS(HARDWARE_RETRY_DELAY_MS));
  }
  if (!connected)return false;

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) != pdTRUE)return false;
  device_.logRXMSFRBX(false);
  device_.logRXMRAWX(false);
  device_.softwareEnableGNSS(false);
  xSemaphoreGive(i2cMutex);
  return true;
}

bool GNSS::begin() {
  device_.setFileBufferSize(fileBufferSize);

  bool connected = false;
  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      connected = device_.begin(Wire, 0x42);
      xSemaphoreGive(i2cMutex);
    }
    if (connected) {
      Serial.printf("[GNSS] Detected on attempt %u\n", attempt + 1);
      break;
    }
    Serial.printf("[GNSS] Detection attempt %u failed\n", attempt + 1);
    vTaskDelay(pdMS_TO_TICKS(HARDWARE_RETRY_DELAY_MS));
  }
  if (!connected) {
    return false;
  }

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    return false;
  }

  // Configure once per power cycle. Do not write receiver flash from the fix
  // acquisition loop.
  const bool outputOk = device_.setI2COutput(COM_TYPE_UBX, VAL_LAYER_RAM_BBR);

  const bool frequencyOk = device_.setNavigationFrequency(1, VAL_LAYER_RAM_BBR);

  const bool sfrbxOk = device_.setAutoRXMSFRBXcallbackPtr(&onSfrbx, VAL_LAYER_RAM_BBR);

  const bool rawxOk = device_.setAutoRXMRAWXcallbackPtr(&onRawx, VAL_LAYER_RAM_BBR);

  const bool configured = outputOk && frequencyOk && sfrbxOk && rawxOk;

  Serial.printf( "[GNSS] Configuration: output=%d frequency=%d SFRBX=%d RAWX=%d\n",outputOk,frequencyOk,sfrbxOk,rawxOk);
  device_.logRXMSFRBX(true);
  device_.logRXMRAWX(true);
  xSemaphoreGive(i2cMutex);

  initialized_ = configured;
  if (configured) {
    Serial.println("[GNSS] Receiver configured successfully");
  }
  if (!configured) {
    Serial.println("[GNSS] Receiver configuration failed");
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      device_.powerOff(0);
      device_.end();
      xSemaphoreGive(i2cMutex);
    }
  }
  return configured;
}

bool GNSS::poll(GnssFix &fix) {
  if (!initialized_ ||
      xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) !=
          pdTRUE) {
    return false;
  }

  const bool communicationOk = device_.checkUblox();
  const bool valid = device_.getGnssFixOk() && device_.getTimeValid() && device_.getDateValid();
  fix.unixTime = device_.getUnixEpoch();
  fix.latitudeE7 = device_.getHighResLatitude();
  fix.longitudeE7 = device_.getHighResLongitude();
  fix.altitudeMslMm = device_.getAltitudeMSL();
  fix.valid = valid;
  sfrbxCount_ = callbackSfrbxCount;
  rawxCount_ = callbackRawxCount;
  xSemaphoreGive(i2cMutex);
  return communicationOk;
}

bool GNSS::enqueueOneBuffer(bool allowPartial, TickType_t freeBufferWait) {
  GnssBuffer *buffer = nullptr;
  if (xQueueReceive(gnssFreeQueue, &buffer, freeBufferWait) != pdTRUE) {
    return false;
  }

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    xQueueSendToFront(gnssFreeQueue, &buffer, 0);
    return false;
  }

  device_.checkUblox();
  const size_t available = device_.fileBufferAvailable();
  size_t bytesToExtract = 0;
  if (available >= sdWriteSize) {
    bytesToExtract = sdWriteSize;
  } else if (allowPartial && available > 0) {
    bytesToExtract = available;
  }

  if (bytesToExtract > 0) {
    device_.extractFileBufferData(buffer->data, bytesToExtract);
  }
  xSemaphoreGive(i2cMutex);

  if (bytesToExtract == 0) {
    buffer->length = 0;
    xQueueSendToFront(gnssFreeQueue, &buffer, 0);
    return false;
  }

  buffer->length = bytesToExtract;
  if (xQueueSend(gnssReadyQueue, &buffer, pdMS_TO_TICKS(1000)) != pdTRUE) {
    signalFatalError("GNSS", "ready-buffer queue invariant violated");
    xQueueSend(gnssFreeQueue, &buffer, 0);
    return false;
  }
  return true;
}

void GNSS::drainFullBuffers() {
  while (enqueueOneBuffer(false, 0)) {
  }
}

void GNSS::flushBuffers() {
  // Storage continues consuming until EVENT_GNSS_DONE is set, so waiting for a
  // free buffer here provides backpressure without losing the final partial
  // block.
  while (enqueueOneBuffer(true, portMAX_DELAY)) {
  }
}

bool GNSS::shutdown() {
  if (!initialized_) {
    return true;
  }

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    return false;
  }
  const bool poweredOff = device_.powerOff(0);
  device_.end();
  xSemaphoreGive(i2cMutex);
  initialized_ = false;
  if(poweredOff){Serial.println("[GNSS] Receiver shut down successfully");}else{Serial.println("[GNSS] Receiver failed to shut down");}
  return poweredOff;
}
