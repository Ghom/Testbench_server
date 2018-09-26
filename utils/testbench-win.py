#!/usr/bin/python3
import socket
import sys
import time
import os.path
import shutil
from pathlib import Path
import csv
import datetime
import select

CSVIN_HEADER = "Motor Speed(rpm);Measure Time(sec);Measure Rep;RFID Power(dbm);\n"
CSVIN_EXEMPLE = "100;60;1;13;\n"
CSV_DELIMITER = ';'
CSV_EXEMPLE_FILE = "exemple-csv-in.csv"
TMP_DIR = ".tmp"

HOST = '192.168.1.20'
PORT = 12345
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
connected = False

#typedef enum {CMD_RFID=0, CMD_MOTOR, INFO_RFID, INFO_MOTOR} packet_type;
CMD_RFID = 0x00
CMD_MOTOR = 0x01
INFO_RFID = 0x02
INFO_MOTOR = 0x03
RFID_DIRECT = 0x04

#typedef enum {CMD_CHANGE_SPEED=0, CMD_GET_SPEED, CMD_SIGNAL_STABLE_SPEED} motor_cmd_type_t;
CMD_SIGNAL_STABLE_SPEED = 0x02


ST_COM_WRITE_REG = 0x68
CMD_TAG_DETECTED = 0x16

def csv_parser(csvPath):
    with open(csvPath, 'r') as csvF:
        rd = csv.DictReader(csvF, delimiter=';')
        line = 0
        skiped = 0
        testlist = []
        for row in rd:
            line += 1
            try:
                d = dict()
                d['testnum'] = line
                speed = int(row['Motor Speed(rpm)'])
                if not (100 <= speed <= 500) and (speed != 0): 
                    print("Warning: {}:{} the field 'Motor Speed(rpm)' need to be in range [100:500] rpm or 0 rpm -- skip test".format(csvPath, line))
                    skiped += 1
                    continue
                d['speed'] = speed
                d['duration'] = int(row['Measure Time(sec)'])
                d['repetition'] = int(row['Measure Rep'])

                power = int(row['RFID Power(dbm)'])
                if not (1 <= power <= 20):
                    s.makedirs(directory)
                    print("Warning: {}:{} the field 'RFID Power(dbm)' need to be in range [1:20] dbm -- skip test".format(csvPath, line))
                    skiped += 1
                    continue
                d['power'] = power                
                testlist.append(d)
            except ValueError:
                print("Warning: {}:{} contain a field that is not a number -- skip test".format(csvPath, line))
                skiped += 1
                continue
        
        if(skiped):
            print("{}/{} will be skiped. do you still want to proceed? (Yes/n)".format(skiped, line))
            if input() != "Yes":
                exit()

        return testlist

# Write the result to the output csv file
def write_result(csvPath, result):
    pass

# Connect to the testbench server
def connect(addr, port):
    try:
        server.connect((addr, port))
        server.setblocking(0)
        print('Connexion vers ' + addr + ':' + str(port) + ' reussie.')
        connected = True
    except ConnectionRefusedError:
        print("Error: Could not connect to {}:{}. Make sure the server is started".format(addr, port))
        exit()

# Disconnect from the testbench server
def disconnect():
    global connected
    print ('Deconnexion.')
    if(connected):
        server.close()
        connected = False

# Handle exiting potentially in the middle of a test, the scan, motor and server connection need to be stoped if possible
def cleanExit():
    disconnect()
    exit()

# Get the following incoming cmd on the network
def get_cmd(): 
    cmd_detected = False
    timeout = False
    cmd_size = -1
    cmd_type = -1
    magic = 0
    magic_detected = False
    frame = []
    cmd = dict()
    while not cmd_detected:
        ready = select.select([server], [], [], 0.5)
        if ready[0]:
            byte = server.recv(1)

            value = int(byte[0])
            #print('value:{}'.format(hex(byte[0])))
            magic = ((magic>>8) | value<<24) & 0xFFFFFFFF
            #print('magic is {}'.format(hex(magic)))
            if(magic == 0x41421356):
                magic_detected=True
                continue
            
            if(magic_detected):
                #print('value:{}'.format(hex(value)))
                frame.append(value)

            if(len(frame) == 3):
                cmd_type = frame[0]
                cmd_size = (frame[1] | frame[2]<<8) & 0xFFFF
                cmd["type"] = cmd_type
                cmd["size"] = cmd_size
                #print("size: " + str(cmd_size))
            elif(len(frame) > 3):
                cmd_size -=1
                if(cmd_size == 0):
                    cmd_detected = True
        else:
            #print("timeout")
            timeout = True
            return -1

    #print("Command detected: {}".format(frame))
    cmd["data"] = frame[3:]
    
    return cmd
# Send the command to change the motor speed and wait until the speed as been reached
def motor_speed(speed):
    clockwise = 1
    if(speed<0):
        speed = abs(speed)
        clockwise = 0

    #                       MAGIC                       type    size        type    argc    arg[...]
    command_motor = bytes([ 0x41, 0x42, 0x13, 0x56,     1,      0, 5,       0,      3,      speed>>8, speed&0xFF, clockwise])

    n = server.send(command_motor)
    if (n != len(command_motor)):
        print ('Erreur envoi.')
        cleanExit()

    stabelised = False
    while(not stabelised):
        cmd = get_cmd()
        if(cmd != -1 and cmd["type"] == INFO_MOTOR and cmd["data"][0] == CMD_SIGNAL_STABLE_SPEED):
            stabelised = True

def dbm_to_reg(dbm):
    value = 0
    if(dbm >= 9):
        value = 20-dbm
    elif(dbm < 9):
        value = 0x10 + (8-dbm)

    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x00;    //   0dB,  20dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x07;    //  -7dB,  13dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x08;    //  -8dB,  12dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x09;    //  -9dB,  11dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x0A;    // -10dB,  10dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x0B;    // -11dB,   9dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x10;    // -12dB,   8dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x11;    // -13dB,   7dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x12;    // -14dB,   6dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x13;    // -15dB,   5dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x14;    // -16dB,   4dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x15;    // -17dB,   3dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x16;    // -18dB,   2dBm
    #define ST25RU3993_REG_MODULATORCONTROL3_VALUE     0x17;    // -19dB,   1dBm
    return value

# Send command to RFID board 
def rfid_set(param, value):
    command = []
    if(param == "scan"):    
        #            MAGIC                       type    size        type    argc    arg[...]
        command = [  0x41, 0x42, 0x13, 0x56,     0,      0, 3,       0x17,   1,      0]
        if(value):
            #print("Start scan")
            command[9] = 1
        else:
            #print("Stop scan")
            command[9] = 0

    if(param == "power"):
        reg = dbm_to_reg(value)
        #            MAGIC                       type               size    TID    reserved payload      protocol            tx-msb-lsb  rx-msb-lsb     data[@      value] 
        command = [  0x41, 0x42, 0x13, 0x56,     RFID_DIRECT,       0, 11,  0x00,  0x00,    0x00,0x07,   ST_COM_WRITE_REG,   0x00,0x02,  0x00,0x00,         0x15,   reg]

    n = server.send(bytes(command))
    if (n != len(command)):
        print ('Erreur envoi.')

def timestampMS():
        return int((datetime.datetime.utcnow() - datetime.datetime(1970, 1, 1)).total_seconds() * 1000)

# Start a test with the specified parameters
def start_test(params):
    motor_speed(params["speed"])
    rfid_set("power", params["power"])
    result = []

    for rep in range(params["repetition"]):
        result.append(["Repetition",rep+1])
        rfid_set("scan", True)
        #wait to make sure we get data straight away
        #time.sleep(0.5)
        
        #start timer
        start_time = timestampMS()
        elapsed_time = timestampMS() - start_time
        timestamp_first = 0

        while(elapsed_time < params["duration"]*1000):
                elapsed_time = timestampMS() - start_time
                #print("Elapsed time: {} sec".format(elapsed_time))
                # retrieve scan informations
                cmd = get_cmd()
                #print("type:{} sub-type:{} INFO_RFID:{} CMD_TAG_DETECTED:{}".format(cmd["type"], cmd["data"][0], INFO_RFID, CMD_TAG_DETECTED))
                if( cmd!= -1 and (cmd["type"] == INFO_RFID) and (cmd["data"][0] == CMD_TAG_DETECTED)):
                    timestamp = 0
                    for i,value in enumerate(cmd["data"][1:8]):
                        #print("value[{}]: {}".format(i, hex(value)))
                        timestamp = timestamp | (value << (i*8)) & 0xFFFFFFFFFFFFFFFF
                    tag = (cmd["data"][9] | cmd["data"][10] << 8) & 0xFFFF
                    
                    if timestamp_first == 0:
                        timestamp_first = timestamp

                    timestamp -= timestamp_first

                    result.append([timestamp ,tag])
                    #print("Timestamp:{} tag:{}".format(timestamp, tag))
                        

        #stop scan
        rfid_set("scan", False)
        result.append(["",""])
        time.sleep(1)
        
        #purge previous scan
        while(get_cmd() != -1):
            pass

    # maybe reset to default state ?
    
    return result

def save_tmp(result, testlist):
#    print("save result")
    # create the file if it doesn't exist and append it if it does
    with open("{}/tmp_{}.csv".format(TMP_DIR,testlist["testnum"]), 'w+') as resFile:
        writer = csv.writer(resFile, delimiter=';')
        writer.writerow(["Test #{}".format(testlist["testnum"]),'','','','',''])
        header = CSVIN_HEADER[:-1].split(';')
        header.append('')
        writer.writerow(header)
        writer.writerow([testlist['speed'], testlist['duration'], testlist['repetition'], testlist['power'],'',''])
        writer.writerow(['timer(ms)', 'RFID detection','','','',''])
        for row in result:
            row.extend(['','','',''])
            writer.writerow(row)
        resFile.close() 

def clear_tmp_file():
    if Path(TMP_DIR).is_dir():
        shutil.rmtree(TMP_DIR)
    os.makedirs(TMP_DIR)

def combine_tmp_file(csvOut):
    path, dirs, files = next(os.walk(TMP_DIR))
    file_count = len(files)
    file_data = []
    prepend_col = 0

    for filenum in range(file_count):
        file_path = "{}/tmp_{}.csv".format(TMP_DIR,filenum+1)

        with open(file_path) as f:
            rd = csv.reader(f)
            for num, row in enumerate(rd):
                try:
                    file_data[num][0] += row[0]
                except IndexError:
                    row[0] = prepend_col*";" + row[0]
                    file_data.append(row)

        prepend_col += 5

    with open(csvOut,'w+') as resfile:
        wr = csv.writer(resfile)
        for row in file_data:
            wr.writerow(row)


# Helper function to output an exemple csv input file with the correct format
def create_exemple():
    with open(CSV_EXEMPLE_FILE, 'w') as exempleFile:
        exempleFile.write(CSVIN_HEADER)
        exempleFile.write(CSVIN_EXEMPLE)
        exempleFile.close()

#------------------- Entry point --------------------#
sys.argv = [".\testbench.py","exemple-csv-in.csv","result.csv"]
if len(sys.argv) != 3:
    print("Wrong number of arguments")
    print("Usage: {} <test-file-path>.csv <path-result-path>.csv".format(sys.argv[0]))
    exit()

csvIn = sys.argv[1]
csvOut = sys.argv[2]

# test if the CSV in file exist
csvInFile = Path(csvIn)
if not csvInFile.is_file():
    print("Error: {} specified doesn't exist".format(csvIn))
    exit()

# test if the CSV in file is a CSV and is the correct version with (header)
if not csvIn.endswith('.csv'):
    print("Error: {} is not a CSV file make sure the file ends with '.csv'".format(csvIn))
    exit()

with open(csvIn, 'r') as csvInFile:
    # check header parameter match the expected format
    if csvInFile.readline() != CSVIN_HEADER:
        print("Error: The file {} doesn't match the expected format (checkout the file {})".format(csvIn, CSV_EXEMPLE_FILE))
        create_exemple()
        exit()

    # check the delimiter match the expected delimiter
    csvInFile.seek(0)
    dialect = csv.Sniffer().sniff(csvInFile.read(1024))
    if dialect.delimiter != CSV_DELIMITER:
        print("Error: the CSV row delimiter doesn't match the expected delimiter '{}' (checkout the file {})".format(CSV_DELIMITER, CSV_EXEMPLE_FILE))
        create_exemple()
        exit()

    csvInFile.close()

# test if the CSV out file path specified exist
csvOutPath = os.path.dirname(os.path.abspath(csvOut))
if not Path(csvOutPath).is_dir():
    print("Error: the output file can't be created because {} is not a valid path".format(csvOutPath))
    exit()

# test if the CSV out file doesn't exist (if so ask authorisation to overwrite) 
if Path(csvOut).is_file():
    print("Warning: {} already exist do you want to overwrite it? (Yes/n)".format(csvOut))
    answ = input()
    if answ != "Yes":
        csvOut = datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d_%H:%M:%S_result.csv')
        print("The output file will be saved as {}".format(csvOut))
    else:
        print("The file will be overwritten")
        # truncate the file
        with open(csvOut, "w") as f:
            f.close

    #combine_tmp_file(csvOut)
    #exit()
try:
    testlist = csv_parser(csvIn)
    connect(HOST, PORT)
    clear_tmp_file()

    for test in testlist:
        print("Starting test {}/{} ... ".format(test["testnum"],len(testlist)), end='', flush=True)
        result = start_test(test)
        print("Done",end='',flush=True)
        save_tmp(result, test)
        print(" - Saved")
    
    motor_speed(0)
    combine_tmp_file(csvOut)
    disconnect()

except KeyboardInterrupt:
    motor_speed(0)
    disconnect()
