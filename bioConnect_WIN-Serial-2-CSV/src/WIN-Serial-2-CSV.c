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

    char port_name[] = "COM1";  // Change this to the correct serial port on your PC (e.g., COM1, COM3, etc.)
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

    //  ----------------------- START DSP Initialization -----------------------
    //  In this section you can initialize varialbes for your algorithm/filter.

    float scaling_factor = 50;
    float offset = 400;

    //  ----------------------- END DSP Initialization -------------------------

    // Continuously read from the serial port

    char buffer[BUFFER_SIZE];  // Buffer to store the received value
    int buffer_index = 0;      // Index to track position in buffer
    char chunk[CHUNK_SIZE];    // Temporary buffer to read multiple bytes
    int raw_val;               // Integer value of the full buffer
    float proc_val;            // Float value to write out
    DWORD n_bytes;             // Number of bytes read

    printf("Press CTRL+C to terminate...\n");

    while (1) {
        // Read from the serial port
        ReadFile(serial_port, chunk, CHUNK_SIZE, &n_bytes, NULL);

        if (n_bytes > 0) {

            // Process each byte
            for (DWORD i = 0; i < n_bytes; ++i) {
                if (chunk[i] == '\n') {

                    // End of a value (newline detected)
                    buffer[buffer_index] = '\0';  // Null-terminate the string
                    raw_val = atof(buffer);       // Convert string to float
                    printf("Raw value: %d\n", raw_val);  // Print the value
                    //  ----------------------- START Processing -------------------------
                    /*
                       Here you can implement your own algorithms or digital filter on the sensor data.
		   		       The new measurement is provided in the variable raw_val (float). The processed value
				       must be stored to the variable proc_val (float), which then will be appended to the
				       CSV file.
				    */

                    // Dummy code removing signal offset and scaling the signal by an arbitrary scale factor.
                    proc_val = raw_val - offset;
                    proc_val = proc_val * scaling_factor;
			   	    // Dumm code END

                    printf("Processed value: %f\n", proc_val);  // Print the value
                    //  ----------------------- END Processing -------------------------

                    // saves the processed value to the end of the csv file.
                    fprintf(csvFile, "%f\n", proc_val);
                    fflush(csvFile);

                    buffer_index = 0;  // Reset buffer for the next value

                } else {
                    // Accumulate characters until newline is detected
                    if (buffer_index < BUFFER_SIZE - 1) {
                        buffer[buffer_index++] = chunk[i];

                        // Handle buffer overflow
                    } else {
                        fprintf(stderr, "Buffer overflow, discarding data\n");
                        buffer_index = 0;  // Reset buffer in case of overflow
                    }
                }
            }
        } else {
            if (n_bytes < 0) {
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
