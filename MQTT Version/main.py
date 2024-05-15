#Patron Handler Service - MQTT - 4/30/24
#Setup like an embedded system

import time
import paho.mqtt.client as mqtt
import json
from enum import Enum
import threading
import queue

#Debug constants
DEBUG_STATEMACHINE = False
DEBUG_COMM = False

#MQTT Server Properties
SERVER_IP_ADDRESS = '10.2.117.37'
SERVER_IP_ADDRESS_PORT = 1883

#Event timing values
TIME_BETWEEN_PINGS = 4
CLIENT_STATUS_INACTIVE_TIME = 10
CLIENT_STATUS_OFFLINE_TIME = 20

#Client status ENUMS
CLIENT_STATUS_OFFLINE = 0
CLIENT_STATUS_INACTIVE = 1
CLIENT_STATUS_ACTIVE = 2

#Command ENUMS
COMMAND_REGISTER = 0
COMMAND_HEALTH = 1
COMMAND_PAGE = 2
COMMAND_CANCEL = 3

#Client response ENUMS
RESPONSE_NONE   = 0
RESPONSE_ACCEPT = 1
RESPONSE_DENY   = 2

#State machine variables
class ServerState(Enum):
    IDLE_STATE = 0
    CALLING_STATE = 1
    NO_HELP_STATE = 2
    ACKED_STATE = 3
    REFUSED_STATE = 4

#ServerState machine timing values
MILLIS_TO_SEC = 1000
WAIT_TIME_MS = 8000
ERROR_BLINK_MS = 1000
STATUS_DISPLAY_MS = 2000

currentState = None
button_call_pressed = False
callAckFlag = 0
callRefusedFlag = 0
attempted_receivers = 0

#Test variable we change based on the number of health requests
#Only using this until the button call functionality is implemented
BUTTON_CALL_COUNT = 1
button_call_var = 0

# Timing variables
currentTime = time.time()
previousTime = 0
deltaTime = 0

# Define as a dictionary
registered_clients = {}
client_sub = None
delayCounter = 0

flag_connected = 0

server_gui = None

# Initialize current client_id
current_client_id = None
prev_client_id = None

# Fires whever we publish on a topic
def on_publish(client, userdata, mid):
    if DEBUG_COMM:
        print("message published")

# Fires whever we connect to the MQTT server/broker
def on_connect(client, userdata, flags, rc):
    if DEBUG_COMM:
        print(f"{client} has connected to MQTT server")
   
    global flag_connected
    flag_connected = 1
    client_subscriptions(client)
   

# Fires whever we disconnect from the MQTT server/broker
def on_disconnect(client, userdata, rc):
    global flag_connected
    flag_connected = 0

    if DEBUG_COMM:
        print("Disconnected from MQTT server")

#Register the client ID in a dictionary with its information packet
def RegisterCallback(client, userdata, msg):
    if DEBUG_COMM:
        print('RPi Broadcast message:  ', str(msg.payload.decode('utf-8')))
    
    global server_gui
    client_props = json.loads(msg.payload.decode('utf-8'))
    print(client_props["i"])

    registered_clients[client_props["i"]] = client_props
    registered_clients[client_props["i"]]["p"] = time.time()
    
    #Update any visuals to show a new client has connected
    server_gui.queue.put(lambda: server_gui.update_status(registered_clients[client_props["i"]],'active'))

# HealthCallback
# Records which client is reporting back on a health update request
def HealthCallback(client, userdata, msg):
    if DEBUG_COMM:
        print('Health Callback')
    client_props = json.loads(msg.payload.decode('utf-8'))
    registered_clients[client_props["i"]]["p"] = time.time()
    registered_clients[client_props["i"]]["s"] = CLIENT_STATUS_ACTIVE

    server_gui.queue.put(lambda: server_gui.update_status(registered_clients[client_props["i"]],'active'))

# PagerCallback
# Handles page responses from clients depending on their response
def PagerCallback(client, userdata, msg):
    if DEBUG_COMM:
        print('Pager Callback')

    global callAckFlag
    global callRefusedFlag
    
    client_props = json.loads(msg.payload.decode('utf-8'))
    registered_clients[client_props["i"]]["r"] = client_props["r"]

    if (client_props["r"] == RESPONSE_ACCEPT):
        if DEBUG_COMM:
            print("client accepted")
        callAckFlag = True
        server_gui.queue.put(lambda: server_gui.update_status(registered_clients[client_props["i"]],'active'))
        server_gui.queue.put(lambda: server_gui.response_disp(True,"page_accept", registered_clients[client_props["i"]]))
    elif (client_props["r"] == RESPONSE_DENY):
        if DEBUG_COMM:
            print("client denied")
        callRefusedFlag = True

# Make the necessary subscriptions to response to what the clients are broadcasting
def client_subscriptions(client):
    client.subscribe("server/register")
    client.subscribe("server/health")
    client.subscribe("server/pager")

# Initial setup of the client and connection to the MQTT server
def setup():
    print("Setting up Patron Handler Service - RaspberryPi")

    global client_sub
    client_sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,"rpi_client1") #this should be a unique name

    #link callback events
    client_sub.on_connect = on_connect
    client_sub.on_disconnect = on_disconnect
    
    #Link callbacks
    client_sub.message_callback_add('server/register', RegisterCallback)
    client_sub.message_callback_add('server/health', HealthCallback)
    client_sub.message_callback_add('server/pager', PagerCallback)

    client_sub.on_publish = on_publish

    client_sub.connect(SERVER_IP_ADDRESS, SERVER_IP_ADDRESS_PORT)
    # start a new thread
    client_sub.loop_start()
    client_subscriptions(client_sub)

    # setup state machine
    global currentState
    currentState = ServerState.IDLE_STATE

    #TODO ask all of the clients to register with the server
    global_publish(COMMAND_REGISTER)

#pageRequestFlag
#tell the system that a page request as been made
def pageRequestFlag():
    global button_call_pressed
    button_call_pressed = True


def client_publish(client_id, command):
    if DEBUG_COMM:
        print(f"client_publish - command: {command}")
    try:
        msg = str(command)
        if (client_sub):
            pubMsg = client_sub.publish(
                topic=str(client_id),
                payload=msg.encode('utf-8'),
                qos=0,
            )
            
            pubMsg.wait_for_publish()
    except Exception as e:
        print(e)

def global_publish(command):
    if DEBUG_COMM:
        print(f"client/global publish - command: {command}")
    try:
        msg = str(command)
        if (client_sub):
            pubMsg = client_sub.publish(
                topic="client/global",
                payload=msg.encode('utf-8'),
                qos=0,
            )
            
            pubMsg.wait_for_publish()
    except Exception as e:
        print(e)

# callNextReceiver
# Sends out a pager request to the next client in the list.
# If a person is going to be out on the 
def callNextReceiver():
    global current_client_id
    global prev_client_id
    global callRefusedFlag
    global callAckFlag
    global attempted_receivers

    #Stop the receiver from calling if it was never refuseed
    if (not callRefusedFlag):
        client_publish(current_client_id, COMMAND_CANCEL)

    callRefusedFlag = False
    callAckFlag = False
        
    # Get list of keys
    client_ids = list(registered_clients.keys())

    # If current_client_id is not in list, start at the beginning
    if current_client_id not in client_ids:
        current_client_id = client_ids[0] if client_ids else None

    # If there are no clients, or we have contacted all clients, return
    if ((current_client_id is None) | (attempted_receivers == len(registered_clients))) :
        return False

    # Get current client_props
    client_props = registered_clients[current_client_id]
    
    published = False

    #TODO Account for sending multiple commands
    
    client_publish(current_client_id, COMMAND_PAGE)
    published = True


    # Find the next client_id in the list, or start at the beginning if at end
    current_index = client_ids.index(current_client_id)
    prev_client_id = current_client_id
    next_index = (current_index + 1) % len(client_ids)
    current_client_id = client_ids[next_index]
    
    attempted_receivers += 1

    # Call the next receiver if this one fails
    # If theres none left to call return false
    if((published == False) & (attempted_receivers < len(registered_clients)) ):
        return callNextReceiver()
    elif (published == False):
        server_gui.queue.put(lambda: server_gui.togglePager(False))
        return False
    else:
        return True

def toMillis(sec):
    return sec*1000

#loop through cyclical operations
def loop():
    global currentState
    global delayCounter
    global callAckFlag
    global callRefusedFlag
    global attempted_receivers

    #Handle visuals
    while not server_gui.server_message_queue.empty():
        task=server_gui.server_message_queue.get()
        task()
        
    currentTime = time.time()
    deltaTime = currentTime - previousTime

    time.sleep(.2)
    if (flag_connected != 1):
        print("trying to connect MQTT server..")

    # Loop through the registered devices in registered_client and check if we need to do
    # a health check on any of them.
    if len(registered_clients) > 0:
        for client_id in list(registered_clients.keys()):  # Create a copy of the keys
            client_props = registered_clients[client_id]
            if not isinstance(client_props, dict):
                print(f"Unexpected value for client {client_id}: {client_props}")
                continue 

            if ((currentTime - client_props['p']) > CLIENT_STATUS_OFFLINE_TIME):
                #Set status of the client to be offline
                client_props["s"] = CLIENT_STATUS_OFFLINE

                #Remove the client from the GUI interface
                server_gui.queue.put(lambda: server_gui.remove_client(client_id))
                if registered_clients[client_id]:
                    del registered_clients[client_id]
                else:
                    raise ValueError("ERROR: Could not find offline registered client")
                continue

            if ((currentTime - client_props['p']) > CLIENT_STATUS_INACTIVE_TIME):
                #Set status of the client to be inactive
                client_props["s"] = CLIENT_STATUS_INACTIVE

                #Update client button GUI
                server_gui.queue.put(lambda: server_gui.update_status(registered_clients[client_props["i"]],'inactive'))
                
                continue
            if ((currentTime - client_props['p']) > TIME_BETWEEN_PINGS):
                client_publish(client_id, COMMAND_HEALTH)
            else:
                pass #Sufficient time not yet elapsed
    else:
        print("No clients registered")

    #################
    # STATE MACHINE #
    #################

    #Start state machine to handle the paging requests
    if (currentState == ServerState.IDLE_STATE):
        if (DEBUG_STATEMACHINE): 
            print("IDLE_STATE")

        global button_call_pressed
        if (button_call_pressed):

            attempted_receivers = 0
            callRefusedFlag = True # To surpress the initial cancel command
            callAckFlag = False

            #Reset button pressed flag
            button_call_pressed = False
            
            # Start the process of calling the clients
            if ((len(registered_clients) > 0) & callNextReceiver()):
                delayCounter = time.time()
                #visual effects here to show the
                #person pressing the button that we are thinking
                #about it.
                server_gui.queue.put(lambda: server_gui.togglePager(True))
                currentState = ServerState.CALLING_STATE
            else:
                delayCounter = time.time()
                #No help visual (Only needs to be called once)
                server_gui.queue.put(lambda: server_gui.response_disp(True,"no_help"))
                currentState = ServerState.NO_HELP_STATE

    elif (currentState == ServerState.CALLING_STATE):
        if DEBUG_STATEMACHINE: 
            print("CALLING_STATE")

        # if the page has been acknowledged, we're good to go.
        if (callAckFlag):
            #Go through each of the clients to see which one has acknowledged
            #Then go through and see if they accepted or rejected
            currentState = ServerState.ACKED_STATE
            delayCounter = time.time()
            callAckFlag = 0

            #Update display to show that pager call was achknowledged
        elif (((toMillis(currentTime) - toMillis(delayCounter)) >= WAIT_TIME_MS) | callRefusedFlag | ((currentTime - registered_clients[prev_client_id]["p"]) > CLIENT_STATUS_INACTIVE_TIME)):
            #Check to see if it was the client that refused the call for aid
            if (((currentTime - registered_clients[prev_client_id]["p"]) < CLIENT_STATUS_INACTIVE_TIME)):
                #Log the fact that a given client has said NO
                if DEBUG_STATEMACHINE:
                    print("CLIENT SAID NO")

            #Check to see if there is someone else
            if (callNextReceiver()):
                delayCounter = time.time()
            else:
                server_gui.queue.put(lambda: server_gui.response_disp(True,"no_help"))
                delayCounter = time.time()
                currentState = ServerState.REFUSED_STATE

    elif (currentState == ServerState.NO_HELP_STATE):
        if DEBUG_STATEMACHINE: 
            print("NO_HELP_STATE")

        # print((currentTime - delayCounter))
        if ((toMillis(currentTime) - toMillis(delayCounter)) >= ERROR_BLINK_MS):
            #Enable ping button
            server_gui.queue.put(lambda: server_gui.togglePager(False))
            currentState = ServerState.IDLE_STATE

    elif ((currentState == ServerState.ACKED_STATE) | (currentState == ServerState.REFUSED_STATE)):
        if DEBUG_STATEMACHINE: 
            print("ACKED/REFUSED_STATE")
        server_gui.queue.put(lambda: server_gui.togglePager(False))
        currentState = ServerState.IDLE_STATE

    else:#This should not ever happen
        if DEBUG_STATEMACHINE: 
            print("Default state triggered")
    
def runStartup(gui):
    global server_gui 
    server_gui = gui
    setup()

    while True:
        loop()