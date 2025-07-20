# WaterSense Software Testing Guidelines

**Critical Rule: If it doesn't work when disconnected from your computer, it doesn't work.**

## Overview

These guidelines ensure WaterSense software is reliable and handles power issues properly before deployment. Focus on software validation, power monitoring, and error handling.

## Software Testing Checklist

### ✅ Phase 1: Power Supply & Software Validation

#### 1.1 5V Power Requirement Test
- [ ] **Verify 5V/1A minimum**: System must operate on 5V/1A external source
- [ ] **Test without USB connection**: Must work completely disconnected from computer
- [ ] **Power monitoring via serial**: Validate voltage readings in software
- [ ] **Brownout detection**: Software must detect and log voltage drops

**Power Supply Options:**
- ✅ **5V Power Banks** (software monitoring approved)
- ✅ **Solar Power Banks** (with software charging status)
- ✅ **External Solar Panels** (with software power monitoring)
- ❌ **3.7V Blue Batteries** (NOT ALLOWED for non-legacy systems - insufficient for multi-component systems)

#### 1.2 Software Power Monitoring Test
```bash
# Serial Monitor Power Validation
1. Connect system to 5V power bank
2. Monitor serial output for voltage readings
3. Watch for 48+ hours via serial monitor
4. Validate:
   - Voltage readings are reasonable (4.5V - 5.5V)
   - Power consumption tracking
   - Low voltage warnings in serial output
   - System behavior during power fluctuations
```

#### 1.3 Power Management Software Test
```bash
# Power Management Validation
1. Monitor serial output for power management messages
2. Check voltage reporting accuracy
3. Verify power-saving mode activation in software
4. Test variable duty cycle adjustments based on voltage
5. Validate power-related error messages and responses
```

### ✅ Phase 2: Data Integrity Validation

#### 2.1 SD Card Write Verification
- [ ] **Write validation**: Every write operation must be verified
- [ ] **Data corruption detection**: Check for corrupted or incomplete files
- [ ] **File system integrity**: Verify SD card file system remains stable
- [ ] **Power loss recovery**: Test SD card state after unexpected power loss

#### 2.2 Data Quality Checks
- [ ] **Time validation**: Ensure timestamps are reasonable (> June 1, 2025)
- [ ] **Sensor range validation**: Distance readings within expected ranges
- [ ] **Complete data records**: No missing fields or corrupted entries
- [ ] **File naming consistency**: Files named correctly with timestamps/counters

#### 2.3 Long-term Data Collection Test
```bash
# 7-Day Lab Test
1. Deploy system in lab with power bank
2. Run continuously for 1 week
3. Check data files every 24 hours
4. Verify:
   - Continuous data collection
   - No data gaps
   - Proper file rotation
   - SD card space management
```

### ✅ Phase 3: System Reliability Testing

#### 3.1 Error Handling Validation
- [ ] **SD card failure response**: System shuts down if SD writes fail
- [ ] **Sensor failure response**: Graceful handling of sensor disconnections
- [ ] **Power failure response**: Clean shutdown on low voltage
- [ ] **Time sync failure response**: Shutdown if time validation fails

#### 3.2 Watchdog and Recovery Testing
- [ ] **Watchdog timer function**: Verify system resets on task hangs
- [ ] **Boot sequence validation**: Clean startup after resets
- [ ] **Configuration persistence**: Settings survive power cycles
- [ ] **Error state recovery**: System recovers from error conditions

#### 3.3 Visual Status Indicators
- [ ] **LED Status Patterns**: Clear indication of system state
  - 🟢 **Solid Green**: Normal operation, data logging
  - 🟡 **Slow Blink Yellow**: Initializing/seeking GPS fix
  - 🔴 **Fast Blink Red**: Error condition, intervention needed
  - ⚫ **No Light**: System shutdown (indicates problem)
- [ ] **Status LED visible**: LED can be seen from outside enclosure

### ✅ Phase 4: Environmental Testing

#### 4.1 Temperature Range Testing
```bash
# Temperature Stress Test
1. Test operation from 0°C to 50°C
2. Monitor for:
   - System stability
   - Sensor accuracy drift
   - Battery/power bank performance
   - SD card reliability
```

#### 4.2 Vibration and Shock Testing
- [ ] **Transport simulation**: Survive typical vehicle transport
- [ ] **Mounting stress**: Stable operation when mounted/deployed
- [ ] **Connection integrity**: No loose connections under stress

#### 4.3 Weather Resistance (if applicable)
- [ ] **Enclosure sealing**: No moisture ingress
- [ ] **Connector protection**: All connections weather-sealed
- [ ] **Ventilation adequacy**: No condensation buildup

### ✅ Phase 5: Field Deployment Simulation

#### 5.1 Rooftop Test (Engineering Building)
```bash
# Multi-day Field Simulation
1. Deploy on Eng 13 rooftop with:
   - Complete power system (power bank + solar)
   - Full sensor suite (GNSS + Radar/Sonar)
   - Production enclosure
2. Run for minimum 72 hours (weekend test)
3. Monitor remotely via debug output
4. Collect and analyze all data
5. Check power bank status
```

#### 5.2 Remote Monitoring Setup
- [ ] **Debug logging**: Comprehensive serial output for troubleshooting
- [ ] **Remote access capability**: WiFi or cellular for status monitoring
- [ ] **Alert system**: Notification if system goes offline

### ✅ Phase 6: Configuration Testing

#### 6.1 Mode Validation
- [ ] **CONTINUOUS mode**: Lab testing with AC power
- [ ] **STANDALONE mode**: Continuous surveying for 2 hours, sleep for 1 minute(to be changed)
- [ ] **RADAR mode**: Verify radar sensor operation vs ultrasonic
- [ ] **VARIABLE_DUTY mode**: Power-adaptive operation

#### 6.2 Timing Validation
- [ ] **Read intervals**: Verify 5-minute default intervals
- [ ] **GNSS sync**: Confirm 2-hour GNSS update cycles
- [ ] **Sleep timing**: Accurate sleep duration calculations
- [ ] **Time alignment**: Proper minute alignment for data collection

## Power Supply Specifications

### Required Power Characteristics
- **Voltage**: 5V ±5%
- **Current**: Minimum 1A capability
- **Ripple**: <100mV peak-to-peak
- **Brownout threshold**: 4.5V (system must shutdown below this)

### Approved Power Solutions
1. **5V Power Banks** (20,000+ mAh recommended)
2. **Solar Power Banks** (integrated solar + battery)
3. **External Solar Panels** (with 5V power bank charging)

### Power Consumption Targets
- **Peak consumption**: <800mA (during GNSS fix + sensor read)
- **Average consumption**: <200mA (normal operation)
- **Sleep consumption**: <50mA (deep sleep mode)

## Failure Modes and Responses

### Critical Failures (System Shutdown Required)
1. **SD Card Write Failure**: `CRITICAL: SD write failed - System halting`
2. **Invalid Time**: `CRITICAL: Time validation failed - System halting`
3. **Low Voltage**: `CRITICAL: Voltage below 4.5V - System halting`
4. **Sensor Communication Loss**: `WARNING: Sensor offline - Continuing with available data`

### Visual Indicators for Field Staff
- **All LEDs OFF**: System has shut down due to critical error - immediate attention required
- **Red Fast Blink**: Non-critical error, system still operating but needs attention
- **Yellow Slow Blink**: Normal startup/initialization sequence
- **Green Solid**: Normal operation

## Debug Output Requirements

### Essential Debug Messages
```cpp
// Power monitoring
Serial.printf("POWER: Battery=%.2fV Solar=%.2fV Status=%s\n", battery, solar, status);

// SD card validation
Serial.printf("SD: Write=%s Verify=%s Size=%d bytes\n", writeOK, verifyOK, bytesWritten);

// Time validation
Serial.printf("TIME: Unix=%u Valid=%s GPS_Fix=%s\n", unixTime, timeValid, hasFix);

// System health
Serial.printf("HEALTH: Tasks=%s Voltage=%s SD=%s Sensors=%s\n", tasks, voltage, sd, sensors);
```

### Log File Requirements
- **Startup log**: System configuration and initialization status
- **Error log**: All warnings and errors with timestamps
- **Data log**: Sensor readings with validation status
- **Power log**: Voltage readings and power events

## Pre-Deployment Sign-off

Before field deployment, the following must be confirmed:

- [ ] **48-hour lab test completed successfully**
- [ ] **Power bank longevity verified for deployment duration**
- [ ] **All data files validated and readable**
- [ ] **No critical errors in debug logs**
- [ ] **LED status indicators verified functional**
- [ ] **Enclosure properly sealed and mounted**
- [ ] **Solar charging tested (if applicable)**
- [ ] **Emergency contact procedures established**

## Emergency Procedures

### If System Found "Dead" (No LEDs)
1. **Check power bank charge level**
2. **Inspect all connections**
3. **Review debug logs via serial connection**
4. **Check SD card for recent data**
5. **Replace power bank if depleted**

### If Data Collection Stopped
1. **Verify power bank status**
2. **Check SD card space and integrity**
3. **Review error logs for failure mode**
4. **Power cycle system with fresh power bank**

## Testing Documentation

### Required Test Records
- [ ] **Power consumption measurements**
- [ ] **SD card write verification results**
- [ ] **Environmental stress test results**
- [ ] **72-hour continuous operation log**
- [ ] **Solar charging performance data**
- [ ] **Error handling validation results**

---

**Remember**: The field is unforgiving. Any issue that can occur in the lab will occur in the field under worse conditions. Test thoroughly and assume worst-case scenarios. 