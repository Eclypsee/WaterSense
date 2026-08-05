#include "sdData.h"

#include <Arduino.h>

SdFat SD;

SD_Data::SD_Data(gpio_num_t chipSelect,
                 gpio_num_t sck,
                 gpio_num_t miso,
                 gpio_num_t mosi)
    : chipSelect_(chipSelect),
      sdck_(sck),
      sdmiso_(miso),
      sdmosi_(mosi) {
}

bool SD_Data::begin() {
  pinMode(chipSelect_, OUTPUT);
  SPI.begin(sdck_, sdmiso_, sdmosi_, chipSelect_);
  SdSpiConfig config(
    chipSelect_,
    SHARED_SPI | USER_SPI_BEGIN,
    SD_SCK_MHZ(10),
    &SPI
  );

  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    if (SD.begin(config)) {
      SD.mkdir("/Data");
      SD.mkdir("/GNSS_Data");
      if (!SD.exists("/GNSS_Data")) {
        if (!SD.mkdir("/GNSS_Data")) {
          Serial.println("[SD] Failed to create /GNSS_Data");
        }
      }
      if (!SD.exists("/Data")) {
        if (!SD.mkdir("/Data")) {
          Serial.println("[SD] Failed to create /Data");
        }
      }
      return true;
    }
    Serial.printf("[SD] Initialization attempt %u failed\n", attempt + 1);
    reportHeartbeat(TaskId::Storage);
    vTaskDelay(pdMS_TO_TICKS(HARDWARE_RETRY_DELAY_MS));
  }
  return false;
}

bool SD_Data::writeHeader() {
  if (SD.exists("/README.txt")) {
    return true;
  }

  ExFile file = SD.open("/README.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (!file) {
    return false;
  }
  file.println("WaterSense data");
  file.println("Measurement CSV columns:");
  file.println("unix_time,distance_mm,battery_voltage,battery_percent");
  file.println("GNSS files contain raw UBX RXM-SFRBX and RXM-RAWX messages.");
  file.close();
  return true;
}

bool SD_Data::createDataFile(ExFile &file, uint32_t unixTime) {
  close(file);
  char path[48];
  snprintf(path, sizeof(path), "/Data/%08lX_%04u.csv",
           static_cast<unsigned long>(unixTime), dataFileSequence_++);
  file = SD.open(path, O_WRITE | O_CREAT | O_TRUNC);
  if (!file) {
    return false;
  }
  file.println("unix_time,distance_mm,battery_voltage,battery_percent");
  return file.sync();
}

bool SD_Data::createGnssFile(ExFile &file, uint32_t unixTime) {
  close(file);
  char path[56];
  snprintf(path, sizeof(path), "/GNSS_Data/%08lX_%04u.ubx", static_cast<unsigned long>(unixTime), gnssFileSequence_++);
  file = SD.open(path, O_WRITE | O_CREAT | O_TRUNC);
  return static_cast<bool>(file);
}

bool SD_Data::writeMeasurement(ExFile &file, const MeasurementRecord &record) {
  if (!file) {
    return false;
  }
  file.printf("%lu,%ld,%.3f,%.2f\n",
              static_cast<unsigned long>(record.unixTime),
              static_cast<long>(record.distanceMm),
              static_cast<double>(record.batteryVoltage),
              static_cast<double>(record.batteryPercent));
  return file.sync();
}

bool SD_Data::writeGnssData(ExFile &file, const uint8_t *buffer,
                            size_t length) {
  if (!file || !buffer || length == 0) {
    return false;
  }
  const size_t written = file.write(buffer, length);
  return written == length && file.sync();
}

bool SD_Data::writeLog(const ClockSnapshot &clock) {
  ExFile log = SD.open("/logFile.csv", O_WRITE | O_CREAT | O_APPEND);
  if (!log) {
    return false;
  }
  if (log.fileSize() == 0) {
    log.println(
        "wake_count,unix_time,latitude_deg,longitude_deg,altitude_msl_m");
  }
  log.printf("%lu,%lu,%.7f,%.7f,%.3f\n",
             static_cast<unsigned long>(wakeCounter),
             static_cast<unsigned long>(clock.unixTime),
             static_cast<double>(clock.latitudeE7) / 10000000.0,
             static_cast<double>(clock.longitudeE7) / 10000000.0,
             static_cast<double>(clock.altitudeMslMm) / 1000.0);
  const bool ok = log.sync();
  log.close();
  return ok;
}

void SD_Data::close(ExFile &file) {
  if (file) {
    file.sync();
    file.close();
  }
}
