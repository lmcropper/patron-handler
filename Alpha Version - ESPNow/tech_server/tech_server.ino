// MAC Address Server
// 08:D1:F9:35:27:14

#include "esp_now.h"
#include <WiFi.h>
#include "Button.h"
#include "LED.h"

#define BLINK_RATE_MS 100
#define WAIT_TIME_MS 5000
#define ERROR_BLINK_MS 1000
#define STATUS_DISPLAY_MS 2000
#define MESSAGE_ATTEMPTS 3
#define MAX_RECEIVERS 20
#define INACTIVE_PING_THRESH 3000
#define DEAD_PING_THRESH 10000

#define CALL_CODE 0xF6
#define START_BYTE 40

//#define DEBUG_PACKETS
//#define DEBUG_SERIAL

// Pin Numbers
const int red_led_pin = 18;
const int yellow_led_pin = 22;
const int green_led_pin = 19;
const int button_pin = 23;

// Objects
LED redLed(red_led_pin);
LED yellowLed(yellow_led_pin);
LED greenLed(green_led_pin);
Button button_call(button_pin);
esp_now_peer_info_t peerInfo[2];

// State Variables
volatile enum { IDLE, CALLING, NO_HELP, ACKED, REFUSED } server_state = IDLE;
uint8_t receivedCommand;
uint8_t receiver_trace_ndx, last_receiver_addr;
uint8_t attempted_receivers, paged_receivers;
uint8_t health_check_device_flag = 255;
long delayCounter = 0;
volatile bool callRefusedFlag, callAckFlag;
volatile bool lastSentResolved = false;
volatile esp_now_send_status_t lastSentStatus;
String getMacString(uint8_t*);

// Timing
long currentTime, prevTime, deltaTime;

// Device Addresses
#define COMMAND_PAGE    1
#define COMMAND_CANCEL  2
#define COMMAND_ACCEPT  3
#define COMMAND_REFUSE  4
#define COMMAND_CONN    5
uint8_t broadcastAddress[MAX_RECEIVERS][6];
uint32_t last_ping[MAX_RECEIVERS];
uint8_t registered_receivers = 0;

// Helpful Functions
void write_yellow_led(bool);
void write_red_led(bool);
void write_green_led(bool);
void OnDataSent(const uint8_t*, esp_now_send_status_t);
void OnDataRecv(const uint8_t*, const uint8_t*, int);
bool send_command(uint8_t, int, int);
uint8_t registerNewReceiver(const uint8_t*);
bool callNextReceiver();

// Communication
void update_tech_status(uint8_t, uint8_t);
void send_frontend_serial_packet(uint8_t*, uint8_t);
void send_frontend_byte(uint8_t c);

void setup() {
  
  // Start Serial Comms
  Serial.begin(115200);

  // Set state variables
  callRefusedFlag = false;
  callAckFlag = false;
  receiver_trace_ndx = 0;
  last_receiver_addr = 0;
  attempted_receivers = 0;
  paged_receivers = 0;

  redLed.on();
  yellowLed.on();
  greenLed.on();
  button_call.begin();

  // Start Wifi Communication
  WiFi.mode(WIFI_MODE_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    #ifdef SERIAL_DEBUG
    Serial.println("Error initializing ESP-NOW");
    #endif
    return;
  }

  // Register Send Callback
  esp_now_register_send_cb(OnDataSent);
  
  // Register Receive Callback
  esp_now_register_recv_cb(OnDataRecv);

  // Signal Setup complete by turning off leds
  redLed.off();
  yellowLed.off();
  greenLed.off();

  send_frontend_byte(START_BYTE);
}

void loop() {

  // Timing
  currentTime = millis();
  deltaTime = currentTime - prevTime;

  // Send health check commands out, respond to their connection requests
  if (health_check_device_flag < MAX_RECEIVERS)
  {
    send_command(health_check_device_flag, COMMAND_CONN, MESSAGE_ATTEMPTS);
    health_check_device_flag = 255;
  }
  
  // State transitions
  switch (server_state)
  {
    case IDLE:
      if (button_call.pressed() || (Serial.available() && Serial.read() == CALL_CODE))
      {
        // Start a new calling rotation
        attempted_receivers = 0;
        paged_receivers = 0;
        callRefusedFlag = true; // To surpress the initial cancel command
        callAckFlag = false;
          
        // If receivers are online
        if (registered_receivers > 0 && callNextReceiver())
        {
          server_state = CALLING;
          delayCounter = 0;
          #ifdef SERIAL_DEBUG
          Serial.println("Entering CALLING");
          #endif

          // Blink the yellow led
          yellowLed.on();
          redLed.off();
          greenLed.off();
        }
        // If no receivers are online
        else
        {
          server_state = NO_HELP;
          delayCounter = 0;
          #ifdef SERIAL_DEBUG
          Serial.println("Entering NO_HELP");
          #endif

          // Blink all the leds
          yellowLed.on();
          redLed.on();
          greenLed.on();
        }
      }
      break;

    case CALLING:
      // Call Accepted! :)
      if (callAckFlag)
      {
        server_state = ACKED;
        delayCounter = 0;
        #ifdef SERIAL_DEBUG
        Serial.println("Accepted --> Entering ACKED");
        #endif

        // Log the accepted state on the front end
        update_tech_status(last_receiver_addr, 4);

        // Release Front end
        send_frontend_byte(20);

        // Turn on the green led
        greenLed.on();
        redLed.off();
        yellowLed.off();
      }
      // Timer expires or pager went dead
      else if (delayCounter >= WAIT_TIME_MS || callRefusedFlag || last_ping[last_receiver_addr] > INACTIVE_PING_THRESH)
      {
        // Log the rejected state on the front end if still online
        if (last_ping[last_receiver_addr] < INACTIVE_PING_THRESH)
          update_tech_status(last_receiver_addr, 5);
          
        // If theres someone else
        if (callNextReceiver())
        {
          delayCounter = 0;
          #ifdef SERIAL_DEBUG
          Serial.println("Refused/Expired --> Restart CALLING");
          #endif

          // Blink the yellow led
          yellowLed.on();
          redLed.off();
          greenLed.off();
        }
        // Nobody answered/everyone refused :(
        else
        {
          server_state = REFUSED;
          delayCounter = 0;
          #ifdef SERIAL_DEBUG
          Serial.println("Entering REFUSED");
          Serial.print("Paged ");
          Serial.print(paged_receivers);
          Serial.print(" devices, all refused/timed out ");
          Serial.print(" (");
          Serial.print(registered_receivers);
          Serial.println(" registered)");
          #endif

          // Release Front end
          send_frontend_byte(20);

          // Inc so the same person isn't paged first again
          receiver_trace_ndx = (receiver_trace_ndx + 1) % registered_receivers;     

          // Turn on the red led
          redLed.on();
          greenLed.off();
          yellowLed.off();
        }
        
      }
      break;

    case NO_HELP:
      if (delayCounter >= ERROR_BLINK_MS)
      {
        server_state = IDLE;
        #ifdef SERIAL_DEBUG
        Serial.println("Back to IDLE");
        #endif

        // Release Front end
        send_frontend_byte(20);

        // Turn off all leds
        yellowLed.off();
        redLed.off();
        greenLed.off();
      }
      break;

    case ACKED:
    case REFUSED:
      if (delayCounter >= STATUS_DISPLAY_MS)
      {
        server_state = IDLE;
        #ifdef SERIAL_DEBUG
        Serial.println("Back to IDLE");
        #endif

        // Turn off all leds
        yellowLed.off();
        redLed.off();
        greenLed.off();
      }
      break;

    default:
      #ifdef SERIAL_DEBUG
      Serial.println("Bad State Error");
      #endif
      break;
  }

  // State actions
  switch (server_state)
  {
    case IDLE:
      break;

    case CALLING:
    case NO_HELP:
    case ACKED:
    case REFUSED:
      delayCounter += deltaTime;
      break;
  }

  // Timing
  prevTime = currentTime;

  // Update the ping table
  for (uint8_t i = 0; i < registered_receivers; i++)
  {
    last_ping[i] += deltaTime;
    if (last_ping[i] >= INACTIVE_PING_THRESH && last_ping[i] - deltaTime < INACTIVE_PING_THRESH)
    {
      // Log the unavailable state on the front end
      update_tech_status(i, 1);
    }
    else if (last_ping[i] >= DEAD_PING_THRESH && last_ping[i] - deltaTime < DEAD_PING_THRESH)
    {
      // Log the error state on the front end
      update_tech_status(i, 0);
    }
  }
  
}


// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  lastSentResolved = true;
  lastSentStatus = status;
  #ifdef DEBUG_PACKETS
  Serial.print("Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  #endif
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  #ifdef DEBUG_PACKETS
  Serial.print("Bytes received: "); 
  Serial.println(len);
  #endif

  // The payload is a mac address
  if (len == 6)
  {
    // Place it in the next index in the array and inc tracker
    uint8_t address_buffer[6];
    memcpy(address_buffer, incomingData, 6);
    health_check_device_flag = registerNewReceiver(address_buffer);

    #ifdef DEBUG_PACKETS
    Serial.println("Recieved Mac Address");
    Serial.println(getMacString(address_buffer));
    #endif
  }
  else
  {
    // Log it into the receivedCommand variable
    memcpy(&receivedCommand, incomingData, sizeof(receivedCommand));

    // Accept Command
    if (receivedCommand == COMMAND_ACCEPT) callAckFlag = true;
    // Refuse Command
    else if (receivedCommand == COMMAND_REFUSE) callRefusedFlag = true;

    #ifdef DEBUG_PACKETS
    Serial.print("Received Byte Code: ");
    Serial.println(receivedCommand);
    #endif
  }
}


bool send_command(uint8_t device_id, int command, int attempts)
{
  #ifdef DEBUG_PACKETS
  Serial.print("Sending command ");
  Serial.print(command);
  Serial.print(" to peer ");
  Serial.print(device_id);
  Serial.print(" @ ");
  Serial.println(getMacString(broadcastAddress[device_id]));
  #endif
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress[device_id], (uint8_t *) &command, sizeof(command));
  lastSentResolved = false;

  // Repeat if the packet doesn't actually send
  if (result == ESP_OK) {
    #ifdef DEBUG_PACKETS
    Serial.println("Sent with success");
    #endif
  }
  else {
    #ifdef DEBUG_PACKETS
    Serial.println("Error sending the data, retrying...");
    #endif
    if (attempts > 1)
    {
      return send_command(device_id, command, attempts - 1);
    } else return false;
  }

  // Repeat if the packet doesn't actually deliever
  while (!lastSentResolved); // Wait until the callback fires
  if (lastSentStatus == ESP_NOW_SEND_SUCCESS) return true;
  else {
    if (attempts > 1)
    {
      return send_command(device_id, command, attempts - 1);
    } else return false;
  }
}

uint8_t registerNewReceiver(const uint8_t* macAddr)
{

  // Record the new address
  memcpy(&(broadcastAddress[registered_receivers]), macAddr, 6);

  // Check for repeat addresses
  for (uint8_t i = 0; i < registered_receivers; i++)
  {
    uint8_t matched_cnt = 0;
    for (uint8_t j = 0; j < 6; j++)
    {
      if (broadcastAddress[i][j] == macAddr[j]) matched_cnt++;
      else break;
    }
    if (matched_cnt == 6)
    {
      #ifdef DEBUG_PACKETS
      Serial.println("Already Registered");
      #endif

      // Update the ping time
      if (last_ping[i] > INACTIVE_PING_THRESH)
      {
        // Update to available state on the front end
        update_tech_status(i, 2);
      }
      // Reset
      last_ping[i] = 0;
      return i;
    }
  }
  
  // Register peer 1
  memcpy(peerInfo[registered_receivers].peer_addr, broadcastAddress[registered_receivers], 6);
  peerInfo[registered_receivers].channel = 0;  
  peerInfo[registered_receivers].encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&(peerInfo[registered_receivers])) != ESP_OK){
    #ifdef SERIAL_DEBUG
    Serial.println("Failed to add peer");
    #endif
    return 255;
  }
  else
  {
    #ifdef SERIAL_DEBUG
    Serial.print("Registered new peer #");
    Serial.println(registered_receivers);
    #endif

    // Report back to front end
    uint8_t out_buf[] = {30, registered_receivers, 'T', 'E', 'C', 'H', '_', (char)('0' + registered_receivers)};
    send_frontend_serial_packet(out_buf, 8);
    
  }

  // Increment the registered reciever count
  return registered_receivers++;
}

bool callNextReceiver()
{
  // Stop the receiver from calling if it was never refuseed
  if (!callRefusedFlag) send_command(last_receiver_addr, COMMAND_CANCEL, MESSAGE_ATTEMPTS);
  
  // Reset flags
  callRefusedFlag = false;
  callAckFlag = false;
  
  // Abort if nobody is left in the chain
  if (attempted_receivers == registered_receivers) return false;
  
  // Send Page Command to receiver
  #ifdef SERIAL_DEBUG
  Serial.print("Calling peer ");
  Serial.println(receiver_trace_ndx);
  #endif
  bool received = send_command(receiver_trace_ndx, COMMAND_PAGE, MESSAGE_ATTEMPTS);
  
  if (received)
  {
    paged_receivers++;

    // Log the calling state on the front end
    update_tech_status(receiver_trace_ndx, 3);
  }
  else
  {
    // Log the unavailable state on the front end
    update_tech_status(receiver_trace_ndx, 1);
    
    #ifdef SERIAL_DEBUG
    Serial.println("Peer cannot be reached");
    #endif
  }

  // Move to next receiver in the line up
  last_receiver_addr = receiver_trace_ndx;
  receiver_trace_ndx = (receiver_trace_ndx + 1) % registered_receivers;
  attempted_receivers++;

  // Call the next receiver if this one fails
  // If theres none left to call return false
  if (!received && attempted_receivers < registered_receivers) return callNextReceiver();
  else if (!received) return false;
  else return true;
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

void update_tech_status(uint8_t tech_index, uint8_t new_state)
{
  uint8_t out_buf[] = {10, tech_index, new_state};
  send_frontend_serial_packet(out_buf, 3);
}

void send_frontend_serial_packet(uint8_t* payload, uint8_t byte_cnt)
{
  Serial.write("ELC"); // Denote the start of a comm packet
  Serial.write(byte_cnt); // How many bytes to read
  Serial.write(payload, byte_cnt);
}

void send_frontend_byte(uint8_t c)
{
  Serial.write("ELC"); // Denote the start of a comm packet
  Serial.write(1); // How many bytes to read
  Serial.write(c);
}
