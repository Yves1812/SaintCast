
/* SmartHome application developped by Yves Bonnefont to monitor fuel level using MQTT protocol
2018-04 V1.
*/

#include <SPI.h>
#include <Ethernet.h>
//#include <HttpClient.h>
#include <PubSubClient.h>

#define debug true

// Pin which we're monitoring
#define trigPin 7
#define echoPin 8
#define CLIENT_NAME "Cuve_fioul"

// assign a MAC address for the ethernet controller.
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x31, 0x42, 0x67};
  
#define mqtt_server "192.168.0.50"
//#define mqtt_user "guest"  //s'il a été configuré sur Mosquitto
//#define mqtt_password "guest" //idem

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff_payload[100];

// manages periodic sent without using delay
unsigned long last_sent=0;
int send_period=180; // in seconds

// Header of callback function
void callback(char* topic, byte* payload, unsigned int length);

// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192,168,1,199);
// initialize the library instance:
EthernetClient client;
//PubSubClient client(espClient);
PubSubClient MQTTclient(mqtt_server, 1883, callback, client);

// Déclenche les actions à la réception d'un message
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  if ( debug ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
  }
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff_payload[i] = payload[i];
  }
  message_buff_payload[i] = '\0';
  
  String msgString = String(message_buff_payload);
  if ( debug ) {
    Serial.println("Payload: " + msgString);
  }
// Any actions other than printing the message to stdout to be inserted here  
}

void MQTTreconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!MQTTclient.connected()) {
    Serial.print("Connexion au serveur MQTT...");
//    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
    if (MQTTclient.connect(CLIENT_NAME)) {
      Serial.print("Connected as ");
      Serial.println(CLIENT_NAME);
    } else {
      Serial.print("KO, erreur : ");
      Serial.print(MQTTclient.state());
      Serial.println(" On attend 5 secondes avant de recommencer");
      delay(5000);
    }
  }
  MQTTclient.subscribe("Cuve_fioul");
}

void sendMQTT(char *topic, float payload)
{
  char message_buffer[20];
  char* message;
  
  message=dtostrf(payload,5,1, message_buffer); //  
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);  
  Serial.println(MQTTclient.publish(topic,message));
}


void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  /* start the Ethernet connection in loop to correct for hardware disconnections*/
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // DHCP failed, so use a fixed IP address:
    Ethernet.begin(mac, ip);
  }
  delay(1500);
  Serial.println(Ethernet.localIP());

  MQTTclient.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
  MQTTclient.setCallback(callback);  //La fonction de callback qui est executée à chaque réception de message
  MQTTreconnect();
  MQTTclient.loop();
}

void loop() {
  float temp;
  long duration;
  float distance;

  if (millis()-last_sent > send_period*1000)
  {
    // Reconnect to MQTT broker and re-subscribe
    if (!client.connected()) {
      MQTTreconnect();
    }

    //read hauteur fioul
    digitalWrite(trigPin, LOW);  // Added this line
    delayMicroseconds(2); // Added this line
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10); // Added this line
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = (duration/2) / 29.1;
    sendMQTT("Niveau_fuel",distance);
      
    Ethernet.maintain();
    last_sent=millis();
  }
  if (last_sent>millis()){ // means millis has been reset => reset last_sent
    last_sent=millis();
  }
  MQTTclient.loop();
}
