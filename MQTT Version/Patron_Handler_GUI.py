""" 5/15/24 Patron_Handler_GUI - Tyler Perkins
Main file that controls the user interface for the Patron Handler

See project documentation @ 
https://www.notion.so/Patron-Handler-393aba4f6f99428d9febcba6a95a684f


"""

import tkinter as tk
import queue
import time
import threading
import tkinter.messagebox
import json

from main import runStartup
from main import pageRequestFlag

DEBUG_GUI = False


DEFAULT_BUTTON_COLOR = 'gray'
RESPONSE_TIME_MS = 5000

#Define font sizes
FONT_SIZE_TITLE = 48
FONT_SIZE_TEXT = 24
FONT_SIZE_SUBTEXT = 18

class ClientGUI:
    def __init__(self):
        #Initial GUI Setup
        self.root = tk.Tk()
        self.root.title("Patron Handler Service v1.0")
        self.root.config(bg="skyblue")
        self.root.maxsize(1200,1000)

        #Define array to contain all of the registered pagers
        self.buttons = {}

        # Create a frame for the buttons
        self.left_frame = tk.Frame(self.root, width = 480, height = 1000, bg='grey')
        self.left_frame.grid(row=0, column=0, padx=10, pady=5)
        # self.left_frame.pack(side='left')

        tk.Label(self.left_frame, text="Patron Handler", font=("default", FONT_SIZE_TITLE)).grid(row=0, column=0, padx=5, pady=5)
        
        self.response_label_number = tk.Label(self.left_frame, text = "Total Responses: 0", font=("default", FONT_SIZE_SUBTEXT))
        self.response_label_number.grid(row=1, column=0, padx=5, pady=5)

        # Create a frame for storing the buttons
        self.button_frame = tk.Frame(self.left_frame)
        self.button_frame.grid(row=2, column=0, padx=10, pady=5)

        # Create a label 
        self.response_label = tk.Label(self.left_frame, text="NO HELP AVAILABLE", font=("default", FONT_SIZE_TEXT))
        self.response_label.grid(row=3, column=0, padx=5, pady=5)
        self.response_label.grid_remove()
        self.displayingResponse = False

        # Create a pager request button
        self.pager_button = tk.Button(self.left_frame, text="Send Pager Request", command=self.send_pager_request, font=("default", FONT_SIZE_TEXT))
        self.pager_button.grid(row=4, column=0, padx=5, pady=5)

        #Generate right frame
        self.right_frame = tk.Frame(self.root, bg='grey')
        self.right_frame.grid(row=0, column=1, padx=10, pady=5)

        #Define GUI elements to show stats of registered pagers
        tk.Label(self.right_frame, text="Staff Details",font=("default", FONT_SIZE_TITLE)).grid(row=0,column=0,padx=5,pady=5)

        
        self.tech_info_image = tk.Label(self.right_frame, width = 20, height = 20, bg='grey')
        self.tech_info_image.grid(row=1, column=0, padx=10, pady=5)
        self.image = tk.PhotoImage(file="Enoch.png")
        self.imageToDisplay = self.image.subsample(20,20)

        self.tech_info_frame = tk.Frame(self.right_frame, bg='grey')
        self.tech_info_frame.grid(row=2,column=0,padx=5,pady=5)

        self.tech_info_label_name = tk.Label(self.tech_info_frame, text="Name: ", font=("default", FONT_SIZE_TEXT))
        self.tech_info_label_name.grid(row=0,column=0,padx=2,pady=2)
        self.tech_info_label_position = tk.Label(self.tech_info_frame, text="Position/ID: ", font=("default", FONT_SIZE_TEXT))
        self.tech_info_label_position.grid(row=1,column=0,padx=2,pady=2)
        
        self.tech_info_label_name_response = tk.Label(self.tech_info_frame, text="", font=("default", FONT_SIZE_TEXT))
        self.tech_info_label_name_response.grid(row=0,column=1,padx=2,pady=2)
        self.tech_info_label_position_response = tk.Label(self.tech_info_frame, text="", font=("default", FONT_SIZE_TEXT))
        self.tech_info_label_position_response.grid(row=1,column=1,padx=2,pady=2)

        # Create a queue for tasks
        self.queue = queue.Queue()
        self.server_message_queue = queue.Queue()

        self.isPaging = False

        self.analytics_file = "analytics.txt"
        # Read the current values from the file
        with open(self.analytics_file, 'r') as file:
            data = json.load(file)

        # Update the response_label_number visual
        self.response_label_number.config(text=f"Total Responses: {str(data['responses'])}")

        # Start checking the queue for new tasks
        self.check_queue()

    def check_queue(self):
        while not self.queue.empty():
            if DEBUG_GUI:
                print("GUI -- executing GUI task")
            task = self.queue.get()
            task()
        self.root.after(100, self.check_queue)

    def update_status(self, client_props, status):
        #if the button doesn't exist, add it
        if client_props["i"] not in self.buttons or self.buttons[client_props["i"]] is None:
            if status == 'active':
                self.queue.put(lambda: self._add_client(client_props))
                return

        # Determine the button color based on the status
        if status == 'active':
            color = 'green'
        elif status == 'inactive':
            color = 'orange'
        else:  # status is 'offline'
            color = 'red'

        if (self.buttons[client_props["i"]]):
            button = self.buttons[client_props["i"]]
            button.config(bg=color)

    def add_client(self, client_props):
        self.queue.put(lambda: self._add_client(client_props))

    def _add_client(self, client_props):
        # Determine the button color based on the status
        color = 'green'
       
        # Create a new button with the determined color
        button = tk.Button(self.button_frame, text=client_props["n"], command=lambda: self.click_client(client_props), bg=color, font=("default", FONT_SIZE_TEXT))
        # button.grid(row=0, column=0, padx=5, pady=5)

        # Add the button to the dictionary
        self.buttons[client_props["i"]] = button
        self._update_button_positions()

    def remove_client(self, client_id):
        self.queue.put(lambda: self._remove_client(client_id))

    def _remove_client(self, client_id):
        # Remove the button from the dictionary and destroy it
        if client_id in self.buttons:
            button = self.buttons.pop(client_id, None)
            if button is not None:
                button.destroy()
        self._update_button_positions()

    def _update_button_positions(self):
        if DEBUG_GUI:
            print("GUI -- UPDATE POSITION")

        # Remove all buttons from the button frame
        for widget in self.button_frame.winfo_children():
            widget.grid_forget()

        # Add the buttons back to the button frame in the correct order
        for i, button in enumerate(self.buttons.values()):
            button.grid(row=i, column=0, padx=5, pady=5)  # Set the new position of the button
            self.button_frame.grid_rowconfigure(i, weight=1)  # Adjust the row configuration

        # Update the layout of the left frame
        self.left_frame.update()

    def click_client(self, client_props):
        # Update the info label with the client's information
        self.tech_info_label_name_response.config(text = f"{client_props['n']}")
        self.tech_info_label_position_response.config(text = f"{client_props['i']}")
        
        #Create a frame for the user 
        self.tech_info_image = tk.Label(self.right_frame, image=self.imageToDisplay)
        self.tech_info_image.image = self.imageToDisplay  # Keep a reference to the image

        self.tech_info_image.grid(row=1, column=0, padx=10, pady=5)

    def togglePager(self, state):
        if state:
            self.isPaging = True
            self.pager_button.config(text="paging", bg="red")
        else:
            self.isPaging = False
            self.pager_button.config(text="Send Pager Request", bg=DEFAULT_BUTTON_COLOR)

    #Display a response to the user based on responseType
    #If there is an accepted page request display that as well.
    def response_disp(self, state, responseType, client_props=None):
        if (responseType == "no_help"):
            if ((state == True) & (self.displayingResponse == False)):
                if DEBUG_GUI:
                    print("GUI -- DISPLAYING disp")
                self.displayingResponse = True
                self.response_label.config(text=f"NO ONE AVAILABLE")
                self.response_label.grid()
                self.root.after(RESPONSE_TIME_MS, lambda: self.response_disp(False, "no_help"))
            else:
                self.displayingResponse = False
                self.response_label.grid_remove()
                if DEBUG_GUI:
                    print("GUI -- REMOVING disp")
        elif (responseType == "page_accept"):
            if ((state == True) & (self.displayingResponse == False)):
                if DEBUG_GUI:
                    print("GUI -- accepted page request")
                self.displayingResponse = True
                self.response_label.config(text=f"{client_props['n']} is responding")
                self.response_label.grid()
                self.root.after(RESPONSE_TIME_MS, lambda: self.response_disp(False, "page_accept"))
                self.increment_response_val('responses')
            else:
                self.displayingResponse = False
                self.response_label.grid_remove()
        else:
            raise TypeError("ERROR: Invalid GUI response type")

    def send_pager_request(self):
        #Make sure we are not already paging
        if (self.isPaging == False):
            if DEBUG_GUI:
                print("Sending pager request...")
            self.server_message_queue.put(lambda: pageRequestFlag())

    def increment_response_val(self, key):
        if DEBUG_GUI:
            print("Increment response")
        # Read the current values from the file
        with open(self.analytics_file, 'r') as file:
            data = json.load(file)

        # Increment the value associated with the key
        data[key] += 1

        # Write the new values back to the file
        with open(self.analytics_file, 'w') as file:
            json.dump(data, file)

        # Update the response_label_number visual
        self.response_label_number.config(text=f"Total Responses: {str(data['responses'])}")
        
    #If you call this, don't expect to run through the rest of your program anytime soon. 
    def run(self):
        self.check_queue()
        self.root.mainloop()

def main():
    gui = ClientGUI()

    server_thread = threading.Thread(target=runStartup, args=(gui,))
    server_thread.start()

    # Run the GUI in the main thread
    gui.run()

if __name__ == "__main__":
    main()
