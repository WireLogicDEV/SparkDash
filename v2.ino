#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <avr/pgmspace.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Без динамических String: у классической Nano всего 2 КиБ SRAM.
// Самая длинная допустимая строка короче 24 байт.
char serialLine[32];
uint8_t serialLength = 0;
bool discardLine = false;
bool frameRendered = false;
uint32_t lastMemoryUsed = 0;
uint32_t lastMemoryTotal = 0;
uint16_t lastTemperature = 0;
int16_t lastUtilization = -1;

void centered(const char *text, int16_t x, int16_t y,
              uint8_t width, uint8_t size = 1) {
  display.setTextSize(size);
  // Используем стандартный фиксированный шрифт: 6 пикселей на символ.
  int16_t textWidth = strlen(text) * 6 * size;
  display.setCursor(x + (width - textWidth) / 2, y);
  display.print(text);
}

void centered(const __FlashStringHelper *text, int16_t x, int16_t y,
              uint8_t width, uint8_t size = 1) {
  display.setTextSize(size);
  int16_t textWidth = strlen_P(reinterpret_cast<PGM_P>(text)) * 6 * size;
  display.setCursor(x + (width - textWidth) / 2, y);
  display.print(text);
}

bool isNumber(const char *value, uint8_t maxLength) {
  uint8_t digits = 0;
  uint8_t length = strlen(value);
  if (length == 0 || length > maxLength) return false;
  for (uint8_t i = 0; i < length; ++i) {
    if (value[i] >= '0' && value[i] <= '9') {
      ++digits;
    } else {
      return false;
    }
  }
  return digits > 0;
}

bool splitPair(char *pair, char *&used, char *&total) {
  char *slash = strchr(pair, '/');
  if (!slash) return false;
  *slash = '\0';
  used = pair;
  total = slash + 1;
  return isNumber(used, 5) && isNumber(total, 5);
}

// Узкие цифры 3x5: строка 111/122 занимает 54x10 пикселей, а не 84x16.
const uint8_t compactDigits[11][5] PROGMEM = {
  {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
  {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
  {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
  {7, 5, 7, 1, 7}, {1, 1, 2, 4, 4}
};

void compactMemory(const char *used, const char *total) {
  char text[12];
  uint8_t usedLength = strlen(used);
  uint8_t totalLength = strlen(total);
  memcpy(text, used, usedLength);
  text[usedLength] = '/';
  memcpy(text + usedLength + 1, total, totalLength + 1);
  uint8_t length = usedLength + 1 + totalLength;
  uint8_t scale = length <= 8 ? 2 : 1;
  int16_t width = (length * 4 - 1) * scale;
  int16_t x = 64 + (64 - width) / 2;
  int16_t y = 47 + (16 - 5 * scale) / 2;
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

// Без printf: меньше Flash и пикового расхода стека на Nano.
void appendUnit(char *text, const char *value, char unit) {
  uint8_t length = strlen(value);
  memcpy(text, value, length);
  text[length] = ' ';
  text[length + 1] = unit;
  text[length + 2] = '\0';
}

void renderLine(char *line) {
  // Протокол: RAM-used/total (целые GiB), GPU-temp, GPU-% или N/A.
  char *fields[3];
  char *next = line;
  uint8_t count = 0;
  while (count < 3) {
    fields[count++] = next;
    char *comma = strchr(next, ',');
    if (!comma) break;
    *comma = '\0';
    next = comma + 1;
    if (count == 3) return; // Лишние поля.
  }
  if (count < 3) return;

  char *memoryUsed, *memoryTotal;
  if (!splitPair(fields[0], memoryUsed, memoryTotal)) return;
  if (!isNumber(fields[1], 3)) return;
  bool gpuAvailable = strcmp(fields[2], "N/A") != 0;
  if (gpuAvailable && (!isNumber(fields[2], 3) || atoi(fields[2]) > 100)) return;

  uint32_t used = strtoul(memoryUsed, NULL, 10);
  uint32_t total = strtoul(memoryTotal, NULL, 10);
  uint16_t temperatureValue = atoi(fields[1]);
  int16_t utilizationValue = gpuAvailable ? atoi(fields[2]) : -1;
  if (total == 0 || used > total) return;
  // Не отправляем неизменившийся кадр повторно по I2C.
  if (frameRendered && used == lastMemoryUsed && total == lastMemoryTotal &&
      temperatureValue == lastTemperature && utilizationValue == lastUtilization) {
    return;
  }

  display.clearDisplay();

  // Верхний левый: имя в одну строку.
  centered(F("SP-01"), 0, 8, 63, 2);

  // Верхний правый: температура GPU.
  centered(F("TEMP"), 64, 1, 64);
  char temperature[6];
  appendUnit(temperature, fields[1], 'C');
  centered(temperature, 64, 15, 64, 2);

  // Нижний левый: загрузка GPU, не использование общей памяти.
  centered(F("GPU"), 0, 35, 63);
  char utilization[6];
  appendUnit(utilization, gpuAvailable ? fields[2] : "--", '%');
  centered(utilization, 0, 47, 63, 2);

  // Нижний правый: занято/всего системной памяти.
  centered(F("RAM GiB"), 64, 35, 64);
  compactMemory(memoryUsed, memoryTotal);
  display.display();
  lastMemoryUsed = used;
  lastMemoryTotal = total;
  lastTemperature = temperatureValue;
  lastUtilization = utilizationValue;
  frameRendered = true;
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) {}
  }
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.clearDisplay();
  centered(F("SP-01"), 0, 12, 128, 2);
  centered(F("WAITING FOR DATA"), 0, 42, 128);
  display.display();
}

void loop() {
  // Неблокирующий приём; слишком длинную строку отбрасываем целиком.
  while (Serial.available() > 0) {
    char value = Serial.read();
    if (value == '\r') continue;
    if (value == '\n') {
      if (!discardLine && serialLength > 0) {
        serialLine[serialLength] = '\0';
        renderLine(serialLine);
      }
      serialLength = 0;
      discardLine = false;
    } else if (!discardLine) {
      if (value < ' ' || value > '~') {
        // Повреждённая строка не должна менять отображаемые значения.
        discardLine = true;
      } else if (serialLength < sizeof(serialLine) - 1) {
        serialLine[serialLength++] = value;
      } else {
        discardLine = true;
      }
    }
  }
}
