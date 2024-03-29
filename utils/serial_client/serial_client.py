import serial
import datetime
import sys

import os

# workaround to prevent windows OS fall into sleep during data capturing
# https://stackoverflow.com/questions/57647034/prevent-sleep-mode-python-wakelock-on-python
class WindowsInhibitor:
    ES_CONTINUOUS = 0x80000000
    ES_SYSTEM_REQUIRED = 0x00000001

    def __init__(self):
        pass

    def inhibit(self):
        import ctypes
        print('Prevent Windows from going to sleep')
        ctypes.windll.kernel32.SetThreadExecutionState(
            WindowsInhibitor.ES_CONTINUOUS | WindowsInhibitor.ES_SYSTEM_REQUIRED)

    def uninhibit(self):
        import ctypes
        print('Allow Windows to go to sleep')
        ctypes.windll.kernel32.SetThreadExecutionState(WindowsInhibitor.ES_CONTINUOUS)

if len(sys.argv) != 2:
    raise ValueError('Please provide port name to connect.')

com = sys.argv[1]    
print(f'Connecting to port {com}')

port = serial.Serial(port=com, baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)

file = open('output.txt', 'w')
line = ''
line_count = 0

wi = None

try:
    wi = WindowsInhibitor()
    wi.inhibit()
    while True:
        if port.in_waiting > 0:
            line = port.readline()
            s = line.decode('ascii', errors='ignore')
            now = datetime.datetime.now()
            print('{} - {}'.format(now.strftime('%Y/%m/%dT%H:%M:%S'), s))
            file.write('{} - {}'.format(now.strftime('%Y/%m/%dT%H:%M:%S'), s))
            line_count += 1
            if line_count % 10 == 0:
                file.flush()
except Exception as e:
    print(f'Exception {e}')

if wi:
    wi.uninhibit()