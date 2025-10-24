/**
 * @file    taskRadar.cpp
 * @author  Armaan Oberai
 * @brief   Radar task (distance mode) using the SparkFun XM125 Distance API
 * @version 0.1
 * @date    2025-04-17
 *
 * @copyright Copyright (c) 2023
 */

 #include <Arduino.h>
 #include <Wire.h>
 
 #include "taskRadar.h"
 #include "setup.h"
 #include "sharedData.h"
 #include "SparkFun_Qwiic_XM125_Arduino_Library.h"
 
 void taskRadar(void* params)
 {
     SparkFunXM125Distance myRadar;
     const uint8_t  I2C_ADDR  = SFE_XM125_I2C_ADDRESS;
     const uint32_t RANGE_MIN = 1000;    // mm
     const uint32_t RANGE_MAX = 13000;  // mm
 
     uint8_t state = 0;
     Serial.println("[RadarTask] Task started, awaiting wakeReady...");
 
     while (true)
     {
         if (state == 0)  // ── Initialization ──
         {
             if (wakeReady.get())
             {
                 Serial.println("[RadarTask] Wake → init I2C + radar...");
 
                 if (myRadar.begin(I2C_ADDR, Wire) != 1)
                 {
                     Serial.println("[RadarTask][ERROR] begin() failed! Suspending task.");
                 }

                 int32_t err = myRadar.distanceSetup(RANGE_MIN, RANGE_MAX);
                 if (err != 0)
                 {
                     Serial.print("[RadarTask][ERROR] distanceSetup() → ");
                     Serial.println(err);
                 }
 
                 Serial.printf("[RadarTask] Range set: %umm%umm\n", RANGE_MIN, RANGE_MAX);
                 radarSleepReady.put(false);
                // 1) cancel any tiny leakage echo near the start 
                 myRadar.setCloseRangeLeakageCancellation(true);

                 state = 1;
             }
         }
         else if (state == 1 && !BluetoothConnected.get())  // ── Decide: sleep or measure ──
         {
             if (sleepFlag.get())
             {
                 Serial.println("[RadarTask] Sleep flag set → entering sleep");
                 state = 3;
             }
             else
             {
                 state = 2;
             }
         }
         else if (state == 2)  // ── Trigger & read one measurement ──
         {
             Serial.println("[RadarTask] Triggering distance measurement...");
             int32_t ret = myRadar.detectorReadingSetup();
             if (ret != 0)
             {
                 Serial.print("[RadarTask][ERROR] detectorReadingSetup() → ");
                 Serial.println(ret);
                 // retry next cycle
             }
             else
             {
                 Serial.println("[RadarTask] Waiting for data...");
                 myRadar.busyWait();
 
                 uint32_t numDistances = 0;
                 myRadar.getNumberDistances(numDistances);
 
                 if (numDistances == 0)
                 {
                     Serial.println("[RadarTask] No distance peaks detected");
                 }
                 else
                 {
                     Serial.printf("[RadarTask] Peaks detected: %u\n", numDistances);
 
                     // Find the furthest peak
                     uint32_t furthestMm = 0;
                     for (uint32_t i = 0; i < numDistances; i++)
                     {
                         uint32_t distMm = 0;
                         if (myRadar.getPeakDistance(i, distMm) != ksfTkErrOk)
                         {
                             Serial.print("[RadarTask][ERROR] getPeakDistance(");
                             Serial.print(i);
                             Serial.println(")");
                             continue;
                         }
                         if (distMm > furthestMm) furthestMm = distMm;
 
                         // debug print each peak
                         float distCm = distMm * 0.1f;
                         Serial.printf("   Peak %u: %.1f cm\n", i, distCm);
                     }
 
                     // Publish the furthest peak
                     if (furthestMm > 0)
                     {
                         float furthestCm = furthestMm * 0.1f;
                         Serial.printf("[RadarTask] Furthest peak: %.1f cm\n", furthestCm);
                         // Store as integer cm
                         distance.put((int16_t)furthestMm);
                         dataReady.put(true);
                     }
                 }
             }
             state = 1;  // back to check for sleep/measure
         }
         else if (state == 3)  // ── Stop & sleep ──
         {
             Serial.println("[RadarTask] Stopping detector and going to sleep...");
             myRadar.stop();
             radarSleepReady.put(true);
             state = 0;
         }
 
         radarCheck.put(true);
         vTaskDelay(pdMS_TO_TICKS(RADAR_TASK_PERIOD));
     }
 }