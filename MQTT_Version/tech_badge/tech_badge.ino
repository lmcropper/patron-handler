/*
tech_badge.ino - 4/29/24

MQTT supported communication with ESP32 on BYU's eduroam network.
- Enter in your netID and password below

MQTT Communication
ESP32 tech pager (client) designed to interface with a broker (server).

When the client connects with the broker, it subscribes to a
uniquely generated token that the broker uses to communicate with
the client.
*/

// Required libraries
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "esp_now.h"
#include "Button.h"


// Debug defines
#define DEBUG_WIFI
#define DEBUG_CALLBACK
#define DEBUG_COMMANDS

// Client request ENUMS
#define COMMAND_REGISTER  0
#define COMMAND_HEALTH    1
#define COMMAND_PAGE      2
#define COMMAND_CANCEL    3

// Client response ENUMS
#define RESPONSE_NONE   0
#define RESPONSE_ACCEPT 1
#define RESPONSE_DENY   2

//Badge properties
#define CLIENT_NAME "YOUR_NAME_HERE" // Include your name

bool pingHealth;
bool clientRegistered;

//MQTT connection variables/constants
const char* mqtt_server = "10.2.117.37";

#include <WiFi.h> //Wifi library
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks

//Identity for user with password related to his realm (organization)
//Available option of anonymous identity for federation of RADIUS servers or 1st Domain RADIUS servers
#define EAP_ANONYMOUS_IDENTITY "anonymous@tuke.sk" //anonymous@example.com, or you can use also nickname@example.com
#define EAP_IDENTITY "USERID@byu.edu" //nickname@example.com, at some organizations should work nickname only without realm, but it is not recommended
#define EAP_PASSWORD "BYU_PASSWORD" //password for eduroam account
#define EAP_USERNAME "USERID@byu.edu" // the Username is the same as the Identity in most eduroam networks.

//SSID NAME
const char* ssid = "eduroam"; // eduroam SSID
String badgeMACAddress = "A0:A3:B3:2D:C6:2C";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;

// esp32 hardware
#define FLASH_RATE 100

const int flash_pin = 17;
const int conn_pin = 19;
const int button_pin_1 = 23;
const int button_pin_2 = 22;

long prev_time;
long last_ping, last_conn_attempt;
bool flash_state; 

volatile bool paging = false;

Button button_accept(button_pin_1);
Button button_refuse(button_pin_2);

void setPaging(bool setting)
{
  paging = setting;
  digitalWrite(flash_pin, setting);
}

//Run through setup features to connect to BYU's eduroam under a student account.
void setup_Wifi(){
    // We start by connecting to a WiFi network
  #ifdef DEBUG_WIFI
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  #endif

  WiFi.disconnect(true);  //disconnect from WiFi to set new WiFi connection
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_ANONYMOUS_IDENTITY, EAP_IDENTITY, EAP_PASSWORD, test_root_ca); //with CERTIFICATE 
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); // without CERTIFICATE, RADIUS server EXCEPTION "for old devices" required

  #ifdef DEBUG_WIFI
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(F("."));
    }
    Serial.println("");
    Serial.println(F("WiFi is connected!"));
    Serial.println(F("IP address set: "));
    Serial.println(WiFi.localIP()); //print LAN IP
    //A0:A3:B3:2D:C6:2C
    //08:D1:F9:E7:76:F4
    Serial.println(F("MAC address is: "));
    badgeMACAddress = WiFi.macAddress();
  #endif
}

/* connect_mqttServer
Setup connection with Wifi and the MQTT broker.
Subscribes to the right topics that the rasp-pi will be using to 
communicate with the client.
*/
void connect_mqttServer() {
  // Loop until we're reconnected
  while (!client.connected()) {
    //first check if connected to wifi
    if(WiFi.status() != WL_CONNECTED){
      //if not connected, then first connect to wifi
      setup_Wifi();
    }
    

    //now attemt to connect to MQTT server
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    const char* clientID = badgeMACAddress.c_str();
    if (client.connect(clientID)) { // Change the name of client here if multiple ESP32 are connected. This should be a unique name.
      //attempt successful
      Serial.println("connected");
      // Subscribe to topics here
      client.subscribe(clientID);
      client.subscribe("client/global");
      digitalWrite(conn_pin, HIGH);
      
    } 
    else {
      //attempt not successful
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 2 seconds");
      digitalWrite(conn_pin, LOW);

      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

/* callback()
Anytime the client picks up on one of its subscribes topics,
we fire the callback function and route it to where it needs to go
based on the received topic
*/
void callback(char* topic, byte* message, unsigned int length) {
  #ifdef DEBUG_CALLBACK
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  #endif
  String messageTemp;
  
  //Decoding the message
  for (int i = 0; i < length; i++) {
    #ifdef DEBUG_CALLBACK
    Serial.print((char)message[i]);
    #endif
    messageTemp += (char)message[i];
  }
  #ifdef DEBUG_CALLBACK
  Serial.println();
  #endif

  // Check if a message is received on the topic unique to the client (its MAC address)
  if ((String(topic) == badgeMACAddress) || (String(topic) == "client/global")) {
    switch (messageTemp.toInt())
    {
      //In cases when the service restarts when multiple clients 
      //are already online, we ask them to re-register
      case COMMAND_REGISTER:
        #ifdef DEBUG_CALLBACK
        Serial.println("Register request made by server");
        #endif
        RegisterClient();
        break;
      case COMMAND_HEALTH:
        #ifdef DEBUG_CALLBACK
        Serial.println("Health request made by server");
        #endif
        //Raise a flag to broadcast a health ping.
        pingHealth = true;
        break;

      case COMMAND_PAGE:
        #ifdef DEBUG_CALLBACK
        Serial.println("Page request made by server");
        #endif
        
        // Init Request
        setPaging(true);
        
        break;
      case COMMAND_CANCEL:
        #ifdef DEBUG_CALLBACK
        Serial.println("Cancel request made by server");
        #endif

        // Cancel Request
        setPaging(false);

        break;
      default:
        #ifdef DEBUG_CALLBACK
        Serial.println("Bad client command");
        #endif
    }
  }

  //Similarly add more if statements to check for other subscribed topics 
}

/* RegisterClient()
client contacts the server (raspberrypi) with a data packet of client
characteristics.

Characteristics
clientID - unique address of the client. Used for topic creation so 
we can communicate with specific clients
*/
void RegisterClient(){
  StaticJsonDocument<80> doc;
  char output[80];

  const char* clientID = badgeMACAddress.c_str();
  uint8_t lastPing = 0;
  uint8_t status = 1;//Showing that it is not active
  uint8_t response = RESPONSE_NONE;

  doc["i"] = clientID;
  doc["p"] = lastPing;
  doc["n"] = CLIENT_NAME;
  doc["s"] = status;
  doc["r"] = response;

  serializeJson(doc, output);
    
  // Publishing to the server/register topic
  client.publish("server/register", output); //topic name (to which this ESP32 publishes its data). 88 is the dummy value.
}

/* HealthPing()
Upon request, this function sends out a reponse ping to the server (rasp-pi)
to say that it is still active.
*/
void HealthPing(){
  Serial.println("In HealthPing");
  StaticJsonDocument<80> doc;
  char output[80];

  const char* clientID = badgeMACAddress.c_str();
  uint8_t ping = 1;//Showing that it is not active
  
  doc["i"] = clientID;
  doc["p"] = ping;

  serializeJson(doc, output);
    
  // Publishing to the server/health topic
  client.publish("server/health", output); //topic name (to which this ESP32 publishes its data). 88 is the dummy value.

  pingHealth = false;
}

/*
Upon request, this function sends out a reponse ping to the server (rasp-pi)
to say that it is still active.
*/
void PagerRequest(){
  //Check if an accept/deny button was pushed
}

/*setup()
Initialize values and client functions.
*/
void setup() {
  delay(100);
  Serial.begin(115200);

  setup_Wifi();

  client.setServer(mqtt_server, 1883);
  
  client.setCallback(callback);

  //Setup default variables
  pingHealth = false;
  clientRegistered = false;

  pinMode(flash_pin, OUTPUT);
  pinMode(conn_pin, OUTPUT);
  button_accept.begin();
  button_refuse.begin();
  
  flash_state = false;
  digitalWrite(conn_pin, LOW);
  prev_time = millis();

}

/*loop()
Handlles
- Reconnection to mqtt server
- waiting for user input w/ buttons in response to pager requests
- visual feedback for paging (flashing LED)
*/
void loop() {
  
  if (!client.connected()) {
    connect_mqttServer();
  }

  client.loop();
  
  long current_time = millis();
  if (current_time - lastMsg > 1000) {
    lastMsg = current_time;

    if (!clientRegistered){
      clientRegistered = true;
      RegisterClient();
    }
  }

  if (pingHealth){
    HealthPing();
  }

  //Handle the operations of the button in response to page requests
  if (paging) {

    if (button_accept.pressed())
    {
      Serial.println("Accept");

      StaticJsonDocument<80> doc;
      char output[80];

      const char* clientID = badgeMACAddress.c_str();
      uint8_t response = RESPONSE_ACCEPT;

      doc["i"] = clientID;
      doc["r"] = response;

      serializeJson(doc, output);
        
      // Publishing to the sensor 1 topic
      client.publish("server/pager", output); //topic name (to which this ESP32 publishes its data)
      setPaging(false);
    }
  
    if (button_refuse.pressed())
    {
      Serial.println("Refuse");
      StaticJsonDocument<80> doc;
      char output[80];

      const char* clientID = badgeMACAddress.c_str();
      uint8_t response = RESPONSE_DENY;

      doc["i"] = clientID;
      doc["r"] = response;

      serializeJson(doc, output);
        
      // Publishing to the sensor 1 topic
      client.publish("server/pager", output); //topic name (to which this ESP32 publishes its data)

      setPaging(false);
    }

    if (current_time - prev_time > FLASH_RATE)
    {
      Serial.println("FLASH");
      Serial.println(String(button_refuse.read()));
      prev_time = current_time;
      flash_state = !flash_state;
      digitalWrite(flash_pin, flash_state);
    }
  }
}
