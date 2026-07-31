# WaterSense runtime architecture

## Ownership

- `taskClockGNSS2` owns the DS3231 and u-blox receiver.
- `taskRadar` owns the XM125 radar.
- `taskVoltage` owns the MAX17048 fuel gauge.
- `taskSD` owns open measurement and GNSS output files.
- Bluetooth may read the SD card, but every SD operation is serialized by
  `sdMutex`.
- Every complete I2C device operation is serialized by `i2cMutex`.

No task communicates data availability through a Boolean flag.

## Data paths

Radar publishes a complete `MeasurementRecord` to `measurementQueue`. The
record contains the timestamp and battery snapshot captured for that sample, so
storage never assembles a row from unrelated shared values.

GNSS logging uses a fixed pool of `GnssBuffer` objects:

1. GNSS takes a pointer from `gnssFreeQueue`.
2. GNSS fills the buffer and transfers its pointer to `gnssReadyQueue`.
3. Storage writes exactly `buffer->length` bytes.
4. Storage clears the length and returns the pointer to `gnssFreeQueue`.

This ownership cycle provides backpressure and prevents overwrite races without
dynamic allocation.

## Lifecycle

`lifecycleEvents` contains startup, shutdown-request, producer-done, and
task-stopped bits. The power task broadcasts one shutdown request. Producers
stop publishing and acknowledge only after releasing hardware. Storage waits
for both GNSS and radar to stop, drains both queues, syncs and closes its files,
then acknowledges. Deep sleep begins only after all required acknowledgements.

Fatal subsystem failures set both `EVENT_FATAL_ERROR` and
`EVENT_SHUTDOWN_REQUEST`, allowing the same orderly shutdown path to run.

## Lock rules

- Never wait for a queue while holding `i2cMutex` or `sdMutex`.
- Never acquire `i2cMutex` and `sdMutex` at the same time.
- Hold `stateMutex` only long enough to copy or replace one snapshot.
- A mutex timeout must not cause ownership of a queued record or GNSS buffer to
  be discarded.

## Watchdog

Tasks publish `Heartbeat` messages. The watchdog tracks the most recent tick
for each enabled task and ignores tasks only after their lifecycle stopped bit
is set. A missed deadline invokes `esp_restart()` rather than relying on
build-dependent assertions.
