/*
esp_mqtt_client - 4/29/24

MQTT supported communication with ESP32 on BYU's eduroam network.
- Enter in your netID and password below

MQTT Communication
ESP32 tech pager (client) designed to interface with a broker (server).

When the client connects with the broker, it subscribes to a
uniquely generated token that the broker uses to communicate with
the client.
*/

#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

const char* mqtt_server = "10.2.117.37";

#include <WiFi.h> //Wifi library
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks

//Identity for user with password related to his realm (organization)
//Available option of anonymous identity for federation of RADIUS servers or 1st Domain RADIUS servers
#define EAP_ANONYMOUS_IDENTITY "anonymous@tuke.sk" //anonymous@example.com, or you can use also nickname@example.com
#define EAP_IDENTITY "tylerp16@byu.edu" //nickname@example.com, at some organizations should work nickname only without realm, but it is not recommended
#define EAP_PASSWORD "Nothing!12345" //password for eduroam account
#define EAP_USERNAME "tylerp16@byu.edu" // the Username is the same as the Identity in most eduroam networks.

//SSID NAME
const char* ssid = "eduroam"; // eduroam SSID


WiFiClient espClient;
PubSubClient client(espClient);

#define ledPin 2 //built-in LED of ESP32

long lastMsg = 0;

void setup_Wifi(){
    // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.disconnect(true);  //disconnect from WiFi to set new WiFi connection
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_ANONYMOUS_IDENTITY, EAP_IDENTITY, EAP_PASSWORD, test_root_ca); //with CERTIFICATE 
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); // without CERTIFICATE, RADIUS server EXCEPTION "for old devices" required

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println("");
  Serial.println(F("WiFi is connected!"));
  Serial.println(F("IP address set: "));
  Serial.println(WiFi.localIP()); //print LAN IP
}

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
        if (client.connect("ESP32_client1")) { // Change the name of client here if multiple ESP32 are connected. This should be a unique name.
          //attempt successful
          Serial.println("connected");
          // Subscribe to topics here
          client.subscribe("rpi/broadcast");
          //client.subscribe("rpi/xyz"); //subscribe more topics here
          
        } 
        else {
          //attempt not successful
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println(" trying again in 2 seconds");
    
          // blink_led(3,200); //blink LED three times (200ms on duration) to show that MQTT server connection attempt failed
          // Wait 2 seconds before retrying
          delay(2000);
        }
  }
  
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Check if a message is received on the topic "rpi/broadcast"
  if (String(topic) == "rpi/broadcast") {
      if(messageTemp == "10"){
        Serial.println("Action: blink LED");
        // blink_led(1,1250); //blink LED once (for 1250ms ON time)
      }
  }

  //Similarly add more if statements to check for other subscribed topics 
}

void setup() {
  delay(100);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  setup_Wifi();

  client.setServer(mqtt_server, 1883);
  
  client.setCallback(callback);
}

void loop() {
  
  if (!client.connected()) {
    connect_mqttServer();
  }

  client.loop();
  
  long now = millis();
  if (now - lastMsg > 4000) {
    lastMsg = now;

    // Publishing to the sensor 1 topic
    client.publish("esp32/sensor1", "88"); //topic name (to which this ESP32 publishes its data). 88 is the dummy value.
    
  }
  
}