# SparkDash (SP-01): внешний аппаратный монитор для NVIDIA DGX Spark

<img width="3000" height="4000" alt="1792220074081" src="https://github.com/user-attachments/assets/b4348114-dedd-4af8-9fb5-14be1b2d9239" />

[English version below](#sparkdash-sp-01-external-hardware-monitor-for-nvidia-dgx-spark)

### О проекте

**SparkDash** (идентификатор узла `SP-01`) — внешний монитор ресурсов для рабочей станции NVIDIA DGX Spark. На Spark процессор и видеочип используют общую память (Unified Memory), поэтому `nvidia-smi` не отдаёт отдельный счётчик VRAM. SparkDash показывает реально занятую общую память (без освобождаемого кэша), загрузку ядра GPU и его температуру на OLED-дисплее 128x64.

### Возможности

* **Экран из четырёх зон:** имя узла (`SP-01`), температура GPU (`TEMP`, °C), загрузка GPU (`GPU`, %) и общая память (`RAM GiB`). Если метрика недоступна, вместо числа выводится `--`. Узкий шрифт 3x5 в `PROGMEM` умещает `111/120` в 54x10 пикселей.
* **Индикация пропажи данных:** если 5 секунд не приходит ни одной корректной строки, экран переключается на `NO DATA` — устаревшие цифры не выдаются за текущие. Через 5 минут без данных экран гаснет и включается сам при появлении данных.
* **Защита OLED от выгорания:** каждые 2 минуты изображение сдвигается на 1 пиксель по кругу, яркость снижена до `0x40` (настраивается в начале скетча).
* **Без фрагментации памяти:** нет `String`, `printf` и динамических объектов — только фиксированный буфер `char serialLine[32]`. Это важно: буфер дисплея уже занимает 1 КиБ из 2 КиБ SRAM Arduino Nano.
* **Экономия шины I2C и валидация:** неизменившийся кадр повторно не отправляется. Неблокирующий парсер отбрасывает повреждённые, слишком длинные и выходящие за диапазон строки.
* **Надёжный демон:** порт задаётся через `SPARKDASH_PORT` или находится автоматически, при отключении кабеля демон переподключается, у `nvidia-smi` есть таймаут.
* **Индикация ошибки дисплея:** если OLED не найден на шине I2C, мигает встроенный светодиод Nano.

### Аппаратная часть

* **Микроконтроллер:** Arduino Nano V3 (ATmega328P, USB Type-C).
* **Дисплей:** 0.96" OLED, контроллер SSD1306, 128x64, I2C-адрес `0x3C`.

| Дисплей | Arduino Nano |
|---|---|
| GND | GND |
| VDD (VCC) | 5V |
| SCK (SCL) | A5 |
| SDA | A4 |

### Структура репозитория

| Файл | Назначение |
|---|---|
| `v2.ino` | Прошивка Arduino |
| `v2.nano.hex` | Скомпилированная прошивка для прошивки через `avrdude` |
| `demon.py` | Демон сбора телеметрии (`psutil` + `nvidia-smi`) |
| `sparkdash.service` | Unit-файл systemd для автозапуска демона |

### Протокол (`115200` бод)

Раз в секунду хост отправляет ASCII-строку, оканчивающуюся `\n`:

```
<RAM занято GiB>/<RAM всего GiB>,<GPU °C или N/A>,<GPU % или N/A>
```

Примеры: `45/120,62,85` или `12/120,N/A,N/A`.

> Прошивку и демон обновляйте вместе: старая прошивка отбрасывает строки с `N/A` в поле температуры.

### Установка

#### 1. Прошивка микроконтроллера

**Arduino IDE:** установите библиотеки `Adafruit GFX Library` и `Adafruit SSD1306`, откройте `v2.ino` (IDE предложит положить его в папку `v2`), выберите плату Arduino Nano и загрузите.

**arduino-cli** (сборка и загрузка из терминала):

```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
mkdir v2 && cp v2.ino v2/
# Для клонов со старым загрузчиком: --fqbn arduino:avr:nano:cpu=atmega328old
arduino-cli compile --fqbn arduino:avr:nano --output-dir build v2
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano v2
```

Готовый `.hex` после сборки лежит в `build/v2.ino.hex`.

**avrdude** (готовый `.hex` без IDE):

```bash
sudo apt install avrdude
# 57600 — старый загрузчик, 115200 — новый
avrdude -v -p atmega328p -c arduino -P /dev/ttyUSB0 -b 57600 -D -U flash:w:v2.nano.hex:i
```

После подачи питания на экране появятся `SP-01` и `WAITING FOR DATA`.

#### 2. Демон на Spark

```bash
sudo apt update && sudo apt install python3-serial python3-psutil
ls -l /dev/serial/by-id/      # узнайте путь к плате

sudo mkdir -p /opt/spark_monitor
sudo cp demon.py /opt/spark_monitor/
sudo cp sparkdash.service /etc/systemd/system/
sudo nano /etc/systemd/system/sparkdash.service   # пропишите SPARKDASH_PORT
sudo systemctl daemon-reload
sudo systemctl enable --now sparkdash
journalctl -u sparkdash -f    # логи
```

У клонов Nano с чипом CH340 путь обычно выглядит как `usb-1a86_USB_Serial-if00-port0`, у плат с FTDI — `usb-FTDI_FT232R_USB_UART_<серийный номер>-if00-port0`. Если к Spark подключена только одна такая плата, `SPARKDASH_PORT` можно не задавать.

Для ручной проверки: `python3 demon.py /dev/serial/by-id/<ваша плата>`.

---

# SparkDash (SP-01): External Hardware Monitor for NVIDIA DGX Spark

### Overview

**SparkDash** (node ID `SP-01`) is an external resource monitor for the NVIDIA DGX Spark workstation. The Spark's CPU and GPU share a unified memory pool, so `nvidia-smi` exposes no separate VRAM counter. SparkDash shows actually used unified memory (excluding reclaimable cache), GPU core utilization and GPU temperature on a 128x64 OLED display.

### Features

* **4-zone screen:** node ID (`SP-01`), GPU temperature (`TEMP`, °C), GPU load (`GPU`, %) and unified memory (`RAM GiB`). Unavailable metrics are shown as `--`. A narrow 3x5 font in `PROGMEM` fits `111/120` into 54x10 pixels.
* **Stale data detection:** if no valid line arrives for 5 seconds, the screen switches to `NO DATA` so old values are never shown as live. After 5 minutes without data the panel turns off and wakes up automatically when data returns.
* **OLED burn-in protection:** the image shifts by 1 pixel in a circle every 2 minutes, and contrast is lowered to `0x40` (configurable at the top of the sketch).
* **No heap fragmentation:** no `String`, `printf` or dynamic objects — just a fixed `char serialLine[32]` buffer. This matters because the display buffer already takes 1 KiB of the Nano's 2 KiB SRAM.
* **I2C savings and validation:** unchanged frames are not resent. The non-blocking parser drops corrupted, oversized and out-of-range lines.
* **Robust daemon:** the port is set via `SPARKDASH_PORT` or auto-detected, the daemon reconnects after the cable is unplugged, and `nvidia-smi` calls have a timeout.
* **Display fault indication:** if the OLED is not found on the I2C bus, the Nano's built-in LED blinks.

### Hardware

* **Microcontroller:** Arduino Nano V3 (ATmega328P, USB Type-C).
* **Display:** 0.96" OLED, SSD1306, 128x64, I2C address `0x3C`.

| Display | Arduino Nano |
|---|---|
| GND | GND |
| VDD (VCC) | 5V |
| SCK (SCL) | A5 |
| SDA | A4 |

### Repository Structure

| File | Purpose |
|---|---|
| `v2.ino` | Arduino firmware |
| `v2.nano.hex` | Pre-compiled firmware for flashing with `avrdude` |
| `demon.py` | Telemetry daemon (`psutil` + `nvidia-smi`) |
| `sparkdash.service` | systemd unit for running the daemon |

### Serial Protocol (`115200` baud)

Every second the host sends a newline-terminated ASCII line:

```
<RAM used GiB>/<RAM total GiB>,<GPU °C or N/A>,<GPU % or N/A>
```

Examples: `45/120,62,85` or `12/120,N/A,N/A`.

> Update the firmware and the daemon together: older firmware rejects lines with `N/A` in the temperature field.

### Installation

#### 1. Flashing the Microcontroller

**Arduino IDE:** install the `Adafruit GFX Library` and `Adafruit SSD1306` libraries, open `v2.ino` (the IDE will offer to move it into a `v2` folder), select Arduino Nano and upload.

**arduino-cli:**

```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
mkdir v2 && cp v2.ino v2/
# Clones with the old bootloader: --fqbn arduino:avr:nano:cpu=atmega328old
arduino-cli compile --fqbn arduino:avr:nano --output-dir build v2
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano v2
```

The compiled firmware is saved as `build/v2.ino.hex`.

**avrdude** (pre-compiled `.hex`, no IDE):

```bash
sudo apt install avrdude
# 57600 for the old bootloader, 115200 for the new one
avrdude -v -p atmega328p -c arduino -P /dev/ttyUSB0 -b 57600 -D -U flash:w:v2.nano.hex:i
```

Once powered, the screen shows `SP-01` and `WAITING FOR DATA`.

#### 2. Daemon on the Spark

```bash
sudo apt update && sudo apt install python3-serial python3-psutil
ls -l /dev/serial/by-id/      # find your board

sudo mkdir -p /opt/spark_monitor
sudo cp demon.py /opt/spark_monitor/
sudo cp sparkdash.service /etc/systemd/system/
sudo nano /etc/systemd/system/sparkdash.service   # set SPARKDASH_PORT
sudo systemctl daemon-reload
sudo systemctl enable --now sparkdash
journalctl -u sparkdash -f    # logs
```

Nano clones with a CH340 chip usually appear as `usb-1a86_USB_Serial-if00-port0`, FTDI boards as `usb-FTDI_FT232R_USB_UART_<serial>-if00-port0`. If only one such board is connected, `SPARKDASH_PORT` can be omitted.

Manual test run: `python3 demon.py /dev/serial/by-id/<your board>`.
