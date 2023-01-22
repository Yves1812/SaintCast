import paho.mqtt.client as mqtt #import the client
import datetime, time
import RPi.GPIO as GPIO
from w1thermsensor import W1ThermSensor

class MQTT_client:
  def __init__(self, broker_address, default_topics, clientId=""):
    #print("creating new instance")
    self.default_topics=default_topics
    self.client = mqtt.Client(clientId) #create new instance
    #attach functions to callback
    self.client.on_message=self.on_message 
    self.client.on_log=self.on_log
    self.client.on_connect = self.on_connect
    self.client.on_disconnect=self.on_disconnect

  def start(self):
    print("connecting to broker")
    self.client.connect(broker_address) #connect to broker
    time.sleep(1)
    self.loop()

  def loop(self):
    self.client.loop()

  def on_connect(self, client, userdata, flags, rc):
    print("Connected with result code: ",str(rc))
    if len(self.default_topics) > 0 :
      for topic in self.default_topics:
          self.subscribe(topic)
          print("Subsrcibed to topic: ", topic)

  def on_disconnect(self, client, userdata, rc):
      if rc != 0:
          print("Unexpected MQTT disconnection. Will auto-reconnect")
          self.reconnect()
      else :
        print("Disconnect completed")

  def reconnect(self):
      attempts=5
      i=0
      while i<attempts :
        try:
          self.stop()
          self.start()
          print("reconnection after connection lost successfull")
          i=1000
        except:
          print("Reconnection failed, will retry in 2mn")
          i=i+1
          time.sleep(120)
      if (i==attempts):
        print("Failed to reconnect")


  def on_message(self,client, userdata, message):
    global period
    payload_str=str(message.payload.decode("utf-8"))
    topic_str=message.topic
    print("message received " ,topic_str, " :",payload_str, " @", datetime.datetime.now())
#    print("message qos=",message.qos)
#    print("message retain flag=",message.retain)
    # if message.topic=="Appentis/Orders/Circuit_eau" :
      # if payload_str=="1.0":
        # GPIO.output(CIRCUIT_EAU, True)
        # print("Circulation d'eau allumée")
      # else :
        # GPIO.output(CIRCUIT_EAU, False)
        # print("Circulation d'eau éteinte")
    # elif message.topic=="Appentis/Orders/VMC_appentis" :
      # if payload_str=="1.0":
        # GPIO.output(VMC_APPENTIS, True)
        # print("VMC allumée")
      # else :
        # GPIO.output(VMC_APPENTIS, False)
        # print("VMC éteinte")
    # elif message.topic=="Appentis/Orders/Send_period" :
      # period=int(payload_str)
      # print("Publishing period set to:", str(period))
    # else :
      # print("Not a known order message")

  def on_log(self, client, userdata, level, buf):
    if LOG == "ON" :
        print("log: ",buf)
        print("Log received à: ", datetime.datetime.now())
    else :
        pass

  def subscribe(self,topic):
    #print("Subscribing to topic", topic)
    self.client.subscribe(topic)

  def publish(self, topic, message):
    #print("Publishing message to topic",topic)
    messageInfo=self.client.publish(topic,message,1)
    if (messageInfo.rc != MQTT_ERR_SUCCESS) :
        self.reconnect()

  def stop(self):
    self.client.loop_stop() #stop the loop
    self.client.disconnect()

def PostOneWireData(sensor_id, MQTT_topic):
    try:
        sensor = W1ThermSensor(W1ThermSensor.THERM_SENSOR_DS18B20, sensor_id)
        temperature_in_celsius = sensor.get_temperature()
        print("Sensor %s has temperature %.2f" % (sensor.id, temperature_in_celsius))
        temperature_formated=str('{:0.1f}').format(temperature_in_celsius)
        client.publish(MQTT_topic+"Temp",temperature_formated)
        return 0
    except Exception as inst:
        print("Can not retrieve or publish sensor :", sensor_id, MQTT_topic)
        print(inst)
        return -1


# Initializing variables
# LOG = ON or OFF
LOG="OFF"

#MQTT
broker_address="192.168.0.60"

# GPIO Pins
GPIO.setmode(GPIO.BCM)

#Sensors id
temp_eau="0416c21f66ff"
temp_ext="0516c026feff"
 
# Topics
default_subscribed_topics=[]
default_subscribed_topics.append("Local_piscine/Orders/#")
publish_topic="Local_piscine/Datas/"
period=300

# Opening client and Pi Portd
client=MQTT_client(broker_address,default_subscribed_topics)

#Starting
current_time=datetime.datetime.now()
client.start()
i=0
try:
    while True:
      try :
        if (datetime.datetime.now()-current_time) > datetime.timedelta(seconds=period):
          current_time=datetime.datetime.now()
          print("Publishing...")
          client.publish(publish_topic+"Keep_alive", current_time.strftime("%d/%m/%y %H:%M:%S"))
          print("Published keep alive", current_time)
          PostOneWireData(temp_ext, publish_topic+"Ext/")
          PostOneWireData(temp_int, publish_topic+"Int/")
        time.sleep(5)
        client.loop()
        i=i+1
        if (i>1000) :
          client.stop()
          time.sleep(1)
          client.start()
          print("Periodic Connection reset completed")
          i=0
      except Exception as exception :
        print("Network error, connexion lost, trying to reconnect ...")        
        client.reconnect()
except KeyboardInterrupt:
    pass
finally :
    client.stop()

