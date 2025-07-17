# WaterSense

A multi-sensor water monitoring system built on ESP32 using FreeRTOS tasks for concurrent sensor management and data logging.

## Overview

WaterSense is an embedded water monitoring solution that measures water levels using ultrasonic/radar sensors, environmental conditions, and GPS/GNSS positioning. Data is logged to SD card with intelligent power management for long-term deployment.

**Features:**
- 🌊 Water level measurement (ultrasonic or radar)
- 🌡️ Temperature and humidity monitoring  
- 📍 GPS positioning and timing
- 💾 SD card data logging
- 🔋 Smart power management with sleep modes
- ⚡ Solar panel voltage monitoring
- 🛡️ Watchdog system for reliability

## Hardware Platform

- **MCU:** ESP32
- **Framework:** Arduino/PlatformIO
- **Sensors:** Maxbotix sonar, SHT31 temp/humidity, u-blox GNSS
- **Storage:** SD card
- **Power:** Solar panel with battery backup

## Quick Start

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- ESP32 development board
- Connected sensors per hardware configuration

### Build & Upload
```bash
# Clone repository
git clone <repository-url>
cd waterSense

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor
```

### Configuration
Configure WaterSense using either the GUI tool or by editing `src/setup.h` manually.
Wake up → Stay awake for 5 minutes (HI_READ) → Sleep for 10 minutes (HI_ALLIGN) → Repeat

#### GUI Configuration Tool 🖥️
Use the streamlined configuration tool:
```bash
# Run from project root directory
python3 watersense_config.py
```

**Simple & Fast:**
- 🎛️ **All-in-One Interface** - Modes and timing in one compact window
- ✅ **One-Click Apply** - Directly updates `src/setup.h` with backup
- 🚫 **Smart Conflicts** - Auto-disables incompatible modes
- 📁 **Load/Save** - Import/export configurations

#### Manual Configuration
Edit `src/setup.h` to configure operating modes, timing, and hardware settings.

## Software Modes & Configurations

WaterSense supports multiple operating modes for different deployment scenarios:

### Operating Modes

| Mode | Description | Use Case | Conflicts |
|------|-------------|----------|-----------|
| **CONTINUOUS** | Always-on measurements, no sleep | Lab testing, continuous monitoring with AC power | None |
| **STANDALONE** | GNSS-only mode (no sensors) | GPS tracking applications, minimal power | ❌ NO_SURVEY |
| **NO_SURVEY** | GNSS for timing only, sensors active | Standard water monitoring with GPS sync | ❌ STANDALONE, LEGACY |
| **LEGACY** | Compatible with v3 Adafruit GPS | Older hardware configurations | ❌ NO_SURVEY, STANDALONE |
| **RADAR** | Radar distance sensor instead of ultrasonic | Harsh environments, better accuracy | None |
| **VARIABLE_DUTY** | Adaptive power management | Battery-powered deployments | None |

**⚠️ Mode Conflicts:**
- **STANDALONE** and **NO_SURVEY** cannot be enabled together
- **LEGACY** cannot be enabled with **NO_SURVEY** or **STANDALONE**

### Timing Configuration
- **Read Intervals:** High=5min, Mid=2min, Low=1min
- **Alignment:** High=10s, Mid=30s, Low=60s  
- **GNSS Update (Standalone):** Every 2 hours
- **Task Periods:** Measurement=100ms, SD=10ms, Voltage=1s

## Project Structure

```
src/
├── main.cpp              # Main program entry point
├── setup.h               # Configuration constants and pin definitions
├── sharedData.h          # Shared variables between tasks
├── waterSenseLibs/       # Hardware abstraction libraries
│   ├── maxbotixSonar/    # Ultrasonic sensor driver
│   ├── adafruitTempHumidity/ # Temperature/humidity sensor
│   ├── zedGNSS/          # GPS/GNSS module
│   ├── sdData/           # SD card operations
│   └── shares/           # Task synchronization utilities
└── waterSenseTasks/      # FreeRTOS task implementations
    ├── taskMeasure/      # Sensor measurement task
    ├── taskSD/           # Data logging task
    ├── taskClockGNSS/    # GPS timing task
    ├── taskSleep/        # Power management task
    └── taskWatch/        # System watchdog task
```

## Adding New Tasks

### 1. Create Task Files
Create a new directory in `src/waterSenseTasks/`:
```bash
mkdir src/waterSenseTasks/taskYourTask
```

Create header file `taskYourTask.h`:
```cpp
/**
 * @file taskYourTask.h
 * @brief Header for your new task
 */

void taskYourTask(void* params);
```

### 2. Implement Task Function
Create implementation file `taskYourTask.cpp`:
```cpp
#include <Arduino.h>
#include "taskYourTask.h"
#include "setup.h"
#include "sharedData.h"

void taskYourTask(void* params)
{
    // Task setup
    uint8_t state = 0;
    
    while (true)
    {
        // State machine logic
        switch (state)
        {
            case 0:
                // Initialization
                break;
            case 1:
                // Main operation
                break;
            case 2:
                // Sleep preparation
                break;
        }
        
        // Update watchdog (if applicable)
        yourTaskCheck.put(true);
        
        // Task delay
        vTaskDelay(YOUR_TASK_PERIOD);
    }
}
```

### 3. Add Shared Variables
In `sharedData.h`, declare any shared data:
```cpp
extern Share<bool> yourTaskCheck;
extern Share<int> yourDataValue;
```

In `main.cpp`, instantiate the shares:
```cpp
Share<bool> yourTaskCheck("Your Task Check");
Share<int> yourDataValue("Your Data");
```

### 4. Register Task in main.cpp
Add the include and create the task:
```cpp
#include "waterSenseTasks/taskYourTask/taskYourTask.h"

void setup()
{
    // ... existing setup code ...
    
    // Create your task (stack size, priority)
    xTaskCreate(taskYourTask, "Your Task", 4096, NULL, 5, NULL);
}
```

### 5. Configure Task Settings
Add task-specific constants to `setup.h`:
```cpp
#define YOUR_TASK_PERIOD 500  // Task period in ms
```

## Key Design Patterns

- **State Machines:** Tasks use state variables for different operational modes
- **Shared Variables:** Inter-task communication via thread-safe `Share<>` objects  
- **Watchdog System:** Each task updates a health check for system reliability
- **Power Management:** Tasks coordinate sleep states via shared flags
- **Priority-based:** Critical tasks (watchdog=10) > sensors(6-7) > power management(1)
