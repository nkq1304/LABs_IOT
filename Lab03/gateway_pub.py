import serial.tools.list_ports

# Function to find the port which is connected with hardware
def getPort():
    ports = serial.tools.list_ports.comports()
    N = len(ports)
    commPort = "None"
    for i in range(0, N):
        port = ports[i]
        strPort = str(port)
        if "USB Serial Device" in strPort:
            splitPort = strPort.split(" ")
            commPort = (splitPort[0])
    return "COM8"

# Open the COM port
if getPort() != "None":
    ser = serial.Serial( port=getPort(), baudrate=115200)
    print(ser)
else: 
    print("Commport not found!")

# Function to process data
def processData(client, data):
    data = data.replace("!", "")
    data = data.replace("#", "")
    splitData = data.split(":")
    print(splitData)
    if splitData[1] == "T":
        client.publish("cambien1", splitData[2])
    elif splitData[1] == "L":
        client.publish("cambien2", splitData[2])
    elif splitData[1] == "H":
        client.publish("cambien3", splitData[2])

# Function to read data from COM port
mess = ""
def readSerial(client):
    bytesToRead = ser.inWaiting()
    if (bytesToRead > 0):
        global mess
        mess = mess + ser.read(bytesToRead).decode("UTF-8")
        while ("#" in mess) and ("!" in mess):
            start = mess.find("!")
            end = mess.find("#")
            processData(client, mess[start:end + 1])
            if (end == len(mess)):
                mess = ""
            else:
                mess = mess[end+1:]