
/* Teleinfo simplifié à partir du programme de Cartes électroniques
Un seul compteur
Compteur monophasé
Suppression horloge
Suppression écriture carte SD
Abonnement base uniquement

Pour démarrer
* enlever cavalier pour déconnecter le flux téléinfo de la liaison série
* charger le sketch
* remettre le cavalier pour connecter le téléinfo
lecture du stream série avec putty or aloke en 1200bauds 7 bits data parité paire

Algo
Initialisation
Lit les caractères sur la ligne série un par un et les met dans un buffer de 21 caractères
  Si fin de ligne détectée Lance le traintement de la ligne
    Lors du premier cycle, Traitement de la ligne vérifie que la ligne est en tarif de base et en monophasé (check supprimé)
    Une fois que cela est confirmé, lors des cycles suivants, on extrait les valeurs
    Si la puissance apparente n'est pas fournie par le compteur on la calcule
Qd les 3 valeurs (IINST, PAPP et INDEX) sont dispos et au plus toutes les WriteFrequency, on sauvegarde sur Xively
*/

#include <SPI.h>
#include <Ethernet.h>
#include <DHT.h>
#include <PubSubClient.h>

// assign a MAC address for the ethernet controller.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// fill in your address here:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x9E, 0x01};

// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192,168,0,19);
IPAddress MQTTserver(192, 168, 0, 50);
// initialize the library instance:
EthernetClient ethClient;
PubSubClient MQTTclient(ethClient);

// #define echo_USB            //envoie toutes les trames téléinfo sur l'USB
#define message_système_USB //envoie des messages sur l'USB (init SD, heure au demarrage, et echo des erreures)
#define debtrame 0x02
#define debligne 0x0A
#define finligne 0x0D


// *************** déclaration pins ******
#define LEC_CPT1 5  // lecture compteur 1 
#define LEC_CPT2 6  // lecture compteur 2
#define ONEWIRE 7 // bus DHT


DHT dht_RJ(ONEWIRE, DHT22);


// *************** déclaration variables globales ******
  char buffteleinfo[21] = "";
  byte bufflen = 0;

  long lastWrite = 0;
  #define writeInterval 240000 //durée entre 2 enregistrement en milliseconde

//  byte num_abo = 0; // Option tarif non intialisée
//  byte type_mono_tri = 0;
  byte presence_IINST = 0;          // indicateur que l'intensité a été mesurée au moins 2 fois
  unsigned int papp = 0;  // Puissance apparente, VA
  uint8_t IINST = 0; // Intensité Instantanée Phase 1, A  (intensité efficace instantanée) ou 1 phase en monophasé
  unsigned long INDEX1 = 0;    // Index option Tempo - Heures Creuses Jours Bleus, Wh
  float temperature=0;
  float humidite=0;
  byte donnee_ok_cpt = 0;  // pour vérifier que les donnees sont bien en memoire avant ecriture dans fichier
  byte donnee_ok_cpt_ph = 0;


///////////////////////////////////////////////////////////////////
// Calcul Checksum teleinfo
///////////////////////////////////////////////////////////////////
char chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
    for (i=1; i<(len-2); i++) sum = sum + buff[i];
    sum = (sum & 0x3F) + 0x20;
    return(sum);
}

///////////////////////////////////////////////////////////////////
// Analyse de la ligne de Teleinfo
///////////////////////////////////////////////////////////////////

void traitbuf_cpt(char *buff, uint8_t len)
{
char optarif[4]= "";    // Option tarifaire choisie: BASE => Option Base, HC.. => Option Heures Creuses, 
                        // EJP. => Option EJP, BBRx => Option Tempo [x selon contacts auxiliaires]

/*  if (num_abo == 0)
  { // vérifie le type d'abonnement
    if (strncmp("OPTARIF ", &buff[1] , 8)==0)
    {
      strncpy(optarif, &buff[9], 3);
      optarif[3]='\0';
      if (strcmp("BAS", optarif)==0)
      {
        num_abo = 1;
      }
      else Serial.print(F("Abonnement différent de base"));
    }
  }
  else
  {*/
  if (strncmp("BASE ", &buff[1] , 5)==0){
    INDEX1 = atol(&buff[6]);
    donnee_ok_cpt = donnee_ok_cpt | B00000001;
    }
//  }
/*  if (type_mono_tri == 0)
  { // Vérifie le type mono
    if (strncmp("IINST", &buff[1] , 5)==0) {
      if (strncmp("IINST ", &buff[1] , 6)==0) type_mono_tri  = 1; // monophasé
    }
  }
  else
  {*/
  if (strncmp("IINST ", &buff[1] , 6)==0){ 
      IINST = atoi(&buff[7]);
      donnee_ok_cpt_ph = donnee_ok_cpt_ph | B00000001;
      if (presence_IINST<2) presence_IINST++; // on garde la trace du fait que l'intensité a été mesurée au moins 2 fois
    }
//  } 
  
  if (strncmp("PAPP ", &buff[1] , 5)==0)
  {
      papp = atoi(&buff[6]);
      donnee_ok_cpt_ph = donnee_ok_cpt_ph | B10000000;
  }
  
  // si l'intensité a été relevé dans plus d'un cycle mais que pas de puissance apparente (PAP) alors on calcule la puissance
  if ((presence_IINST > 1) && !(donnee_ok_cpt_ph & B10000000))
  {
      //if (type_mono_tri == 1) 
      papp = IINST * 240;
      donnee_ok_cpt_ph = donnee_ok_cpt_ph | B10000000;
  }
}


///////////////////////////////////////////////////////////////////
// Lecture trame teleinfo (caractère par caractère et lance processing ligne si fin de ligne)
/////////////////////////////////////////////////////////////////// 
void read_teleinfo()
{
  byte inByte = 0 ;        // caractère entrant téléinfo
  // si une donnée est dispo sur le port série
  if (Serial.available() > 0) 
  {
    // recupère le caractère dispo
    inByte = Serial.read();
//    Serial.print(char(inByte));  // echo des trames sur l'USB (choisir entre les messages ou l'echo des trames)
    if ((inByte == debtrame) || (inByte == debligne)) bufflen = 0; // test le début de trame ou début de ligne
    buffteleinfo[bufflen] = inByte;
    bufflen++;
    if (inByte == finligne && bufflen > 5) // si Fin de ligne trouvée 
    {
      if (chksum(buffteleinfo,bufflen-1)== buffteleinfo[bufflen-2]) // Test du Checksum
      {
        traitbuf_cpt(buffteleinfo,bufflen-1); // ChekSum OK => Analyse de la Trame
      }
    }
    if (bufflen > 21) bufflen=0; // Evite le buffer overflow si un début de trame ou de ligne a été manqué
  } 
}


void sendMQTT()
{
  char message_buffer[16];
  char* message;
  
  //send value to MQTT server
  while (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTTclient.connect("arduinoRDJClient")) {
      break;
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  Serial.println("connected, publishing");
  message=dtostrf(IINST,5,1, message_buffer); //
  Serial.print("I inst:");
  Serial.println(message);  
  MQTTclient.publish("I_ins",message);
  message=dtostrf(papp,7,1, message_buffer); //
  Serial.print("P app:");
  Serial.println(message);  
  MQTTclient.publish("P_app",message);
  message=dtostrf(INDEX1,15,1, message_buffer); //
  Serial.print("Index:");
  Serial.println(message);  
  MQTTclient.publish("Index",message);
  message=dtostrf(temperature,4,1, message_buffer); //
  Serial.print("Temperature:");
  Serial.println(message);  
  MQTTclient.publish("Temp_RJ",message);
  message=dtostrf(humidite,4,1, message_buffer); //
  Serial.print("Humidite:");
  Serial.println(message);  
  MQTTclient.publish("Humi_RJ",message);
}

void affiche()
{
  Serial.print("Intentsité : ");
  Serial.println(IINST);
  Serial.print("Puissance instantanée : ");
  Serial.println(papp);
  Serial.print("Conso cumulée : ");
  Serial.println(INDEX1);
}

// ************** initialisation *******************************
void setup() 
{
 // initialisation du port 0-1 lecture Téléinfo
    Serial.begin(1200);
      // parité paire E
      // 7 bits data
    UCSR0C = B00100100;

    dht_RJ.begin();
    
#ifdef message_système_USB
    Serial.println(F("-- Teleinfo USB Arduino --"));
#endif

 // initialisation des sorties selection compteur
    pinMode(LEC_CPT1, OUTPUT);
    pinMode(LEC_CPT2, OUTPUT);
    digitalWrite(LEC_CPT1, HIGH);
    digitalWrite(LEC_CPT2, LOW);

// start the Ethernet connection: 
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // DHCP failed, so use a fixed IP address:
    Ethernet.begin(mac, ip);
  }
  MQTTclient.setServer(MQTTserver, 1883);

Serial.println("Set-up completed");
}

// ************** boucle principale *******************************

void loop()                     // Programme en boucle
{
  read_teleinfo();
  if ((millis() - lastWrite) > writeInterval) 
  {
    if ((donnee_ok_cpt == B00000001) && (donnee_ok_cpt_ph == B10000001)) // un enregistrement tout les writeINterval et si les données sont dispo
    {
      donnee_ok_cpt = B00000000;
      donnee_ok_cpt_ph = B00000000;
      //read DHT_Ext
      temperature=dht_RJ.readTemperature();
      humidite=dht_RJ.readHumidity();
      sendMQTT();
//      affiche();
      lastWrite = millis();
    }
  }
  if (lastWrite > millis())
    {lastWrite=0;}
}

