# --------------------------------------------------------------
# Corrected Python Script for BioConnect Project
# --------------------------------------------------------------
import csv
import os
import time
import collections
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- Configuration ---
FILE_NAME = '../Export/data.csv'
BUFFER_SIZE = 1000      # How many points to show on screen
UPDATE_INTERVAL_MS = 10 # How fast to refresh (lower = smoother)

# --- Path Setup ---
# This ensures Python finds the file relative to where the script is saved
script_dir = os.path.dirname(os.path.abspath(__file__))
filepath = os.path.join(script_dir, FILE_NAME)

# --- Data Buffer ---
# A deque is a list that automatically pops old items when full
y_data = collections.deque([0] * BUFFER_SIZE, maxlen=BUFFER_SIZE)

# --- Plot Setup ---
fig, ax = plt.subplots()
line, = ax.plot([], [], c='#d62728', linewidth=1.2) # Red line style
ax.set_facecolor('#f0f0f0') # Light gray background
ax.grid(True, which='both', linestyle='--', linewidth=0.5)

# Labels
ax.set_title("Live PPG Signal (Filtered)")
ax.set_xlabel("Time (Samples)")
ax.set_ylabel("Amplitude")

# --- Helper: File Reader Generator ---
# This function handles the "tailing" logic (reading a file that is growing)
def data_generator():
    try:
        with open(filepath, 'r') as f:
            # Move to the end of the file immediately so we don't read old data
            f.seek(0, os.SEEK_END)
            
            while True:
                line_text = f.readline()
                if line_text:
                    try:
                        # Parse the float from the C code output
                        val = float(line_text.strip())
                        yield val
                    except ValueError:
                        continue # Skip bad lines (if any)
                else:
                    # If no new line, yield None to say "waiting"
                    yield None
    except FileNotFoundError:
        print(f"Waiting for file: {filepath}...")
        yield None

# Initialize the data stream
stream = data_generator()

# --- Animation Update Function ---
def update(frame):
    # 1. Read ALL new data available in the file buffer
    points_added = 0
    while True:
        try:
            val = next(stream)
        except StopIteration:
            break
            
        if val is None:
            break # No more new data right now
            
        y_data.append(val)
        points_added += 1

        # Limit: Don't read more than 500 points in one frame (prevents freezing)
        if points_added > 500: 
            break

    # 2. Only redraw if we actually added new data
    if points_added > 0:
        # Update the line data
        line.set_data(range(len(y_data)), y_data)
        
        # Adjust X-Axis
        ax.set_xlim(0, len(y_data))
        
        # --- AUTO-SCALE Y-AXIS ---
        # This is critical because we don't know the exact amplitude of your sensor
        current_min = min(y_data)
        current_max = max(y_data)
        
        # Add a little padding (margin) to the top and bottom
        margin = (current_max - current_min) * 0.1
        if margin == 0: margin = 1 # Prevent crash on flat line
        
        ax.set_ylim(current_min - margin, current_max + margin)

    return line,

# --- Run the Animation ---
print(f"Reading from: {filepath}")
print("Plot window open...")

ani = FuncAnimation(fig, update, interval=UPDATE_INTERVAL_MS, blit=False, cache_frame_data=False)
plt.show()