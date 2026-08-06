#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <SPIFFS.h>

// ── Configurable log size ───────────────────────────────────────────────────
// Change this to adjust how many rows are kept (ring buffer — oldest row is
// overwritten first once the buffer is full). At ~28 bytes/row, 500 rows is
// ~14 KB, a tiny fraction of the SPIFFS partition.
#define MAX_LOG_ROWS 500

#define LOG_FILE_PATH "/sensor_log.csv"

// CSV header — matches column order written per row.
// time_s = seconds since ESP32 boot (millis()/1000). Kept numeric on purpose
// so it imports cleanly into MATLAB (readmatrix/readtable) without a
// timestamp-parsing step.
static const char* LOG_HEADER = "time_s,temp_outside_C,temp_internal_C,ch4_ppm,co2_ppm";

struct LogRow {
    unsigned long timeS;
    float tempOutside;
    float tempInternal;
    float ch4Ppm;
    float co2Ppm;
};

class DataLogger {
private:
    LogRow buffer[MAX_LOG_ROWS];
    int    count     = 0;   // rows currently held (<= MAX_LOG_ROWS)
    int    writeIdx  = 0;   // next slot to write (wraps around)
    bool   wrapped   = false;

public:
    void begin() {
        // Nothing to load back in — buffer starts empty each boot. The file
        // on SPIFFS is rewritten fresh as new rows come in.
    }

    void addRow(float tempOutside, float tempInternal, float ch4Ppm, float co2Ppm) {
        LogRow row;
        row.timeS        = millis() / 1000UL;
        row.tempOutside  = tempOutside;
        row.tempInternal = tempInternal;
        row.ch4Ppm       = ch4Ppm;
        row.co2Ppm       = co2Ppm;

        buffer[writeIdx] = row;
        writeIdx = (writeIdx + 1) % MAX_LOG_ROWS;
        if (count < MAX_LOG_ROWS) {
            count++;
        } else {
            wrapped = true;
        }

        flushToFile();
    }

    // Rewrites the CSV file on SPIFFS from the current ring buffer contents,
    // oldest row first. Called after every new row so the download is always
    // up to date; if this becomes too much flash wear, add a "flush every N
    // rows" throttle later using a static counter here.
    void flushToFile() {
        File f = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
        if (!f) {
            Serial.println("[LOGGER] Failed to open log file for writing.");
            return;
        }

        f.println(LOG_HEADER);

        int startIdx = wrapped ? writeIdx : 0;
        for (int i = 0; i < count; i++) {
            int idx = (startIdx + i) % MAX_LOG_ROWS;
            LogRow &r = buffer[idx];
            f.printf("%lu,%.2f,%.2f,%.2f,%.2f\n",
                     r.timeS, r.tempOutside, r.tempInternal, r.ch4Ppm, r.co2Ppm);
        }

        f.close();
    }

    int rowCount() { return count; }
};

DataLogger dataLogger;

void initDataLogger() {
    dataLogger.begin();
}

#endif
