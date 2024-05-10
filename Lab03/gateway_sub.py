import sys
from Adafruit_IO import MQTTClient
import time
import random
import serial
from gateway_pub import *

AIO_FEED_IDs = ["nutnhan1", "nutnhan2"]
AIO_USERNAME = "kimquynh1304"
AIO_KEY = "aio_gJib69ZYqycZilN45hRtWsVBpI2l"

def connected(client):
    print("Ket noi thanh cong ...")
    # Button subcribe to feed on adafruit
    for topic in AIO_FEED_IDs:
        client.subscribe(topic)

# This function is called when sucessfully subcribe to feed on adafruit
def subscribe(client , userdata , mid , granted_qos):
    print("Subscribe thanh cong ...")

def disconnected(client):
    print("Ngat ket noi ...")
    sys.exit (1)
ser = serial.Serial()  # Define the "ser" variable


def WriteData(data):
    ser.write(str(data).encode())
    
# This function is called when gateway receive a signal from adafruit
def message(client , feed_id , payload):
    print("Nhan du lieu: " + payload, "feed_id: " + feed_id)
    if feed_id == "nutnhan1":
        if payload == "0":
            WriteData(1) # Turn off the light
            print("led off")
        else:
            WriteData(2) # Turn on the light
            print("led on")
    elif feed_id == "nutnhan2":
        if payload == "0":
            WriteData(3) # Turn off the pump
            print("pump off")
        else:
            WriteData(4) # Turn on the pump
            print("pump on")

client = MQTTClient(AIO_USERNAME , AIO_KEY)
client.on_connect = connected
client.on_disconnect = disconnected
client.on_message = message
client.on_subscribe = subscribe
client.connect()
client.loop_background()

while True:
    readSerial(client)
    time.sleep(1)
    pass