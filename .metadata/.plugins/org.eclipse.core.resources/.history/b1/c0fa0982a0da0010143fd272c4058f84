/*
 *  Title: Serial Port to CSV
 *  Description: This program reads from a serial port, processes the samples continuously, and saves the output into a CSV file.
 *  Author: Frédéric Waldmann
 *  Date: 10.09.2024
 *  Version: 1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#define BUFFER_SIZE 1024  // Buffer size for storing each complete value
#define CHUNK_SIZE 256    // Number of bytes to read in each call

// Function to configure and open the serial port
HANDLE setup_serial_port(const char* port_name) {
    // Open the serial port
    HANDLE hSerial = CreateFile(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening serial port %s\n", port_name);
        return INVALID_HANDLE_VALUE;
    } else {
        printf("Serial Port opened: %s\n", port_name);
    }

    // Configure serial port parameters
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Error getting serial port state\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Set serial port parameters (115200 baud, 8N1)
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Error setting serial port parameters\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("Error setting serial port timeouts\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    return hSerial;
}

int main(int argc, const char* argv[]) {

    char port_name[] = "COM5";  // Change this to the correct serial port on your PC (e.g., COM1, COM3, etc.)
    HANDLE serial_port = setup_serial_port(port_name);

    if (serial_port == INVALID_HANDLE_VALUE) {
        return 1;  // Failed to open the serial port
    }

    // Open the CSV file for writing
    char export_file_name[] = "../Export/data.csv";  // Output CSV file
    FILE* csvFile = fopen(export_file_name, "w");
    if (csvFile == NULL) {
        perror("Unable to open data.csv");
        CloseHandle(serial_port);
        return 1;
    }

    // ----------------------- START DSP Initialization -----------------------
    // Sampling: main.c triggers a measurement about every 10 ms -> ~100 Red/IR pairs per second.
    const float FS = 100.0f;          // effective sample rate in Hz (pairs per second)

    // IR / RED raw values
    float red_raw = 0.0f;
    float ir_raw  = 0.0f;

    // Filtered IR signal for HR detection (simple low-pass / smoothing)
    float ir_filt = 0.0f;
    const float alpha_filt = 0.2f;    // 0<alpha<1; higher = less smoothing

    // Heart-rate estimation
    float hr_bpm      = 0.0f;
    float hr_bpm_filt = 0.0f;
    int   sample_idx  = 0;            // counts IR samples (pairs)
    int   last_peak   = -1000;
    int   prev_peak   = -1000;
    const float peak_thr     = 10.0f; // threshold on filtered IR (adjust as needed)
    const float min_hr_bpm   = 40.0f;
    const float max_hr_bpm   = 200.0f;
    const float refractory_s = 0.3f;  // 300 ms refractory -> ~33 samples
    const int   refractory_n = (int)(refractory_s * FS);
    float prev_ir_filt = 0.0f;
    // ----------------------- END DSP Initialization -------------------------

    // Continuously read from the serial port
    char buffer[BUFFER_SIZE];  // Buffer to store the received line "red,ir"
    int buffer_index = 0;      // Index to track position in buffer
    char chunk[CHUNK_SIZE];    // Temporary buffer to read multiple bytes
    float proc_val = 0.0f;     // Value to write to CSV (here: filtered IR)
    DWORD n_bytes;             // Number of bytes read

    printf("Press CTRL+C to terminate...\n");

    while (1) {
        // Read from the serial port
        ReadFile(serial_port, chunk, CHUNK_SIZE, &n_bytes, NULL);

        if (n_bytes > 0) {

            // Process each byte
            for (DWORD i = 0; i < n_bytes; ++i) {
                if (chunk[i] == '\n') {

                    // End of a line (newline detected): buffer contains "red,ir\r" or "red,ir"
                    buffer[buffer_index] = '\0';  // Null-terminate the string

                    // Parse two values: red and ir
                    int red_int = 0;
                    int ir_int  = 0;
                    sscanf(buffer, "%d,%d", &red_int, &ir_int);
                    red_raw = (float)red_int;
                    ir_raw  = (float)ir_int;

                    printf("%d, %d\n", red_int, ir_int);

                    // ----------------------- START Processing -------------------------

                    // 1) Simple filtering of IR to get a smoother PPG for peak detection
                    ir_filt = ir_filt + alpha_filt * (ir_raw - ir_filt);

                    // 2) Heart-rate peak detection on filtered IR:
                    //    detect upward crossing of threshold with refractory time
                    if (prev_ir_filt < peak_thr && ir_filt >= peak_thr) {
                        if ((sample_idx - last_peak) > refractory_n) {

                            prev_peak = last_peak;
                            last_peak = sample_idx;

                            if (prev_peak >= 0) {
                                int   delta_n    = last_peak - prev_peak;
                                float period_sec = delta_n / FS;
                                float inst_hr    = 60.0f / period_sec;

                                // Accept only plausible HR values
                                if (inst_hr > min_hr_bpm && inst_hr < max_hr_bpm) {
                                    const float alpha_hr = 0.3f;
                                    hr_bpm      = inst_hr;
                                    hr_bpm_filt = hr_bpm_filt + alpha_hr * (hr_bpm - hr_bpm_filt);
                                    printf("HR ≈ %.1f bpm\n", hr_bpm_filt);
                                }
                            }
                        }
                    }
                    prev_ir_filt = ir_filt;

                    // 3) Choose what to send to CSV: here the filtered IR waveform
                    proc_val = ir_filt;

                    // ----------------------- END Processing -------------------------

                    // Save the processed value to the end of the CSV file.
                    fprintf(csvFile, "%f\n", proc_val);
                    fflush(csvFile);

                    buffer_index = 0;  // Reset buffer for the next line
                    sample_idx++;      // one more Red/IR pair processed

                } else {
                    // Accumulate characters until newline is detected
                    if (buffer_index < BUFFER_SIZE - 1) {
                        buffer[buffer_index++] = chunk[i];
                    } else {
                        fprintf(stderr, "Buffer overflow, discarding data\n");
                        buffer_index = 0;  // Reset buffer in case of overflow
                    }
                }
            }
        } else {
            if ((int)n_bytes < 0) {
                printf("Error reading from the serial port\n");
                break;
            }
        }
        Sleep(10);  // Wait for a short time to allow data to arrive
    }

    // when while loop get terminated properly close port and file
    if (CloseHandle(serial_port) == 0 && fclose(csvFile) == 0) {
        printf("Serial Port and CSV file closed.\n");
    }
    return 0;
}

