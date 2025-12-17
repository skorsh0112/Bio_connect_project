# --------------------------------------------------------------
# ADVANCED BIO-DASHBOARD: PPG + 2nd Derivative (SDPPG)
# --------------------------------------------------------------
import os
import collections
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from scipy.signal import savgol_filter # Essential for smooth derivatives

# --- Configuration ---
FILE_NAME = '../Export/data.csv'
BUFFER_SIZE = 400       # Smaller buffer = "Zoomed in" view to see the wave shapes
UPDATE_INTERVAL_MS = 30 # Refresh rate

# --- Path Setup ---
script_dir = os.path.dirname(os.path.abspath(__file__))
filepath = os.path.join(script_dir, FILE_NAME)

# --- Data Buffer ---
# We use a standard list here to convert to numpy easily later
raw_buffer = collections.deque([0.0] * BUFFER_SIZE, maxlen=BUFFER_SIZE)

# --- Plot Setup (3 Subplots) ---
# ShareX means if you zoom on one, all three zoom.
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
plt.subplots_adjust(hspace=0.3) # Space between graphs

# Plot 1: Standard PPG (Volume)
line1, = ax1.plot([], [], c='#d62728', lw=2)
ax1.set_title("1. Blood Volume (Standard PPG)", fontsize=10, loc='left')
ax1.set_ylabel("Vol")
ax1.grid(True, alpha=0.3)

# Plot 2: Velocity (1st Derivative)
line2, = ax2.plot([], [], c='#1f77b4', lw=1.5)
ax2.set_title("2. Blood Velocity (VPG)", fontsize=10, loc='left')
ax2.set_ylabel("Speed")
ax2.grid(True, alpha=0.3)

# Plot 3: Acceleration (2nd Derivative - SDPPG)
line3, = ax3.plot([], [], c='#2ca02c', lw=2) # Green line
ax3.set_title("3. Arterial Stiffness (Acceleration / SDPPG)", fontsize=10, loc='left', fontweight='bold')
ax3.set_ylabel("Accel")
ax3.set_xlabel("Time Samples")
ax3.grid(True, alpha=0.3)

# Mark the 'zero' line for acceleration to see positive/negative waves
ax3.axhline(y=0, color='black', linestyle='--', linewidth=0.8, alpha=0.5)

# --- Helper: File Reader ---
def data_generator():
    try:
        with open(filepath, 'r') as f:
            f.seek(0, os.SEEK_END) # Go to end
            while True:
                line_text = f.readline()
                if line_text:
                    try:
                        yield float(line_text.strip())
                    except ValueError:
                        continue
                else:
                    yield None
    except FileNotFoundError:
        print(f"Waiting for {filepath}...")
        yield None

stream = data_generator()

# --- The Math Engine ---
def update(frame):
    # 1. Read new data
    points_added = 0
    while True:
        try:
            val = next(stream)
        except StopIteration:
            break
        if val is None: break
        
        raw_buffer.append(val)
        points_added += 1
        if points_added > 100: break

    # 2. Process & Plot (Only if we have data)
    if len(raw_buffer) >= BUFFER_SIZE:
        # Convert to numpy array for math
        data_np = np.array(raw_buffer)
        
        # A. SMOOTHING (Critical for Derivatives)
        # Window length 15, Polynomial order 3. Adjust window (11, 15, 21) if too jittery.
        try:
            smooth_data = savgol_filter(data_np, window_length=25, polyorder=3)
        except:
            smooth_data = data_np # Fallback if buffer too small

        # B. CALCULATE DERIVATIVES
        velocity = np.gradient(smooth_data)
        acceleration = np.gradient(velocity)

        # C. UPDATE PLOTS
        x_axis = range(len(data_np))
        
        # Top Plot
        line1.set_data(x_axis, smooth_data)
        ax1.set_ylim(min(smooth_data), max(smooth_data))
        
        # Middle Plot
        line2.set_data(x_axis, velocity)
        ax2.set_ylim(min(velocity), max(velocity))
        
        # Bottom Plot (The Star of the Show)
        line3.set_data(x_axis, acceleration)
        # Add a tiny margin to y-limits so it looks neat
        mx = max(np.max(acceleration), 0.0001)
        mn = min(np.min(acceleration), -0.0001)
        ax3.set_ylim(mn * 1.2, mx * 1.2)
        
        ax3.set_xlim(0, len(data_np))

    return line1, line2, line3

# --- Run ---
print("Starting Dashboard...")
ani = FuncAnimation(fig, update, interval=UPDATE_INTERVAL_MS, blit=False, cache_frame_data=False)
plt.show()