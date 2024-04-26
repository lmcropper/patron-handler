
class Button
{
  float x, y, w, h;
  int textsize, state;
  String text;
  boolean pressedFlag;
  boolean debouncedFlag;
  boolean disabled;
  
  Button (float x, float y, float w, float h, int textsize, String text)
  {
    this.x = x;
    this.y = y;
    this.w = w;
    this.h = h;
    this.textsize = textsize;
    this.text = text;
    
    state = 0;
    pressedFlag = false;
    debouncedFlag = true;
    disabled = false;
  }
  
  void update()
  {
    if (disabled) return;
    if (techsOnline == 0)
    {
      state = 3;
      return;
    }
    
    if (mouseX > x && mouseX < x+w && mouseY > y && mouseY < y+h )
    {
      if (mousePressed)
      {
        state = 2;
        if (debouncedFlag)
        {
          pressedFlag = true;
          debouncedFlag = false;
        }
      }
      else
      {
        if (!mousePressed) debouncedFlag = true;
        state = 1;
      }
    }
    else
    {
      state = 0;
    }
  }
  
  void clearFlag()
  {
    pressedFlag = false;
  }
  
  void disable(boolean val)
  {
    disabled = val;
    if (disabled) state = 3;
  }
  
  void render()
  {
    noStroke();
    
    if (state == 0)
    {
      fill(#f5ae5d);
      rect(x, y + 10, w, h, 20);
      fill(#ffbe73);
      rect(x, y, w, h, 20);
      fill(#FFFFFF);
      textSize(textsize);
      text(text, x + w/2, y + h/2);
    }
    else if (state == 1)
    {
      fill(#d99548);
      rect(x, y + 10, w, h, 20);
      fill(#e8ab64);
      rect(x, y, w, h, 20);
      fill(#FFFFFF);
      textSize(textsize);
      text(text, x + w/2, y + h/2);
    }
    else if (state == 2)
    {
      fill(#d99548);
      rect(x, y + 10, w, h, 20);
      fill(#ab793f);
      rect(x, y, w, h, 20);
      fill(#FFFFFF);
      textSize(textsize);
      text(text, x + w/2, y + h/2);
    }
    else if (state == 3)
    {
      fill(#e8d9c8);
      rect(x, y + 10, w, h, 20);
      fill(#fff1e0);
      rect(x, y, w, h, 20);
      fill(#FFFFFF);
      textSize(textsize);
      text(text, x + w/2, y + h/2);
    }
  }
}


void drawButton(int state, String str, int textsize, float locX, float locY)
{
  noStroke();
  
  float w = width*0.2;
  float h = width*0.2;
  
  if (state == 0)
  {
    fill(#f5ae5d);
    rect(locX, locY + 10, w, h, 20);
    fill(#ffbe73);
    rect(locX, locY, w, h, 20);
    fill(#FFFFFF);
    textSize(textsize);
    text(str, locX + w/2, locY + h/2);
  }
  else if (state == 1)
  {
    fill(#d99548);
    rect(locX, locY + 10, w, h, 20);
    fill(#e8ab64);
    rect(locX, locY, w, h, 20);
    fill(#FFFFFF);
    textSize(textsize);
    text(str, locX + w/2, locY + h/2);
  }
  else if (state == 2)
  {
    fill(#ffbe73);
    rect(locX, locY + 10, w, h, 20);
    fill(#ab793f);
    rect(locX, locY, w, h, 20);
    fill(#FFFFFF);
    textSize(textsize);
    text(str, locX + w/2, locY + h/2);
  }
}

class TechStatus
{
  // 0 - Error
  // 1 - Offline
  // 2 - Available
  // 3 - Calling
  // 4 - Accepted
  // 5 - Rejected
  
  int status;
  String name;
  
  TechStatus (String name, int id)
  {
    this.name = name;
    status = 2;
  }
  
  void updateStatus(int newStatus)
  {
    this.status = newStatus;
    updateTechsOnline();
  }
  
  boolean isOnline()
  {
    return status != 0 && status != 1; 
  }
  
  void render(float x, float y, float w, float h)
  {
    // Background
    fill(#FFFFFF);
    rect(x, y, w, h, 20);
    
    // Name
    if (isOnline()) fill(#000000);
    else fill(#FF0000);
    textAlign(CENTER, BOTTOM);
    textSize(30);
    text(name, x + w*0.5, y + h);
    
    // User Icon
    if (status != 0 && status != 1)
    {
      float wid = w * 0.57;
      shape(userIcon, x + (w - wid)/2, y + (h - wid)/2, wid, wid);
    }
    else if (status == 1)
    {
      float wid = w * 0.57;
      shape(userGrayIcon, x + (w - wid)/2, y + (h - wid)/2, wid, wid);
    }
    
    if (status == 3)
    {
      shape(callIcon, x + w*0.7, y + h*0.3, w * 0.2, w * 0.2);
    }
    else if (status == 4)
    {
      shape(checkIcon, x + w*0.7, y + h*0.3, w * 0.2, w * 0.2);
    }
    else if (status == 5)
    {
      shape(rejectIcon, x + w*0.7, y + h*0.3, w * 0.2, w * 0.2);
    }
  }
  
}
  
