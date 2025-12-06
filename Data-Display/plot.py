# --------------------------------------------------------------
# Python Script as part of the BioConnect Project Template 
# 
# This script reads PPG values from the file FILE_NAME and displays
# the data in a live plot. 
# --------------------------------------------------------------
 
import csv
import os
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import collections


FILE_NAME = '../Export/data.csv' #'/Users/skorsh/STM32CubeIDE/Project_bioConnect/bioConnect_UNIX-Serial-2-CSV/src/data.csv' # in original file it was ../Export/data.csv 
BUFFER_SIZE = 2**12  # Length of the buffer
UPDATE_AFTER_MS = 10  # Updates plot after x miliseconds

# Fixed size buffer with fast appends and pops on eighter end (FILO-Queue)
y_data = collections.deque(maxlen=BUFFER_SIZE)

# Initialize the plot
fig, ax = plt.subplots()
line, = ax.plot([], [], c='k')  # Line to be updated
ax.set_ylim(-100000, 100000)  # Adjust y-axis !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! in original -2300 to 4200
a, b = 0, BUFFER_SIZE  # Set x-axis limits

# Opening the CSV file
filepath = os.path.join(os.path.dirname(__file__), FILE_NAME)
with open(filepath, 'r') as f:
    # Getting the number of rows of the CSV file
    row_count = sum(1 for row in csv.reader(f))

# Open CSV file and fill buffer with last chunk of data
file = open(filepath, 'r')
reader = csv.reader(file)
for i, row in enumerate(reader):
    if i == (row_count-BUFFER_SIZE) and row:
        y_data.append(float(row[0]))
        break

# Function to initialize the plot
def init():
    line.set_data([], [])
    return line,

# Function to update the plot
def update(frame):
    global y_data, a, b # declare global variables
    
    # Read new lines from the CSV file
    for i, row in enumerate(reader):
        if row:

            # Append new row of data to the buffer (pops the first element)
            y_data.append(float(row[0])) 
            
            # Update x-axis limits
            a += 1 
            b += 1
   
            # Update the line plot
            line.set_data(range(a, b), y_data)       
            
            # Adjust x-axis
            ax.set_xlim(a, b)
        else:
            print('waiting')

    return line,

# Create animation function which countinously calls the update function 
ani = FuncAnimation(fig, update, frames=None, init_func=init, blit=False, interval=UPDATE_AFTER_MS, save_count=BUFFER_SIZE)

# Close the CSV file when the window is closed
def close_file(event):
    print("Closing file...")
    file.close()

# Connect the close event to also close the file
fig.canvas.mpl_connect('close_event', close_file)

# Show the plot
plt.show()
