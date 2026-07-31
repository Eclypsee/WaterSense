#ifndef WATERSENSE_SETUP_H
#define WATERSENSE_SETUP_H

// Optional subsystems and operating modes.
#define GNSS_ON
// #define BLE_on
// #define CONTINUOUS

// Shared I2C bus.
#define SCL 27
#define SDA 26
#define CLK 100000

// Storage.
#define SD_CS GPIO_NUM_5
#define MAX_FILESIZE (50UL * 1024UL)
#define BT_TRANSF_SIZE (64UL * 1024UL)
#define sdWriteSize 8192

// Measurement and sleep cadence.
#define HI_READ (5UL * 60UL)
#define HI_ALLIGN 10
#define GNSS_READ_TIME (8UL * 60UL * 60UL)
#define GNSS_MONTH_SECONDS (30UL * 24UL * 60UL * 60UL)
#define FIX_DELAY (2UL * 60UL)

// Task service periods, expressed in milliseconds.
#define SD_PERIOD 10
#define CLOCK_PERIOD 100
#define VOLTAGE_PERIOD 1000
#define WATCHDOG_PERIOD 100
#define RADAR_TASK_PERIOD 100
#define BLE_POLLING_FREQ 20

// Watchdog and shutdown deadlines.
#define WATCH_TIMER (30UL * 1000UL)
#define SHUTDOWN_TIMEOUT_MS (30UL * 1000UL)

// Fixed-capacity synchronization objects.
#define GNSS_BUFFER_COUNT 3
#define MEASUREMENT_QUEUE_LENGTH 16
#define HEARTBEAT_QUEUE_LENGTH 24

// Bounded resource and hardware waits.
#define STATE_MUTEX_TIMEOUT_MS 250
#define I2C_MUTEX_TIMEOUT_MS 1000
#define SD_MUTEX_TIMEOUT_MS 5000
#define HARDWARE_RETRY_COUNT 5
#define HARDWARE_RETRY_DELAY_MS 1000

#endif
