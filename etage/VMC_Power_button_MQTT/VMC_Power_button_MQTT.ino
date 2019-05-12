 /* Management of power and VMC
 *  Written by Yves Bonnefont August 2016
 *  V1.0
 *  V1.01 added force power to on in set-up so that in case server is down, power will be set to on at least for Etage
 *  V2.0 re-write to use MQTT protocol
 *  V2.1 Added multiple in-loop mechanism to increase reliability (MQTT reconnect/resubscribe, send Keep Amive, DHCP regular renewal)
 */

#include <Ethernet.h>
#include <PubSubClient.h>


#define VMC_pin 8
#define master_power_pin 9
#define led_pin 7
#define SD_pin 4

int button_pin = A0;

byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x9E, 0x41 };   //physical mac address
byte ip[] = { 192, 168, 0, 178 };                      // ip in lan (that's what you need to use in your browser. ("192.168.1.178")
byte gateway[] = { 192, 168, 0, 1 };                   // internet access via router
byte subnet[] = { 255, 255, 255, 0 };                  //subnet mask

#define mqtt_server "192.168.0.50"
//#define mqtt_user "guest"  //s'il a été configuré sur Mosquitto
//#define mqtt_password "guest" //idem

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff_payload[100];

// Header of callback function
void callback(char* topic, byte* payload, unsigned int length);

EthernetClient client;
//PubSubClient client(espClient);
PubSubClient MQTTclient(mqtt_server, 1883, callback, client);


byte home_server_ip[]={ 192, 168, 0, 50 };
int count=0, time_window=100, inactivity_time=1000;
unsigned long time_stamp_first_detection, i=0;

void MQTTreconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!MQTTclient.connected()) {
    Serial.print("Connexion au serveur MQTT...");
    if (MQTTclient.connect("Etage")) {
      Serial.println("Connected as Etage");
    } else {
      Serial.print("KO, erreur : ");
      Serial.print(MQTTclient.state());
      Serial.println(" On attend 5 secondes avant de recommencer");
      delay(5000);
    }
  }
  MQTTclient.subscribe("power_haut");
  MQTTclient.subscribe("VMC");
  MQTTclient.subscribe("time_update");

}

void sendMQTT(char *topic, float payload)
{
  char message_buffer[20];
  char* message;
  
  if (!MQTTclient.connected())
  {
    MQTTreconnect();
  }
  message=dtostrf(payload,5,1, message_buffer); //  
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);  
  Serial.println(MQTTclient.publish(topic,message));
}

// Déclenche les actions à la réception d'un message
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;

  Serial.println("Message recu =>  topic: " + String(topic));
  Serial.print(" | longueur: " + String(length,DEC));
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff_payload[i] = payload[i];
  }
  message_buff_payload[i] = '\0';
  
  String msgString = String(message_buff_payload);
  Serial.println("Payload: " + msgString);
  if (String(topic).equals("VMC")){
    if ( msgString == "1.0" ) {
      digitalWrite(VMC_pin,HIGH);
    } else {
      digitalWrite(VMC_pin,LOW);
    }
  }
  if (String(topic).equals("power_haut")){
    if ( msgString == "1.0" ) {
      digitalWrite(master_power_pin,LOW);
      digitalWrite(led_pin, HIGH);
      Serial.println("Power off, Led low");
    } else {
      digitalWrite(master_power_pin,HIGH);
      digitalWrite(led_pin, LOW);
      Serial.println("Power off, Led low");
    }
  }
}


void setup() {
// Open serial communications
  Serial.begin(9600);

// Define pin out
  pinMode(master_power_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(VMC_pin, OUTPUT);
  pinMode(SD_pin, OUTPUT);

  digitalWrite(SD_pin, HIGH);
  digitalWrite(master_power_pin, LOW);
  digitalWrite(led_pin, HIGH);
   
  time_stamp_first_detection=millis();
  
  /* start the Ethernet connection in loop to correct for hardware disconnections*/
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // DHCP failed, so use a fixed IP address:
    Ethernet.begin(mac, ip);
  }
  delay(500);
  Serial.println(Ethernet.localIP());

  MQTTreconnect();
  MQTTclient.loop();
}


void loop() {
  int val;
    
  // Read button
  if ((count!=0) && (millis()-time_stamp_first_detection > inactivity_time)) {
    count=0;
  }
  val = analogRead(0);    // read the input pin
  if (val<512){
    if (count==0) {time_stamp_first_detection=millis();}
    count=count+1;
    if (count==3 && millis()-time_stamp_first_detection < time_window){
      // if button was pressed send Get update to OpenHab Power_push_button:
      sendMQTT("power_button_pushed",1.0);
    }
  }    
  //Serial.println(val);             // debug value
  i++;
  if (i%1000==0)
  {
    MQTTclient.loop();
  }
  //Serial.println(i);

  if(i%1000000==0 && count==0)
  {
    sendMQTT("power_button_keepalive",1.0);
  }
  if(i%100000==0)
  {
    Serial.print("MQTT client state: ");
    Serial.println(MQTTclient.state());
  }
  if (i>200000000 && count==0){
      /* start the Ethernet connection in loop to correct for hardware disconnections*/
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Ethernet using DHCP");
      // DHCP failed, so use a fixed IP address:
      Ethernet.begin(mac, ip);
    }
    delay(500);
    MQTTreconnect();
    i=0;
    Serial.println("Renewed ethernet and MQTT connexions");

  }
}
