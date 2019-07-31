
/* SmartHome application developped by Yves Bonnefont to monitor fuel level using MQTT protocol
  2018-04 V1
  2019-05 added alternative wifi
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define debug true

#define wifi_ssid_preferred "Penn_Ty_Breizh"
#define wifi_password_preferred "marie-pascale"
#define wifi_ssid_alternative "Home_Etage"
#define wifi_password_alternative "mariepascale"


#define MAX_TRIES 50

// Pin which we're using
#define trigPin 4
#define echoPin 13
#define CLIENT_NAME "Cuve_fioul"
#define mqtt_server "192.168.0.50"

// wifi used
char active_wifi_ssid[] = wifi_ssid_preferred;
char active_wifi_password[] = wifi_password_preferred;

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff_payload[100];

// manages periodic sent without using delay
unsigned long last_sent = 0;
int send_period = 60; // in seconds

// Header of callback function
void callback(char* topic, byte* payload, unsigned int length);

//Création des objets
WiFiClient espClient;
PubSubClient MQTTclient(mqtt_server, 1883, callback, espClient);

//Connexion au réseau WiFi
int connect_wifi(){
  int tries = 0;
  delay(10);
  Serial.println();
  Serial.print("Connexion a ");
  Serial.print(active_wifi_ssid);

  // attempt to connect to Wifi network:
  WiFi.begin(active_wifi_ssid, active_wifi_password);
  while (WiFi.status() != WL_CONNECTED  && tries < MAX_TRIES) {
    delay(250);
    tries = tries + 1;
    Serial.print(".");
  }

  Serial.println("");
  if (tries < MAX_TRIES) {
    Serial.println("Connexion WiFi etablie ");
    Serial.print("=> Addresse IP : ");
    Serial.println(WiFi.localIP());
    return 0;
  }
  else
  {
    Serial.print("Connexion a ");
    Serial.print(active_wifi_ssid);
    Serial.println(" a echoue.");
    return -1;
  }
}

void setup_wifi() {
  if (connect_wifi()!=0){
    if (strcmp(active_wifi_ssid, wifi_ssid_preferred)==0){
      strcpy(active_wifi_ssid, wifi_ssid_alternative);
      strcpy(active_wifi_password, wifi_password_alternative);
    }
    else
    {
      strcpy(active_wifi_ssid, wifi_ssid_preferred);
      strcpy(active_wifi_password, wifi_password_preferred);
    }
    if (connect_wifi()!=0){
      Serial.println("Failed to connect to both preferred and alternative WiFi");
    }
  }
}

//Connexion - Reconnexion MQTT
void MQTTreconnect() {
  int tries = 0;
  //Boucle jusqu'à obtenir une reconnexion
  Serial.print("Connexion au serveur MQTT...");
  while (!MQTTclient.connected() && tries < 5) {
    Serial.print(".");
    if (!MQTTclient.connect(CLIENT_NAME)) {
      tries = tries + 1;
      delay(1000);
    }
  }
  if (tries < 5) {
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
    Serial.print(" | longueur: " + String(length, DEC));
  }
  // create character buffer with ending null terminator (string)
  for (i = 0; i < length; i++) {
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

  message = dtostrf(payload, 3, 1, message_buffer); //
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
  Serial.println(MQTTclient.publish(topic, message));
}


void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  /* start the Ethernet connection in loop to correct for hardware disconnections*/
  setup_wifi();           //On se connecte au réseau wifi
  delay(1500);

  MQTTclient.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
  MQTTclient.setCallback(callback);  //La fonction de callback qui est executée à chaque réception de message
  MQTTreconnect();
  MQTTclient.loop();
}

void loop() {
  float temp;
  long duration;
  float distance;

  if (millis() - last_sent > send_period * 1000)
  {
    // reconnect to wifi if needed
    if (WiFi.status() != WL_CONNECTED) {
      setup_wifi();
    }
    // Reconnect to MQTT broker and re-subscribe
    if (!MQTTclient.connected()) {
      MQTTreconnect();
    }

    //read hauteur fioul
    distance=0;
    for (int i = 0; i <= 3; i++){
       digitalWrite(trigPin, LOW);  // Added this line
       delayMicroseconds(2); // Added this line
       digitalWrite(trigPin, HIGH);
       delayMicroseconds(10); // Added this line
       digitalWrite(trigPin, LOW);
       duration = pulseIn(echoPin, HIGH);
       distance += (duration / 2) / 29.1;
       delay(500);
    }
    distance=distance/4.0;
    sendMQTT("Niveau_fuel", distance);

    last_sent = millis();
  }
  if (last_sent > millis()) { // means millis has been reset => reset last_sent
    last_sent = millis();
  }
  MQTTclient.loop();
}
