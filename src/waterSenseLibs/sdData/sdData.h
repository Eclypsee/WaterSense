#ifndef SD_DATA_H
#define SD_DATA_H

#include <SdFat.h>

#include "sharedData.h"

extern SdFat SD;

class SD_Data {
 public:
  explicit SD_Data(gpio_num_t chipSelect,
          gpio_num_t sck,
          gpio_num_t miso,
          gpio_num_t mosi);

  bool begin();
  bool writeHeader();
  bool createDataFile(ExFile &file, uint32_t unixTime);
  bool createGnssFile(ExFile &file, uint32_t unixTime);
  bool writeMeasurement(ExFile &file, const MeasurementRecord &record);
  bool writeGnssData(ExFile &file, const uint8_t *buffer, size_t length);
  bool writeLog(const ClockSnapshot &clock);
  void close(ExFile &file);

 private:
  gpio_num_t chipSelect_;
  gpio_num_t sdck_;
  gpio_num_t sdmiso_;
  gpio_num_t sdmosi_;
  uint16_t dataFileSequence_ = 0;
  uint16_t gnssFileSequence_ = 0;
};

#endif
