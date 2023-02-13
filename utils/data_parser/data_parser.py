import re

out_file_ccs811 = open('ccs811.csv', 'w')
out_file_ccs811.write('timestamp;CO2;TVOC\n')

out_file_dht22 = open('dht22.csv', 'w')
out_file_dht22.write('timestamp;humidity;temperature\n')

out_file_aht10 = open('aht10.csv', 'w')
out_file_aht10.write('timestamp;humidity;temperature\n')

# 2023/02/08T08:28:38 - DHT22: humidity 20.20 %, temperature 22 celsius
# 2023/02/08T08:28:38 - CCS811: CO2 400.00 ppm, TVOC 0.00 ppb    

def parse_ccs811(line, pos):
    match = re.search(r'\d{4}/\d{2}/\d{2}T\d{2}:\d{2}:\d{2}', line)
    if match != None:
        group = match.group()
        dt = group
    else:
        print(f'no date time in string {line}\n')
        return
    match = re.search(r'CO2 (\d+.\d+) ppm, TVOC (\d+.\d+) ppb', line[pos:])
    if match != None:
        co2 = match.group(1)
        tvoc = match.group(2)
    else:
        print(f'no measurements data in string {line}\n')
        return
    # print(f'{dt};{co2};{tvoc}\n')
    out_file_ccs811.write('{};{};{}\n'.format(dt, co2, tvoc))
        
def parse_dht22(line, pos):
    match = re.search(r'\d{4}/\d{2}/\d{2}T\d{2}:\d{2}:\d{2}', line)
    if match != None:
        group = match.group()
        dt = group
    else:
        print(f'no date time in string {line}\n')
        return
    match = re.search(r'humidity (\d+.\d+) %, temperature (\d+) celsius', line[pos:])
    if match != None:
        h = match.group(1)
        t = match.group(2)
    else:
        print(f'no measurements data in string {line}\n')
        return
    # print(f'{dt};{h};{t}\n')
    out_file_dht22.write('{};{};{}\n'.format(dt, h, t))
    
    
def parse_aht10(line, pos):
    match = re.search(r'\d{4}/\d{2}/\d{2}T\d{2}:\d{2}:\d{2}', line)
    if match != None:
        group = match.group()
        dt = group
    else:
        print(f'no date time in string {line}\n')
        return
    match = re.search(r'humidity (\d+.\d+) %, temperature (\d+) celsius', line[pos:])
    if match != None:
        h = match.group(1)
        t = match.group(2)
    else:
        print(f'no measurements data in string {line}\n')
        return
    # print(f'{dt};{h};{t}\n')
    out_file_aht10.write('{};{};{}\n'.format(dt, h, t))


with open('output.txt', 'r') as in_file:
    while True:
        line = in_file.readline()
        if not line:
            break
        pos = line.find('-')
        if pos == -1:
            continue
        p0 = pos + 2
        p1 = line.find(':', p0)
        if p1 == -1:
            continue
        sensor = line[p0:p1]
        # print(f'sensor:{sensor}\n')
        if sensor == 'CCS811':
            parse_ccs811(line, p1 + 1)
        elif sensor == 'DHT22':
            parse_dht22(line, p1 + 1)
        elif sensor == 'AHT10':
            parse_aht10(line, p1 + 1)
        else:
            print('unknown censor\n')