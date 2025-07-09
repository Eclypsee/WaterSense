/**
 * @file bluetooth.cpp
 * @author Evan Lee
 * @brief Implementation file for Bluetooth file management library
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "bluetooth.h"
#include "setup.h"

// Global instance
BluetoothFileManager bluetoothFileManager;

BluetoothFileManager::BluetoothFileManager() {
    fileList = "";
    currentFileData = "";
    currentFileName = "";
    fileLoaded = false;
    currentChecksum = 0;
}

bool BluetoothFileManager::begin() {
    // Refresh file list on initialization
    //refreshFileList();
    return true;
}

// String BluetoothFileManager::getFileListFromSD() {
//     String list = "";
//     File root = SD.open("/");
    
//     if (!root) {
//         Serial.println("Failed to open SD card root directory");
//         return "Error: Cannot access SD card";
//     }
    
//     if (!root.isDirectory()) {
//         Serial.println("Root is not a directory");
//         root.close();
//         return "Error: Invalid SD card structure";
//     }
    
//     File file = root.openNextFile();
//     int fileCount = 0;
    
//     while (file) {
//         if (!file.isDirectory()) {
//             if (fileCount > 0) {
//                 list += ",";
//             }
//             list += file.name();
//             fileCount++;
//         }
//         file.close(); // Close current file before getting next
//         file = root.openNextFile();
//     }
    
//     root.close();
    
//     if (fileCount == 0) {
//         list = "No files found on SD card";
//     }
    
//     Serial.printf("Found %d files on SD card\n", fileCount);
//     return list;
// }

bool BluetoothFileManager::loadFileFromSD(const String& fileName) {
    // Clear previous file data
    currentFileData = "";
    fileLoaded = false;
    currentChecksum = 0;
    
    // Open file from SD card
    File file = SD.open(fileName);
    
    if (!file) {
        Serial.printf("Failed to open file: %s\n", fileName.c_str());
        return false;
    }
    
    if (file.isDirectory()) {
        Serial.printf("Error: %s is a directory\n", fileName.c_str());
        file.close();
        return false;
    }
    
    // Read file data
    size_t fileSize = file.size();
    Serial.printf("Loading file: %s (size: %d bytes)\n", fileName.c_str(), fileSize);
    
    // Check if file is too large (limit to 64KB for memory safety)
    if (fileSize > 65536) {
        Serial.printf("File too large: %d bytes (max 64KB)\n", fileSize);
        file.close();
        return false;
    }
    
    // Pre-allocate string size to prevent memory fragmentation
    currentFileData.reserve(fileSize);
    
    // Read file into string
    while (file.available()) {
        char c = file.read();
        currentFileData += c;
    }
    
    file.close();
    
    // Set file as loaded and calculate checksum
    currentFileName = fileName;
    fileLoaded = true;
    currentChecksum = calculateChecksum(currentFileData);
    
    Serial.printf("File loaded successfully: %s (%d bytes, checksum: %u)\n", 
                  fileName.c_str(), currentFileData.length(), currentChecksum);
    return true;
}

String BluetoothFileManager::getFileList() {
    return fileList;
}

bool BluetoothFileManager::loadFile(const String& fileName) {
    return loadFileFromSD(fileName);
}

String BluetoothFileManager::getFileData() {
    return currentFileData;
}

String BluetoothFileManager::getCurrentFileName() {
    return currentFileName;
}

bool BluetoothFileManager::isFileLoaded() {
    return fileLoaded;
}

uint32_t BluetoothFileManager::getCurrentChecksum() {
    return currentChecksum;
}

// void BluetoothFileManager::refreshFileList() {
//     fileList = getFileListFromSD();
//     Serial.printf("File list refreshed: %s\n", fileList.c_str());
// }

uint32_t BluetoothFileManager::calculateChecksum(const String& data) {
    uint32_t checksum = 0;
    for (int i = 0; i < data.length(); i++) {
        checksum = ((checksum << 1) + (uint32_t)data[i]) ^ (checksum >> 31);
    }
    return checksum;
}

void BluetoothFileManager::clearFile() {
    currentFileData = "";
    currentFileName = "";
    fileLoaded = false;
    currentChecksum = 0;
}

bool BluetoothFileManager::generateFileList() {
    // Create filelist.txt on SD card
    File listFile = SD.open("filelist.txt", FILE_WRITE);
    
    if (!listFile) {
        Serial.println("Failed to create filelist.txt");
        return false;
    }
    

    
    // Get all files from SD card
    File root = SD.open("/");
    
    if (!root) {
        Serial.println("Failed to open SD card root directory");
        listFile.close();
        return false;
    }
    
    if (!root.isDirectory()) {
        Serial.println("Root is not a directory");
        root.close();
        listFile.close();
        return false;
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    
    while (file) {
        if (!file.isDirectory()) {
            // Write filename to filelist.txt, one per line
            listFile.println(file.name());
            fileCount++;
        }
        file.close(); // Close current file before getting next
        file = root.openNextFile();
    }
    
    root.close();
    listFile.close();
    
    Serial.printf("Generated filelist.txt with %d files\n", fileCount);
    return true;
}