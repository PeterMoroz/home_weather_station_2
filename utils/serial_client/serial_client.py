import serial
import datetime
import sys

if len(sys.argv) != 2:
    raise ValueError('Please provide port name to connect.')

com = sys.argv[1]    
print(f'Connecting to port {com}')

port = serial.Serial(port=com, baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)

file = open('output.txt', 'w')
line = ''
line_count = 0

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