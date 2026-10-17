# SparkDash (SP-01): Внешний аппаратный монитор для NVIDIA Spark

<img width="3000" height="4000" alt="1792220074081" src="https://github.com/user-attachments/assets/b4348114-dedd-4af8-9fb5-14be1b2d9239" />


### О проекте

**SparkDash** (идентификатор узла `SP-01`) — отказоустойчивый внешний аппаратный монитор ресурсов для рабочей станции NVIDIA Spark. Поскольку на архитектуре Spark центральный и графический процессоры используют общую память (Unified Memory), утилита `nvidia-smi` не отдает отдельный счетчик VRAM. Проект решает эту задачу, объединяя мониторинг реально занятой системной памяти (за вычетом доступной памяти и освобождаемого кэша) с текущей загрузкой ядра GPU и температурой на контрастном I2C OLED-дисплее с разрешением 128x64 пикселя.

### Ключевые инженерные решения

* **Интерфейс из 4 зон и кастомный шрифт в PROGMEM:** Экран 128x64 разделен на четыре квадранта: имя узла (`SP-01`), температура видеочипа (`TEMP` в `C`), загрузка графического ядра (`GPU` в `%` или `-- %`, если метрика недоступна) и системная память (`RAM GiB`). Собственный узкий числовой шрифт 3x5 пикселей (`compactDigits`) хранится во флеш-памяти (`PROGMEM`) и позволяет отрисовывать длинные значения памяти вроде `111/122` в области 54x10 пикселей вместо стандартных 84x16.


* **Отсутствие фрагментации памяти:** Прошивка написана на чистом C/C++ без использования динамических объектов `String` и функции `printf`, опираясь на фиксированный буфер `char serialLine[32]` для защиты 2 КиБ оперативной памяти SRAM классической Arduino Nano при круглосуточной работе.


* **Экономия ресурса шины I2C и валидация пакетов:** Микроконтроллер кеширует предыдущие значения (`lastMemoryUsed`, `lastMemoryTotal`, `lastTemperature`, `lastUtilization`) и пропускает повторную отправку кадра по шине I2C, если показатели не изменились. Неблокирующий посимвольный парсер автоматически отбрасывает поврежденные или слишком длинные строки.


* **Привязка к физическому ID порта и автовосстановление:** Фоновый Python-демон подключается к плате по неизменному аппаратному пути (`/dev/serial/by-id/...`) вместо динамических номеров `/dev/ttyUSB*` и автоматически переподключается при отсоединении USB-кабеля.



### Аппаратная часть и схема пайки (I2C)

* **Микроконтроллер:** Arduino Nano V3 (ATmega328P, разъем USB Type-C).


* **Дисплей:** 0.96" OLED (контроллер SSD1306, 128x64, I2C-адрес `0x3C`).


* **Подключение контактов:**
* Дисплей **GND** ➔ Arduino **GND**

* Дисплей **VDD** (VCC) ➔ Arduino **5V**

* Дисплей **SCK** (SCL) ➔ Arduino **A5**

* Дисплей **SDA** ➔ Arduino **A4**




### Структура репозитория

* `demon.py` — фоновый Python-скрипт сбора телеметрии (`psutil` + `nvidia-smi`) и отправки данных по Serial на скорости `115200` бод.


* `Spark.hex` — готовая скомпилированная прошивка для быстрой загрузки в микроконтроллер без Arduino IDE.


* `v2 (2).ino` (или `spark_dash/spark_dash.ino`) — исходный код прошивки микроконтроллера.



### Протокол передачи данных (`115200` бод)

Каждую секунду хост отправляет ASCII-строку с символом перевода строки `\n` на конце в следующем формате:
`<RAM_занято_ГиБ>/<RAM_всего_ГиБ>,<GPU_темп_C>,<GPU_загрузка_%_или_NA>\n`
*Пример:* `45/128,62,85\n` или `12/128,48,N/A\n`.

### Установка и запуск

#### 1. Прошивка микроконтроллера

Вы можете скомпилировать и загрузить скетч через Arduino IDE (потребуются библиотеки `Adafruit GFX` и `Adafruit SSD1306`) либо прошить готовый бинарный файл `Spark.hex` напрямую из терминала Linux с помощью `avrdude`:

```bash
sudo apt install avrdude
# Замените путь к порту на ваш (57600 бод для Old Bootloader или 115200 для New Bootloader)
avrdude -v -p atmega328p -c arduino -P /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 -b 57600 -D -U flash:w:Spark.hex:i

```

После подачи питания на экране появится заставка `SP-01` и `WAITING FOR DATA` до момента подключения демона.

#### 2. Настройка службы на Linux

Установите необходимые системные пакеты (`python3-serial` и `python3-psutil`) и узнайте уникальный идентификатор вашей платы:

```bash
sudo apt update && sudo apt install python3-serial python3-psutil
ls -l /dev/serial/by-id/

```

Укажите полученный путь в переменной `PORT` внутри файла `demon.py`, скопируйте скрипт в рабочую директорию и запустите службу `systemd`.

---

# SparkDash: External Hardware Telemetry Monitor for NVIDIA Spark (EN)

### Overview

**SparkDash** (node ID `SP-01`) is a fault-tolerant, plug-and-play external hardware monitor designed for the NVIDIA Spark workstation. Because the NVIDIA Spark architecture utilizes a unified memory pool shared between the CPU and GPU, separate VRAM counters are not exposed via `nvidia-smi`. This project solves that by combining system-wide memory telemetry (excluding reclaimable cache) with real-time GPU core utilization and temperature metrics on a crisp 128x64 I2C OLED display.

### Key Engineering Features

* **4-Quadrant UI & Custom PROGMEM Font:** The 128x64 screen is divided into four dedicated zones: Node ID (`SP-01`), GPU Temperature (`TEMP` in `C`), GPU Core Load (`GPU` in `%` or `-- %` when unavailable), and System Memory (`RAM GiB`). A custom 3x5 pixel numeric font (`compactDigits`) stored in flash memory (`PROGMEM`) renders long memory strings like `111/122` in just 54x10 pixels instead of 84x16.


* **Zero Heap Fragmentation:** Written in pure C/C++ without dynamic `String` objects or `printf`, the firmware relies on a fixed 32-byte buffer (`char serialLine[32]`) to protect the Arduino Nano's 2 KiB SRAM during 24/7 operation.


* **I2C Bus Wear Leveling & Packet Validation:** The microcontroller caches previous telemetry values (`lastMemoryUsed`, `lastMemoryTotal`, `lastTemperature`, `lastUtilization`) and skips I2C screen redraws if metrics remain unchanged. Non-blocking character-by-character serial parsing automatically discards corrupted or oversized packets.


* **Persistent USB Binding & Auto-Recovery:** The Python host daemon connects via a static hardware path (`/dev/serial/by-id/...`) rather than dynamic `/dev/ttyUSB*` numbers and automatically reconnects if the USB cable is unplugged.



### Hardware & Wiring (I2C)

* **Microcontroller:** Arduino Nano V3 (ATmega328P, USB Type-C).


* **Display:** 0.96" OLED (SSD1306, 128x64, I2C address `0x3C`).


* **Pinout Mapping:**
* Display **GND** ➔ Arduino **GND**

* Display **VDD** (VCC) ➔ Arduino **5V**

* Display **SCK** (SCL) ➔ Arduino **A5**

* Display **SDA** ➔ Arduino **A4**




### Repository Structure

* `demon.py` — Python host telemetry daemon (`psutil` + `nvidia-smi`) transmitting over Serial at `115200` baud.


* `Spark.hex` — Pre-compiled Intel HEX firmware binary for direct flashing without Arduino IDE.


* `v2 (2).ino` (or `spark_dash/spark_dash.ino`) — C++ microcontroller firmware source code.



### Serial Protocol (`115200` baud)

The host sends newline-terminated ASCII strings every second in the following format:
`<RAM_used_GiB>/<RAM_total_GiB>,<GPU_temp_C>,<GPU_util_percent_or_NA>\n`
*Example:* `45/128,62,85\n` or `12/128,48,N/A\n`.

### Installation & Deployment

#### 1. Flashing the Microcontroller

You can compile and upload the `.ino` sketch via Arduino IDE (requires `Adafruit GFX` and `Adafruit SSD1306` libraries), or flash the pre-compiled `Spark.hex` binary directly from Linux using `avrdude`:

```bash
sudo apt install avrdude
# Replace the port with your actual device path (use 57600 baud for Old Bootloader, or 115200 for New Bootloader)
avrdude -v -p atmega328p -c arduino -P /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 -b 57600 -D -U flash:w:Spark.hex:i

```

Once powered, the screen will display `SP-01` and `WAITING FOR DATA` until the daemon connects.

#### 2. Setting Up the Linux Daemon

Install the required system packages (`python3-serial` and `python3-psutil`) and verify your board's unique serial ID:

```bash
sudo apt update && sudo apt install python3-serial python3-psutil
ls -l /dev/serial/by-id/

```

Update the `PORT` variable in `demon.py` with your device path, copy the script to `/opt/spark_monitor/demon.py`, and enable the `systemd` service.
