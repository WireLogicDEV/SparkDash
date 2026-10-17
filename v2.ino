// SparkDash (SP-01) — внешний монитор NVIDIA DGX Spark.
// Arduino Nano (ATmega328P) + OLED SSD1306 128x64 по I2C (0x3C).
//
// Протокол (115200 бод, ASCII, строка заканчивается '\n'):
//   <RAM занято GiB>/<RAM всего GiB>,<GPU °C или N/A>,<GPU % или N/A>
//   Пример: 45/120,62,85   или   12/120,N/A,N/A

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <avr/pgmspace.h>

// ---------------- Настройки ----------------
// Через сколько миллисекунд без валидных данных показывать NO DATA.
const uint32_t DATA_TIMEOUT_MS = 5000UL;
// Через сколько миллисекунд без данных полностью гасить экран (0 — не гасить).
const uint32_t SCREEN_OFF_MS = 300000UL;   // 5 минут
// Как часто сдвигать изображение на 1 px против выгорания OLED.
const uint32_t SHIFT_PERIOD_MS = 120000UL; // 2 минуты
// Яркость 0..255. Меньше — медленнее выгорание. Заводское значение 0xCF.
const uint8_t CONTRAST = 0x40;
const uint8_t I2C_ADDRESS = 0x3C;
// -------------------------------------------

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Без динамических String: буфер дисплея уже занимает 1 КиБ из 2 КиБ SRAM.
// Самая длинная допустимая строка: 99999/99999,N/A,100 — 19 символов.
char serialLine[32];
uint8_t serialLength = 0;
bool discardLine = false;

struct Telemetry {
  uint32_t memoryUsed;
  uint32_t memoryTotal;
  int16_t temperature;  // -1 = нет данных
  int16_t utilization;  // -1 = нет данных
};

enum ScreenState { SCREEN_WAITING, SCREEN_DATA, SCREEN_NO_DATA, SCREEN_OFF };

Telemetry current;
ScreenState screen = SCREEN_WAITING;
bool needsRedraw = true;
uint32_t lastDataMs = 0;

// Сдвиг кадра по кругу в пределах ±1 px: разметка рассчитана так,
// что при любом сдвиге ничего не выходит за край экрана.
const int8_t SHIFT_PATTERN[][2] PROGMEM = {
  {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
};
const uint8_t SHIFT_COUNT = sizeof(SHIFT_PATTERN) / sizeof(SHIFT_PATTERN[0]);
uint8_t shiftIndex = 0;
int8_t shiftX = 0;
int8_t shiftY = 0;
uint32_t lastShiftMs = 0;

// ---------------- Вспомогательные функции ----------------

// Без printf/itoa: меньше Flash и стека. Возвращает длину строки.
uint8_t formatUint(char *out, uint32_t value) {
  char reversed[10];
  uint8_t length = 0;
  do {
    reversed[length++] = '0' + value % 10;
    value /= 10;
  } while (value > 0);
  for (uint8_t i = 0; i < length; ++i) out[i] = reversed[length - 1 - i];
  out[length] = '\0';
  return length;
}

// Число (или "--" при value < 0), пробел и единица измерения.
void formatWithUnit(char *out, int16_t value, char unit) {
  uint8_t length;
  if (value < 0) {
    out[0] = '-';
    out[1] = '-';
    length = 2;
  } else {
    length = formatUint(out, value);
  }
  out[length] = ' ';
  out[length + 1] = unit;
  out[length + 2] = '\0';
}

void centered(const char *text, int16_t x, int16_t y,
              uint8_t width, uint8_t size = 1) {
  display.setTextSize(size);
  // Стандартный шрифт: 6 пикселей на символ (5 + 1 интервал).
  int16_t textWidth = strlen(text) * 6 * size - size;
  display.setCursor(x + (width - textWidth) / 2 + shiftX, y + shiftY);
  display.print(text);
}

void centered(const __FlashStringHelper *text, int16_t x, int16_t y,
              uint8_t width, uint8_t size = 1) {
  display.setTextSize(size);
  int16_t textWidth = strlen_P(reinterpret_cast<PGM_P>(text)) * 6 * size - size;
  display.setCursor(x + (width - textWidth) / 2 + shiftX, y + shiftY);
  display.print(text);
}

// Узкие цифры 3x5: строка 111/122 занимает 54x10 пикселей, а не 84x16.
const uint8_t compactDigits[11][5] PROGMEM = {
  {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
  {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
  {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
  {7, 5, 7, 1, 7}, {1, 1, 2, 4, 4}
};

void compactMemory(uint32_t used, uint32_t total) {
  char text[12];  // 99999/99999 + '\0'
  uint8_t length = formatUint(text, used);
  text[length++] = '/';
  length += formatUint(text + length, total);

  uint8_t scale = length <= 8 ? 2 : 1;
  int16_t width = (length * 4 - 1) * scale;
  int16_t x = 64 + (64 - width) / 2 + shiftX;
  int16_t y = 47 + (16 - 5 * scale) / 2 + shiftY;
  for (uint8_t i = 0; i < length; ++i) {
    uint8_t index = text[i] == '/' ? 10 : text[i] - '0';
    for (uint8_t row = 0; row < 5; ++row) {
      uint8_t bits = pgm_read_byte(&compactDigits[index][row]);
      for (uint8_t col = 0; col < 3; ++col) {
        if (bits & (4 >> col)) {
          display.fillRect(x + col * scale, y + row * scale, scale, scale, WHITE);
        }
      }
    }
    x += 4 * scale;
  }
}

// ---------------- Разбор протокола ----------------

bool isNumber(const char *value, uint8_t maxLength) {
  uint8_t length = strlen(value);
  if (length == 0 || length > maxLength) return false;
  for (uint8_t i = 0; i < length; ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

uint32_t parseUint(const char *value) {
  uint32_t result = 0;
  while (*value) result = result * 10 + (*value++ - '0');
  return result;
}

// Число до maxLength цифр или "N/A" (-1). Иначе — ошибка.
bool parseOptional(const char *value, uint8_t maxLength, int16_t &result) {
  if (strcmp(value, "N/A") == 0) {
    result = -1;
    return true;
  }
  if (!isNumber(value, maxLength)) return false;
  result = parseUint(value);
  return true;
}

bool parseLine(char *line, Telemetry &out) {
  char *fields[3];
  uint8_t count = 0;
  char *next = line;
  while (true) {
    if (count == 3) return false;  // Лишние поля.
    fields[count++] = next;
    char *comma = strchr(next, ',');
    if (!comma) break;
    *comma = '\0';
    next = comma + 1;
  }
  if (count != 3) return false;

  char *slash = strchr(fields[0], '/');
  if (!slash) return false;
  *slash = '\0';
  if (!isNumber(fields[0], 5) || !isNumber(slash + 1, 5)) return false;

  Telemetry parsed;
  parsed.memoryUsed = parseUint(fields[0]);
  parsed.memoryTotal = parseUint(slash + 1);
  if (parsed.memoryTotal == 0 || parsed.memoryUsed > parsed.memoryTotal) return false;
  if (!parseOptional(fields[1], 3, parsed.temperature)) return false;
  if (!parseOptional(fields[2], 3, parsed.utilization)) return false;
  if (parsed.utilization > 100) return false;

  out = parsed;
  return true;
}

bool sameTelemetry(const Telemetry &a, const Telemetry &b) {
  return a.memoryUsed == b.memoryUsed && a.memoryTotal == b.memoryTotal &&
         a.temperature == b.temperature && a.utilization == b.utilization;
}

// ---------------- Экраны ----------------

void drawTelemetry() {
  char text[8];

  // Верхний левый: имя узла.
  centered(F("SP-01"), 0, 8, 64, 2);

  // Верхний правый: температура GPU.
  centered(F("TEMP"), 64, 1, 64);
  formatWithUnit(text, current.temperature, 'C');
  centered(text, 64, 15, 64, 2);

  // Нижний левый: загрузка ядра GPU (не занятость общей памяти).
  centered(F("GPU"), 0, 35, 64);
  formatWithUnit(text, current.utilization, '%');
  centered(text, 0, 47, 64, 2);

  // Нижний правый: занято/всего общей памяти.
  centered(F("RAM GiB"), 64, 35, 64);
  compactMemory(current.memoryUsed, current.memoryTotal);
}

void drawStatus(const __FlashStringHelper *message) {
  centered(F("SP-01"), 0, 12, 128, 2);
  centered(message, 0, 42, 128);
}

void render() {
  display.clearDisplay();
  switch (screen) {
    case SCREEN_WAITING: drawStatus(F("WAITING FOR DATA")); break;
    case SCREEN_NO_DATA: drawStatus(F("NO DATA")); break;
    case SCREEN_DATA:    drawTelemetry(); break;
    case SCREEN_OFF:     return;
  }
  display.display();
}

void setScreenPower(bool on) {
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

// ---------------- Основной цикл ----------------

void onValidData(const Telemetry &telemetry) {
  lastDataMs = millis();
  if (screen == SCREEN_OFF) setScreenPower(true);
  // Неизменившийся кадр по I2C повторно не отправляем.
  if (screen != SCREEN_DATA || !sameTelemetry(telemetry, current)) {
    needsRedraw = true;
  }
  current = telemetry;
  screen = SCREEN_DATA;
}

void readSerial() {
  // Неблокирующий приём; длинную или повреждённую строку отбрасываем целиком.
  while (Serial.available() > 0) {
    char value = Serial.read();
    if (value == '\r') continue;
    if (value == '\n') {
      if (!discardLine && serialLength > 0) {
        serialLine[serialLength] = '\0';
        Telemetry telemetry;
        if (parseLine(serialLine, telemetry)) onValidData(telemetry);
      }
      serialLength = 0;
      discardLine = false;
    } else if (!discardLine) {
      if (value < ' ' || value > '~' || serialLength >= sizeof(serialLine) - 1) {
        discardLine = true;
      } else {
        serialLine[serialLength++] = value;
      }
    }
  }
}

void updateTimeouts(uint32_t now) {
  uint32_t silence = now - lastDataMs;  // корректно и при переполнении millis()
  if (screen == SCREEN_DATA && silence > DATA_TIMEOUT_MS) {
    // Не показываем устаревшие цифры как живые.
    screen = SCREEN_NO_DATA;
    needsRedraw = true;
  }
  if (SCREEN_OFF_MS > 0 && silence > SCREEN_OFF_MS &&
      (screen == SCREEN_NO_DATA || screen == SCREEN_WAITING)) {
    setScreenPower(false);
    screen = SCREEN_OFF;
  }
}

void updatePixelShift(uint32_t now) {
  if (now - lastShiftMs < SHIFT_PERIOD_MS) return;
  lastShiftMs = now;
  shiftIndex = (shiftIndex + 1) % SHIFT_COUNT;
  shiftX = pgm_read_byte(&SHIFT_PATTERN[shiftIndex][0]);
  shiftY = pgm_read_byte(&SHIFT_PATTERN[shiftIndex][1]);
  if (screen != SCREEN_OFF) needsRedraw = true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    // Дисплей не найден: мигаем встроенным светодиодом.
    for (;;) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);
    }
  }
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(CONTRAST);
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  lastDataMs = millis();
  render();
  needsRedraw = false;
}

void loop() {
  readSerial();
  uint32_t now = millis();
  updateTimeouts(now);
  updatePixelShift(now);
  if (needsRedraw) {
    render();
    needsRedraw = false;
  }
}
