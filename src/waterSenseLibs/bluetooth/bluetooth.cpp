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
#include <SdFat.h>
#include "waterSenseLibs/sdData/sdData.h"

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
    if (!SD.begin(SD_CS, SD_SCK_MHZ(10))) {
        Serial.println("SD card initialization failed!");
        return false;
    }
    return true;
}


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
    
    // Check if file is too large (limit to 100KB for memory safety)
    if (fileSize > 100*1024) {
        Serial.printf("File too large: %d bytes (max 100KB)\n", fileSize);
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
    char nameBuf[64];
    const size_t MAX_FILE_SIZE = 90 * 1024;  // 90 KB
    int fileIndex = 1;
    int fileCount = 0;

    auto getFileName = [](int index) {
        return "/filelist" + String(index) + ".txt";
    };

   
    // Remove all existing filelistN.txt files AND MACOS specific ._file stuff in root only
    File root = SD.open("/");
    int max_iter = 100;
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file && max_iter>=0) {
            if (!file.isDirectory()) {
                file.getName(nameBuf, sizeof(nameBuf));
                String name =  String(nameBuf);// Step 1: Get name
                file.close();              // Step 2: Close file

                if ((name.indexOf("filelist") != -1) || (name.indexOf("._") != -1)) {
                    Serial.println("Removing old file: " + name);
                    SD.remove(String("/")+name.c_str()); // Step 3: Now it's safe
                }
            } else {
                file.close();  // Still close directories too
            }

            file = root.openNextFile();
            max_iter--;
        }
        root.close();
    }


     // Prepare first file
    File listFile = SD.open(getFileName(fileIndex).c_str(), O_WRITE | O_CREAT | O_APPEND);
    if (!listFile) {
        Serial.println("Failed to create initial filelist");
        return false;
    }

    // File rotation logic
    auto writePath = [&](const String& path) {
        if (!listFile) return;
        listFile.println(path);
        fileCount++;

        // If current file exceeds size limit, roll over
        if (listFile.size() >= MAX_FILE_SIZE) {
            listFile.close();
            fileIndex++;
            SD.remove(getFileName(fileIndex).c_str());
            listFile = SD.open(getFileName(fileIndex).c_str(), O_WRITE | O_CREAT | O_APPEND);
        }
    };

    // Directory traversal logic
    auto listDir = [&](const char* dirName) {
        if (!SD.exists(dirName)) return;
        File dir = SD.open(dirName);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return;
        }
        File file = dir.openNextFile();
        while (file) {
            file.getName(nameBuf, sizeof(nameBuf));
            if (!file.isDirectory()&&(String(nameBuf).indexOf("filelist")==-1)) {
                String fullPath = String(dirName) + "/" + String(nameBuf);
                writePath(fullPath);
                Serial.println("Writing path: " + fullPath);
            }
            file.close();
            file = dir.openNextFile();
        }
        dir.close();
    };

    // Walk target directories
    listDir("/Data");
    listDir("/GNSS_Data");

    // List root-level files (non-directories)
    File rootf = SD.open("/");
    if (rootf && rootf.isDirectory()) {
        File file = rootf.openNextFile();
        while (file) {
            file.getName(nameBuf, sizeof(nameBuf));
            if (!file.isDirectory()&&(String(nameBuf).indexOf("filelist")==-1)) {
                writePath("/" + String(nameBuf));
                Serial.println("Writing path: " + String(nameBuf));
            }
            file.close();
            file = rootf.openNextFile();
        }
        rootf.close();
    }

    if (listFile) listFile.close();
    Serial.printf("Generated %d file(s) across %d filelist chunk(s)\n", fileCount, fileIndex);
    return true;
}
