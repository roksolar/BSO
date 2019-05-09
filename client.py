import paho.mqtt.client as mqtt #import the client1
import time as t
import sqlite3
import datetime


############

def on_message(client, userdata, message):
    print("dobil")
    sqlite_file = 'sensordata.db'
    # Connecting to the database file
    conn = sqlite3.connect(sqlite_file)
    c = conn.cursor()
    print("-----------------------------------------------")
    print("message recekkived " ,str(message.payload.decode("utf-8")))
    print("message topic=",message.topic)
    print("message qos=",message.qos)
    print("message retain flag=",message.retain)    
    pressure = str(message.payload.decode("utf-8"))
    device_name = message.topic

    try:
        today = datetime.datetime.now()
        c.execute('''INSERT INTO readings(pressure,currentdate,device) VALUES(?,?,?)''', (pressure,today,device_name,))
        conn.commit()
        print("KONEC")
    except Exception as e:
        print(e)
        print("BAZA")
    
########################################
#broker_address="localhost"
broker_address="test.mosquitto.org"
print("creating new instance")
client = mqtt.Client("1479") #create new instance
#client.username_pw_set("test","bso123")
client.on_message=on_message #attach function to callback
print("connecting to broker")
client.connect(broker_address) #connect to broker
client.loop_start() #start the loop
topic = "/BSO-1"
print("Subscribing to topic", topic)
client.subscribe(topic)
topic = "/BSO-2"
print("Subscribing to topic", topic)
client.subscribe(topic)

while True:
    t.sleep(1) # wait