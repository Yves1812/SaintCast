#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>

#ifndef STASSID
#define STASSID "Home_Etage"
#define STAPSK  "mariepascale"
//#define STASSID "Penn_Ty_Breizh"
//#define STAPSK  "marie-pascale"

#endif

const char* ssid = STASSID;
const char* password = STAPSK;

#define debug true
#define MAX_TRIES 50

// Pin which we're using
#define trigPin 4
#define echoPin 13
#define CLIENT_NAME "Cuve_fioul"
  
#define mqtt_server "192.168.0.60"

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff_payload[100];

// manages periodic sent without using delay
unsigned long last_sent=0;
int send_period=30; // in seconds

// Header of callback function
void callback(char* topic, byte* payload, unsigned int length);

//Création des objets
WiFiClient espClient;
PubSubClient MQTTclient(mqtt_server, 1883, callback, espClient);

//Connexion - Reconnexion MQTT
void MQTTreconnect(){
  int tries=0;
  //Boucle jusqu'à obtenir une reconnexion
  Serial.print("Connexion au serveur MQTT...");
  while (!MQTTclient.connected() && tries<5) {
    Serial.print(".");
    if (!MQTTclient.connect(CLIENT_NAME)) {
      tries=tries+1;
      delay(1000);
    }
  }
  if (tries<5){
    Serial.print("Connected as ");
    Serial.println(CLIENT_NAME);
  }
  else {
    Serial.print("KO, erreur : ");
    Serial.print(MQTTclient.state());
  }
  MQTTclient.subscribe("Cuve_fioul");
}


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
// Connect to wifi and set OTA
  Serial.begin(9600);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setting up pins and MQTT
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  MQTTclient.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
  MQTTclient.setCallback(callback);  //La fonction de callback qui est executée à chaque réception de message
  MQTTreconnect();
  MQTTclient.loop();

}

void loop() {
  ArduinoOTA.handle();  float temp;
  long duration;
  float distance;

  if (millis()-last_sent > send_period*1000)
  {
    // reconnect to wifi if needed
//    if (WiFi.status() != WL_CONNECTED) {
//      setup_wifi();
//    }
    // Reconnect to MQTT broker and re-subscribe
    if (!MQTTclient.connected()) {
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
//    Serial.println(duration); // debug purpose
    sendMQTT("Niveau_fuel",distance);
    
    last_sent=millis();
  }
  if (last_sent>millis()){ // means millis has been reset => reset last_sent
    last_sent=millis();
  }
  MQTTclient.loop();
}
