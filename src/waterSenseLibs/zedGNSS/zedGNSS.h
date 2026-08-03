#ifndef ZED_GNSS_H
#define ZED_GNSS_H

#include <SparkFun_u-blox_GNSS_v3.h>

#include "sharedData.h"

#define fileBufferSize 20000

struct GnssFix {
  uint32_t unixTime;
  int32_t latitudeE7;
  int32_t longitudeE7;
  int32_t altitudeMslMm;
  bool valid;
};

class GNSS {
 public:
  bool begin();
  bool beginIdle();
  bool poll(GnssFix &fix);
  void drainFullBuffers();
  void flushBuffers();
  bool shutdown();

  uint32_t sfrbxCount() const { return sfrbxCount_; }
  uint32_t rawxCount() const { return rawxCount_; }

 private:
  bool enqueueOneBuffer(bool allowPartial, TickType_t freeBufferWait);

  SFE_UBLOX_GNSS device_;
  bool initialized_ = false;
  uint32_t sfrbxCount_ = 0;
  uint32_t rawxCount_ = 0;
};

#endif
