# 1 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
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
# 15 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
//Required libraries
# 17 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2

# 19 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 20 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 21 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 22 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 23 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 24 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 25 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 26 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 27 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2


//Debug defines




//Client request ENUMS





//Client response ENUMS




//Badge properties


//Flags for different commands
bool pingHealth;
bool clientRegistered;
volatile bool paging = false;

//MQTT connection variables/constants
const char* mqtt_server = "10.2.117.37";

# 57 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2
# 58 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino" 2

//Identity for user with password related to his realm (organization)
//Available option of anonymous identity for federation of RADIUS servers or 1st Domain RADIUS servers





//SSID NAME
const char* ssid = "eduroam"; // eduroam SSID
String badgeMACAddress = "A0:A3:B3:2D:C6:2C";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;

// esp32 hardware


//Pin definitions
const int flash_pin = 16;
const int conn_pin = 17;
const int button_pin_1 = 18;
const int button_pin_2 = 19;
const int vibrate_pin = 25;


//OLED display parameters




Adafruit_SSD1306 display(128 /* OLED display width, in pixels*/, 32 /* OLED display height, in pixels*/, &Wire, -1 /* Reset pin # (or -1 if sharing Arduino reset pin)*/);


//Housekeeping variables
long prev_time;
long last_ping, last_conn_attempt;
bool flash_state;
char outLine[50];

//Button objects
Button button_accept(button_pin_1);
Button button_refuse(button_pin_2);

void display_message(char* message){
  display.clearDisplay();
  display.setTextSize(1); // Normal 1:1 pixel scale
  display.setTextColor(1 /*|< Draw 'on' pixels*/); // Draw white text
  display.setCursor(0,0); // Start at top-left corner
  display.println(message);
  display.display();
}

void setPaging(bool setting)
{
  paging = setting;
  digitalWrite(flash_pin, setting);
}

//Run through setup features to connect to BYU's eduroam under a student account.
void setup_Wifi(){
    // We start by connecting to a WiFi network

    Serial.println();
    sprintf(outLine, "Connecting to %s", ssid);

    display_message(outLine);

    Serial.println(outLine);


  WiFi.disconnect(true); //disconnect from WiFi to set new WiFi connection
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_ANONYMOUS_IDENTITY, EAP_IDENTITY, EAP_PASSWORD, test_root_ca); //with CERTIFICATE 
  WiFi.begin(ssid, WPA2_AUTH_PEAP, "lmc82@byu.edu" /*nickname@example.com, at some organizations should work nickname only without realm, but it is not recommended*/, "lmc82@byu.edu" /* the Username is the same as the Identity in most eduroam networks.*/, "92V50g8m**" /*password for eduroam account*/); // without CERTIFICATE, RADIUS server EXCEPTION "for old devices" required


    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((".")))));
    }
    Serial.println("");
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>(("WiFi is connected!")))));
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>(("IP address set: ")))));
    Serial.println(WiFi.localIP()); //print LAN IP
    //A0:A3:B3:2D:C6:2C
    //08:D1:F9:E7:76:F4
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>(("MAC address is: ")))));
    badgeMACAddress = WiFi.macAddress();

}

/* connect_mqttServer

Setup connection with Wifi and the MQTT broker.

Subscribes to the right topics that the rasp-pi will be using to 

communicate with the client.

*/
# 156 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
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
    display_message("Attempting MQTT connection...");
    // Attempt to connect
    const char* clientID = badgeMACAddress.c_str();
    if (client.connect(clientID)) { // Change the name of client here if multiple ESP32 are connected. This should be a unique name.
      //attempt successful
      Serial.println("connected");
      display_message("Connected");
      // Subscribe to topics here
      client.subscribe(clientID);
      client.subscribe("client/global");
      digitalWrite(conn_pin, 0x1);

    }
    else {
      //attempt not successful
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 2 seconds");
      digitalWrite(conn_pin, 0x0);

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
# 198 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
void callback(char* topic, byte* message, unsigned int length) {

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");

  sprintf(outLine, "Message arrived on topic: %s", topic);
  Serial.println(outLine);
  display_message(outLine);
  String messageTemp;

  //Decoding the message
  for (int i = 0; i < length; i++) {

    Serial.print((char)message[i]);

    messageTemp += (char)message[i];
  }

  Serial.println();


  // Check if a message is received on the topic unique to the client (its MAC address)
  if ((String(topic) == badgeMACAddress) || (String(topic) == "client/global")) {
    switch (messageTemp.toInt())
    {
      //In cases when the service restarts when multiple clients 
      //are already online, we ask them to re-register
      case 0:

        Serial.println("Register request made by server");

        RegisterClient();
        break;
      case 1:

        Serial.println("Health request made by server");

        //Raise a flag to broadcast a health ping.
        pingHealth = true;
        break;

      case 2:

        Serial.println("Page request made by server");
        display_message("Page request");


        // Init Request
        setPaging(true);

        break;
      case 3:

        Serial.println("Cancel request made by server");


        // Cancel Request
        setPaging(false);

        break;
      default:

        Serial.println("Bad client command");

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
# 277 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
void RegisterClient(){
  StaticJsonDocument<80> doc;
  char output[80];

  const char* clientID = badgeMACAddress.c_str();
  uint8_t lastPing = 0;
  uint8_t status = 1;//Showing that it is not active
  uint8_t response = 0;

  doc["i"] = clientID;
  doc["p"] = lastPing;
  doc["n"] = "Logan" /* Include your name*/;
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
# 302 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
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
# 325 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
void PagerRequest(){
  //Check if an accept/deny button was pushed
}

/*setup()

Initialize values and client functions.

*/
# 332 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
void setup() {
  delay(100);
  Serial.begin(115200);

  digitalWrite(conn_pin, 0x0);
  pinMode(flash_pin, 0x03);
  pinMode(conn_pin, 0x03);
  pinMode(vibrate_pin, 0x03);
  button_accept.begin();
  button_refuse.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(0x02 /*|< Gen. display voltage from 3.3V*/, 0x3C /*|< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32*/)) {
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>(("SSD1306 allocation failed")))));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(1 /*|< Draw 'on' pixels*/);

  setup_Wifi();

  client.setServer(mqtt_server, 1883); //Set IP and port for mqtt server

  client.setCallback(callback); //Set callback function to our defined function

  //Setup default variables (flags)
  pingHealth = false;
  clientRegistered = false;

  flash_state = false;
  prev_time = millis();

}

/*loop()

Handlles

- Reconnection to mqtt server

- waiting for user input w/ buttons in response to pager requests

- visual feedback for paging (flashing LED)

*/
# 374 "C:\\Users\\logan\\Desktop\\Repos\\patron-handler\\MQTT_Version\\tech_badge\\tech_badge.ino"
void loop() {
  Serial.println(button_accept.read());
  digitalWrite(vibrate_pin, 0x0);
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
      display_message("Accept");

      StaticJsonDocument<80> doc;
      char output[80];

      const char* clientID = badgeMACAddress.c_str();
      uint8_t response = 1;

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
      display_message("Refuse");
      StaticJsonDocument<80> doc;
      char output[80];

      const char* clientID = badgeMACAddress.c_str();
      uint8_t response = 2;

      doc["i"] = clientID;
      doc["r"] = response;

      serializeJson(doc, output);

      // Publishing to the sensor 1 topic
      client.publish("server/pager", output); //topic name (to which this ESP32 publishes its data)

      setPaging(false);
    }

    if (current_time - prev_time > 100)
    {
      Serial.println("FLASH");
      Serial.println(String(button_refuse.read()));
      prev_time = current_time;
      flash_state = !flash_state;
      digitalWrite(flash_pin, flash_state);
      digitalWrite(vibrate_pin, flash_state);
    }
  }
}