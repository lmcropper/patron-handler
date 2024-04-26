// MAC Address Reciever
// A0:A3:B3:2D:C6:2C - 1
// 08:D1:F9:35:01:28 - 2

#define FLASH_RATE 100

#include "esp_now.h"
#include <WiFi.h>
#include "Button.h"

#define COMMAND_PAGE    1
#define COMMAND_CANCEL  2
#define COMMAND_ACCEPT  3
#define COMMAND_REFUSE  4
#define COMMAND_CONN    5

#define PING_INTERVAL_MS   1000
#define PING_TIMEOUT_MS    2000

const int flash_pin = 17;
const int conn_pin = 19;
const int button_pin_1 = 23;
const int button_pin_2 = 22;

long prev_time;
long last_ping, last_conn_attempt;
bool flash_state; 

volatile bool paging = false;

// Address of the system server
uint8_t broadcastAddress[] =  {0x08, 0xD1, 0xF9, 0x35, 0x27, 0x14};
//uint8_t broadcastAddress[] =  {0xA0, 0xA3, 0xB3, 0x2D, 0xC6, 0x2C};
uint8_t myMacAddress[6];

Button button_accept(button_pin_1);
Button button_refuse(button_pin_2);

int recievedCommand;
esp_now_peer_info_t peerInfo;

String getMacString(uint8_t*);

void togglePaging()
{
  setPaging(!paging);
}

void setPaging(bool setting)
{
  paging = setting;
  digitalWrite(flash_pin, setting);
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&recievedCommand, incomingData, sizeof(recievedCommand));
  Serial.print("Bytes received: ");
  Serial.println(len);

  // Take action
  if (recievedCommand == COMMAND_PAGE)
  {
    // Init Request
    setPaging(true);
  }
  else if (recievedCommand == COMMAND_CANCEL)
  {
    // Cancel Request
    setPaging(false);
  }
  else if (recievedCommand == COMMAND_CONN)
  {
    // Yes you are connected
    last_ping = millis();
  }

  Serial.print("Recieved Byte Code: ");
  Serial.println(recievedCommand);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);

  Serial.println("Init ESP32...");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
  pinMode(flash_pin, OUTPUT);
  pinMode(conn_pin, OUTPUT);
  button_accept.begin();
  button_refuse.begin();

  // Send your mac address to register with the server
  esp_read_mac(myMacAddress, ESP_MAC_WIFI_STA);
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)myMacAddress, 6);
  Serial.print("Sending address ");
  Serial.println(getMacString(myMacAddress));
  
  if (result == ESP_OK) {
    Serial.println("Mac Address sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }

  // No first ping
  last_ping = -PING_TIMEOUT_MS;
  
  flash_state = false;
  digitalWrite(conn_pin, LOW);
  prev_time = millis();
}

void send_command(int command)
{
  Serial.print("Sending command: ");
  Serial.println(command);
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &command, sizeof(command));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

void loop() {

  long current_time = millis();

  if (paging) {
    if (button_accept.pressed())
    {
      Serial.println("Accept");
      send_command(3);
      setPaging(false);
    }
  
    if (button_refuse.pressed())
    {
      Serial.println("Refuse");
      send_command(4);
      setPaging(false);
    }

    if (current_time - prev_time > FLASH_RATE)
    {
      prev_time = current_time;
      flash_state = !flash_state;
      digitalWrite(flash_pin, flash_state);
    }
  }

  // Send Connection Requests
  if (current_time - last_conn_attempt > PING_INTERVAL_MS)
  {
    last_conn_attempt = current_time;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)myMacAddress, 6);
    Serial.print("Checking network health...");
  
    if (result == ESP_OK) Serial.println("sent");
    else Serial.println("error sending the data");
  }

  // Handle Connection LED
  if (current_time - last_ping > PING_TIMEOUT_MS) digitalWrite(conn_pin, LOW);
  else digitalWrite(conn_pin, HIGH);
  
}

String getMacString(uint8_t* memloc)
{
  String str;
  for (int i = 0; i < 6; i++)
  {
    str += String(memloc[i], HEX);
    if (i < 5) str += ":";
  }
  return str;
}
