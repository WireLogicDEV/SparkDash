#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  Serial.begin(115200);
  
 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }
  

  display.clearDisplay();
  display.setTextColor(WHITE);
  

  display.setTextSize(2);
  display.setCursor(15, 10);
  display.print("NV SPARK");
  

  display.setTextSize(1);
  display.setCursor(20, 35);
  display.print("MONITOR INIT...");
  display.display();
  delay(800);
  

  for(int i = 10; i <= 118; i += 5) {
    display.drawRect(10, 50, 108, 6, WHITE); 
    display.fillRect(10, 50, i, 6, WHITE);   
    display.display();
    delay(50);
  }
  

  delay(500);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(35, 25);
  display.print("READY");
  display.display();
  delay(1000);
  
  display.clearDisplay();
  display.display();
}

void loop() {

  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    
    int comma1 = data.indexOf(',');
    int comma2 = data.indexOf(',', comma1 + 1);
    
  
    if (comma1 > 0 && comma2 > 0) {
      String gpuMem = data.substring(0, comma1);       
      String gpuTemp = data.substring(comma1 + 1, comma2); 
      String ram = data.substring(comma2 + 1);         
      
      display.clearDisplay();
      
    
      display.setTextSize(1);
      display.setCursor(0, 4);  
      display.print("VRAM:");
      
      
      
      String memText = gpuMem + " GiB";
      display.setTextSize(1);
      display.setTextWrap(false);
      if (memText.length() <= 15) {
        display.setCursor(35, 4);
      } else {
        display.setCursor(0, 14);
      }
      display.print(memText);
      display.setTextWrap(true);
      
      
      display.setTextSize(1);
      display.setCursor(0, 28);
      display.print("TEMP:");
      
      display.setTextSize(2);
      display.setCursor(35, 24);
      display.print(gpuTemp); 
      display.print(" C");
      
      
      display.setTextSize(1);
      display.setCursor(0, 52);
      display.print("RAM :");
      
      display.setTextSize(2);
      display.setCursor(35, 48);
      display.print(ram); 
      display.print(" %");
      
      display.display();
    }
  }
}