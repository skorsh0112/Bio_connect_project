/*
 * Title: Serial Port to CSV (Dual Channel: BPM & SpO2)
 * Description: Reads "RED,IR" format, filters both, calculates BPM and SpO2.
 * Updated with Moving Average for smoothness and corrected Beat Threshold.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>

#define BUFFER_SIZE 1024
#define CHUNK_SIZE 256
#define AVG_WINDOW 4
#define MA_SIZE 8   // <--- ADDED: Size of the moving average window

// --- Signal Structure ---
typedef struct {
    float raw;
    float dc;
    float ac;
    float filtered;
    float min_val; // For amplitude tracking
    float max_val; // For amplitude tracking
} PPG_Channel;

HANDLE setup_serial_port(const char* port_name) {
    HANDLE hSerial = CreateFile(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;
    SetCommState(hSerial, &dcbSerialParams);

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);

    return hSerial;
}

int main(int argc, const char* argv[]) {

    // 1. SETUP
    char port_name[] = "COM5"; // <--- CHECK YOUR PORT
    HANDLE serial_port = setup_serial_port(port_name);
    if (serial_port == INVALID_HANDLE_VALUE) {
        printf("Failed to open %s\n", port_name);
        return 1;
    }

    FILE* csvFile = fopen("../Export/data.csv", "w");
    if (csvFile == NULL) {
        perror("Unable to open data.csv");
        CloseHandle(serial_port);
        return 1;
    }

    // 2. SIGNAL VARIABLES
    PPG_Channel red = {0};
    PPG_Channel ir = {0};

    // Filter Coefficients
    float alpha_dc = 0.02; // Slower DC tracking for better AC extraction
    float alpha_lp = 0.1;  // Low pass smoothing

    // BPM Variables
    DWORD last_beat_time = 0;
    int bpm_buffer[AVG_WINDOW] = {0};
    int bpm_buf_index = 0;
    int current_bpm = 0;

    // <--- FIXED: Threshold lowered to catch peaks around 1.5 -->
    float beat_thresh = 0.5;

    // <--- ADDED: Variables for Moving Average Filtering -->
    float ir_history[MA_SIZE] = {0};
    int ir_hist_idx = 0;

    // SpO2 Variables
    float current_spo2 = 98.0; // Start assumed healthy
    int samples_since_beat = 0;

    char buffer[BUFFER_SIZE];
    int buffer_index = 0;
    char chunk[CHUNK_SIZE];
    DWORD n_bytes;

    printf("Listening for 'RED,IR'... [SpO2 & BPM Active]\n");

    while (1) {
        ReadFile(serial_port, chunk, CHUNK_SIZE, &n_bytes, NULL);

        if (n_bytes > 0) {
            for (DWORD i = 0; i < n_bytes; ++i) {
                if (chunk[i] == '\n') {
                    buffer[buffer_index] = '\0';

                    // --- A. PARSING (The Demultiplexer) ---
                    // Partner sends: "12345,67890" (Red, IR)
                    char* token = strtok(buffer, ",");
                    if (token != NULL) {
                        red.raw = (float)atof(token);
                        token = strtok(NULL, ",");
                        if (token != NULL) {
                            ir.raw = (float)atof(token);
                        }
                    }

                    // --- B. SIGNAL PROCESSING (Dual Channel) ---

                    // 1. RED Channel Processing
                    if (red.dc == 0) red.dc = red.raw; // Init
                    red.dc = (alpha_dc * red.raw) + ((1.0 - alpha_dc) * red.dc);
                    red.ac = red.raw - red.dc;
                    // Invert RED signal (pulse = drop in light)
                    red.filtered = (alpha_lp * (-red.ac)) + ((1.0 - alpha_lp) * red.filtered);

                    // 2. IR Channel Processing (With Added Moving Average)
                    if (ir.dc == 0) ir.dc = ir.raw; // Init
                    ir.dc = (alpha_dc * ir.raw) + ((1.0 - alpha_dc) * ir.dc);
                    ir.ac = ir.raw - ir.dc;

                    // First step: Standard Low Pass Filter
                    float temp_filtered = (alpha_lp * (-ir.ac)) + ((1.0 - alpha_lp) * ir.filtered);

                    // <--- ADDED: Moving Average Step --->
                    ir_history[ir_hist_idx] = temp_filtered;
                    ir_hist_idx = (ir_hist_idx + 1) % MA_SIZE;

                    float sum = 0;
                    for(int k=0; k<MA_SIZE; k++) {
                        sum += ir_history[k];
                    }
                    ir.filtered = sum / MA_SIZE; // Final smooth signal used for graph & beats

                    // --- C. AMPLITUDE TRACKING (For SpO2) ---
                    if (red.ac > red.max_val) red.max_val = red.ac;
                    if (red.ac < red.min_val) red.min_val = red.ac;

                    if (ir.ac > ir.max_val) ir.max_val = ir.ac;
                    if (ir.ac < ir.min_val) ir.min_val = ir.ac;


                    // --- D. BEAT DETECTION (Using IR Signal) ---
                    DWORD now = GetTickCount();
                    samples_since_beat++;

                    // Check beat on IR filtered signal
                    if (ir.filtered > beat_thresh && (now - last_beat_time > 300)) {

                        DWORD ibi = now - last_beat_time;
                        last_beat_time = now;

                        if (ibi > 0) {
                            // 1. BPM Calculation
                            int instant_bpm = 60000 / ibi;

                            // Moving Average for BPM
                            bpm_buffer[bpm_buf_index] = instant_bpm;
                            bpm_buf_index = (bpm_buf_index + 1) % AVG_WINDOW;
                            int buffer_sum = 0, count = 0;
                            for(int k=0; k<AVG_WINDOW; k++) {
                                if(bpm_buffer[k] > 0) { buffer_sum += bpm_buffer[k]; count++; }
                            }
                            if(count > 0) current_bpm = buffer_sum / count;

                            // 2. SpO2 Calculation (At the moment of a beat)
                            // We use the amplitude range observed since the last beat
                            float ac_red_amp = red.max_val - red.min_val;
                            float ac_ir_amp = ir.max_val - ir.min_val;

                            // Avoid division by zero
                            if (ac_ir_amp > 0 && red.dc > 0 && ir.dc > 0) {
                                // Ratio of Ratios Formula
                                float R = (ac_red_amp / red.dc) / (ac_ir_amp / ir.dc);

                                // Standard approximation: SpO2 = 110 - 25 * R
                                float instant_spo2 = 110.0 - (25.0 * R);

                                // Constrain reasonable human limits
                                if (instant_spo2 > 100) instant_spo2 = 100;
                                if (instant_spo2 < 80) instant_spo2 = 80;

                                // Smooth the SpO2 value (it can be jittery)
                                current_spo2 = (0.1 * instant_spo2) + (0.9 * current_spo2);
                            }

                            // Reset Amplitude Trackers for next beat
                            red.max_val = -1000; red.min_val = 1000;
                            ir.max_val = -1000; ir.min_val = 1000;

                            printf(">> BEAT! BPM: %d | SpO2: %.1f%%\n", current_bpm, current_spo2);
                        }
                    }

                    // --- OUTPUT ---
                    // We print IR Filtered for the graph, and Red Filtered for debugging
                    printf("IR: %.2f | Red: %.2f | BPM: %d | SpO2: %.1f\n",
                           ir.filtered, red.filtered, current_bpm, current_spo2);

                    // Save to CSV (Saving IR filtered for the graph)
                    fprintf(csvFile, "%f\n", ir.filtered);
                    fflush(csvFile);

                    buffer_index = 0;
                } else {
                    if (buffer_index < BUFFER_SIZE - 1) buffer[buffer_index++] = chunk[i];
                }
            }
        }
        Sleep(5);
    }

    CloseHandle(serial_port);
    fclose(csvFile);
    return 0;
}

