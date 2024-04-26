Button[] buttonList;
ArrayList<TechStatus> techs = new ArrayList<TechStatus>();
int techsOnline;
int visits;
long free_timer, last_loop;

final int CALLING_CODE = 0xF6;

import processing.serial.*; 
Serial myPort;   
boolean serialOn = true;
boolean errorState = true;

PShape userIcon, checkIcon, rejectIcon, callIcon, userGrayIcon;

void updateTechsOnline()
{
  int online = 0;
  for (TechStatus t : techs)
  {
    if (t.status != 0 && t.status != 1) online++;
  }
  techsOnline = online;
}

void setup()
{
  fullScreen();
  if (serialOn) myPort = new Serial(this, "/dev/cu.usbserial-0001", 115200); 
  
  buttonList = new Button[4];
  buttonList[0] = new Button(width*0.05, height*0.3, width*0.2, width*0.2, 50, "CHECKOUT");
  buttonList[1] = new Button(width*0.05, height*0.65, width*0.2, width*0.2, 50, "WHERE IS IT");
  buttonList[2] = new Button(width*0.26, height*0.3, width*0.2, width*0.2, 50, "REQUEST\nINQUIRY");
  buttonList[3] = new Button(width*0.26, height*0.65, width*0.2, width*0.2, 40, "PROJECT HELP/\nCONSULTATION");
  
  techsOnline = 0;
  
  //for (int i = 0; i < 10; i++)
  //{
  //  techs.add(new TechStatus("Tate", techsOnline));
  //  techsOnline++; 
  //}
  
  userIcon = loadShape("user-solid.svg");
  checkIcon = loadShape("circle-check-solid.svg");
  rejectIcon = loadShape("circle-xmark-solid.svg");
  callIcon = loadShape("phone-flip-solid.svg");
  userGrayIcon = loadShape("user-solid-gray.svg");
  
  last_loop = millis();
  free_timer = 0;
}

void draw()
{
  // Draw background
  background(#c7e9fc);
  
  // Draw title
  fill(#6ec0f0);
  noStroke();
  rect(width*(1 - 0.9)/2, height*0.05, width*0.9, height*0.2, 20);
  textAlign(CENTER, CENTER);
  fill(#FFFFFF);
  textSize(104);
  text("Welcome to the ELC", width/2, (height*0.05) + (height*0.2)/2);
  
  // Update Buttons
  for (int i = 0; i < 4; i++)
  {
    Button b = buttonList[i];
    b.update();
    b.render();
    if (b.pressedFlag)
    {
      b.clearFlag();
      visits++;
      if (serialOn) myPort.write(CALLING_CODE); // Send command to the ESP
      
      for (Button b2 : buttonList)
      {
        b2.disable(true);
      }
      break;
    }
  }
  
  // Draw Tech Banner
  fill(#86f582);
  noStroke();
  rect(width*0.50, height*0.3, width*0.45, height*0.12, 20);
  textAlign(LEFT, CENTER);
  fill(#FFFFFF);
  textSize(54);
  String techString = "Techs Available: ";
  techString += techsOnline;
  text(techString, width*0.50 + 30, height*0.3 + (height*0.12/2));
  
  // Draw Tech Window
  fill(#f5edab);
  noStroke();
  rect(width*0.50, height*0.43, width*0.45, height*0.51, 20);
  
  // Communicate with Pager
  if (serialOn) doSerialCommunication();
  
  long current_time = millis();
  if (free_timer > 0)
  {
    free_timer -= current_time - last_loop;
    if (free_timer <= 0)
    {
      free_interface();
      free_timer = 0;
    }
  }
  last_loop = current_time;
  
  // Draw Techs
  // Update Buttons
  int visual_index = 0;
  for (int i = 0; i < techs.size(); i++)
  {
    TechStatus t = techs.get(i);
    if (t.status != 0) // Check if not in an error state
    {
      t.render(width*0.51 + (visual_index%4)*150, height*0.44 + (visual_index/4)*150, 140, 140);
      visual_index++;
    }
  }
  
  // Stats Bar
  fill(#aaaaaa);
  textAlign(LEFT, CENTER);
  textSize(30);
  String stats = "Visits: ";
  stats += visits;
  text(stats, width*0.50 + 30, height - 30);
  
  // ERROR STATE
  if (errorState)
  {
    fill(#FF0000);
    textSize(100);
    textAlign(CENTER, CENTER);
    text("PLEASE RESET ESP32", width*0.5, height*0.5);
  }
  
}

void doSerialCommunication()
{
  //if ( myPort.available() > 0) { 
  //  // Read from serial production sensor value.
  //  byte[] byteBuffer = new byte[64];
  //  int cnt = myPort.readBytes(byteBuffer);
  //  if (byteBuffer != null) {
  //    for (int i = 0; i < cnt; i++)
  //    {
  //      print("0x");
  //      print(hex(byteBuffer[i]));
  //      print("(" + (char)byteBuffer[i] + ")");
  //      print(' ');
  //    }
  //    println();
  //  }
  //}
  
  while ( myPort.available() > 0) { 

    // Read from serial
    if (myPort.read() == 'E')
    if (myPort.read() == 'L')
    if (myPort.read() == 'C')
    {
      print("Recieved a packet of length ");
      int length = myPort.read();
      println(length);
      if (length == 0) break;
      byte[] byteBuffer = new byte[length];
      int read = myPort.readBytes(byteBuffer);
      if (length != read) println("Error! only recieved a partial packet");
      
      if (byteBuffer[0] == 10)
      {
        // Update the displayed state of the tech card
        if (byteBuffer[1] >= techs.size())
        {
          errorState = true;
          return;
        }
        techs.get(byteBuffer[1]).updateStatus(byteBuffer[2]);
        print("Updating Shop Tech #");
        print(byteBuffer[1]);
        print(" to status ");
        println(byteBuffer[2]);
      }
      else if (byteBuffer[0] == 20)
      {`
        // Call operation terminated
        free_timer = 1000;
        println("Unlocking front");
      }
      else if (byteBuffer[0] == 30)
      {
        // Add a new shop tech operation terminated
        println("Adding Shop Tech");
        char[] name = new char[6];
        for (int i = 0; i < 6; i++)
        {
          name[i] = (char)byteBuffer[i+2];
        }
        techs.add(new TechStatus(new String(name), byteBuffer[1]));
        techsOnline++;
      }
      else if (byteBuffer[0] == 40)
      {
        errorState = false;
      }
    }
  }
}

void free_interface()
{
  for (Button b : buttonList)
  {
    b.disable(false);
  }
  for (TechStatus t : techs)
  {
    if (t.status != 0 && t.status != 1)
      t.updateStatus(2);
  }
}
