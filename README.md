SparkDash: External Hardware Telemetry Monitor for NVIDIA Spark
  
  OverviewSP-01 is a fault-tolerant, plug-and-play external hardware monitor designed for the NVIDIA Spark workstation. Because the NVIDIA Spark architecture utilizes a unified memory pool shared between the CPU and GPU, separate VRAM counters are not exposed via nvidia-smi. This project solves that by combining system-wide memory telemetry (excluding reclaimable cache) with real-time GPU core utilization and temperature metrics on a crisp 128x64 I2C OLED display.   
Key Engineering Features

  4-Quadrant UI & Custom PROGMEM Font: The 128x64 screen is divided into four dedicated zones: Node ID (SP-01), GPU Temperature (TEMP in C), GPU Core Load (GPU in % or -- % when unavailable), and System Memory (RAM GiB). A custom 3x5 pixel numeric font (compactDigits) stored in flash memory (PROGMEM) renders long memory strings like 111/122 in just 54x10 pixels instead of 84x16.

  Zero Heap Fragmentation: Written in pure C/C++ without dynamic String objects, the firmware relies on a fixed 32-byte buffer (char serialLine[32]) to protect the Arduino Nano's 2 KiB SRAM during 24/7 operation. 

  I2C Bus Wear Leveling & Packet Validation: The microcontroller caches previous telemetry values (lastMemoryUsed, lastMemoryTotal, lastTemperature, lastUtilization) and skips I2C screen redraws if metrics remain unchanged. Non-blocking character-by-character serial parsing automatically discards corrupted or oversized packets. 
 
  Persistent USB Binding & Auto-Recovery: The Python host daemon connects via a static hardware path (/dev/serial/by-id/...) rather than dynamic /dev/ttyUSB* numbers and automatically reconnects if the USB cable is unplugged. 

Hardware & Wiring (I2C)
  Microcontroller: Arduino Nano V3 (ATmega328P, USB Type-C). 
  Display: 0.96" OLED (SSD1306, 128x64, I2C address 0x3C). 
  
Pinout Mapping:
Display GND ➔ Arduino GND
   
Display VDD (VCC) ➔ Arduino 5V
   
Display SCK (SCL) ➔ Arduino A5
   
Display SDA ➔ Arduino A4
   
Serial Protocol (115200 baud)
The host sends newline-terminated ASCII strings every second in the following format:
<RAM_used_GiB>/<RAM_total_GiB>,<GPU_temp_C>,<GPU_util_percent_or_NA>\n
Example: 45/128,62,85\n or 12/128,48,N/A\n.   

Installation & Deployment
1. Flashing the Microcontroller
You can compile and upload v2 (2).ino via Arduino IDE (requires Adafruit GFX and Adafruit SSD1306 libraries), or flash the pre-compiled v2.nano.hex binary directly from Linux using avrdude:   

Bash
sudo apt install avrdude
# Замените порт на ваш (для старого бутлоадера используется скорость 57600, для нового — 115200)
avrdude -v -p atmega328p -c arduino -P /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 -b 57600 -D -U flash:w:v2.nano.hex:i

Once powered, the screen will display SP-01 and WAITING FOR DATA until the daemon connects.   

2. Setting Up the Linux Daemon
Install the required system packages (python3-serial and python3-psutil) and verify your board's unique serial ID:

Bashsudo apt update && sudo apt install python3-serial python3-psutil
ls -l /dev/serial/by-id/

Update the PORT variable in demon.py with your device path, copy the script to /opt/spark_monitor/demon.py, and enable the systemd service.  
