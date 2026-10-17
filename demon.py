#!/usr/bin/env python3
"""SparkDash host daemon: телеметрия NVIDIA DGX Spark -> Arduino по Serial.

Раз в секунду отправляет строку:
    <RAM занято GiB>/<RAM всего GiB>,<GPU °C или N/A>,<GPU % или N/A>\n

Порт берётся (по порядку):
    1. первый аргумент командной строки;
    2. переменная окружения SPARKDASH_PORT;
    3. единственное устройство в /dev/serial/by-id/.
"""

import glob
import os
import subprocess
import sys
import time

import psutil
import serial

BAUD = 115200
INTERVAL_S = 1.0
NVIDIA_SMI_TIMEOUT_S = 5
RECONNECT_DELAY_S = 5
# Nano перезагружается при открытии порта (DTR): ждём загрузчик.
RESET_DELAY_S = 3
GIB = 1024 ** 3
NA_VALUES = {'', 'N/A', '[N/A]', '[Not Supported]', 'Not Supported'}

_last_message = None


def log(message):
    """Пишем в journald без буферизации и без повторов одной и той же ошибки."""
    global _last_message
    if message != _last_message:
        print(message, flush=True)
        _last_message = message


def resolve_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    env_port = os.environ.get('SPARKDASH_PORT')
    if env_port:
        return env_port
    candidates = sorted(glob.glob('/dev/serial/by-id/*'))
    if len(candidates) == 1:
        return candidates[0]
    if candidates:
        log('Найдено несколько устройств, укажите SPARKDASH_PORT: '
            + ', '.join(candidates))
    else:
        log('В /dev/serial/by-id/ нет устройств, жду подключения платы...')
    return None


def connect_serial():
    """Ждёт плату и возвращает открытый порт; никогда не бросает исключений."""
    while True:
        port = resolve_port()
        if port:
            try:
                ser = serial.Serial(port, BAUD, timeout=1, write_timeout=2)
                time.sleep(RESET_DELAY_S)
                ser.reset_input_buffer()
                log(f'Подключено к {port}')
                return ser
            except (serial.SerialException, OSError) as error:
                log(f'Ожидание платы на {port}: {error}')
        time.sleep(RECONNECT_DELAY_S)


def parse_metric(text, maximum):
    """Целое значение 0..maximum или None, если метрика недоступна."""
    text = text.strip()
    if text in NA_VALUES:
        return None
    value = int(round(float(text)))
    return value if 0 <= value <= maximum else None


def get_gpu_stats():
    """(температура °C, загрузка %); None — значение недоступно."""
    cmd = [
        'nvidia-smi',
        '--query-gpu=temperature.gpu,utilization.gpu',
        '--format=csv,noheader,nounits',
    ]
    try:
        output = subprocess.check_output(
            cmd, encoding='utf-8', stderr=subprocess.DEVNULL,
            timeout=NVIDIA_SMI_TIMEOUT_S)
        # На Spark один GPU; на всякий случай берём первую строку.
        temp_text, util_text = output.strip().splitlines()[0].split(',')
        return parse_metric(temp_text, 999), parse_metric(util_text, 100)
    except (OSError, subprocess.SubprocessError, ValueError, IndexError) as error:
        log(f'Не удалось прочитать данные GPU: {error}')
        return None, None


def get_memory_gib():
    """Общая (unified) память: занято без учёта освобождаемого кэша."""
    memory = psutil.virtual_memory()
    total = int(memory.total / GIB + 0.5)
    used = int((memory.total - memory.available) / GIB + 0.5)
    return min(used, total), total


def format_value(value):
    return 'N/A' if value is None else str(value)


def build_line():
    used, total = get_memory_gib()
    temperature, utilization = get_gpu_stats()
    return (f'{used}/{total},{format_value(temperature)},'
            f'{format_value(utilization)}\n')


def main():
    log('Запуск SparkDash...')
    ser = connect_serial()
    next_tick = time.monotonic()
    while True:
        line = build_line()
        try:
            ser.write(line.encode('ascii'))
        except (serial.SerialException, OSError) as error:
            log(f'Связь с платой потеряна ({error}), переподключаюсь...')
            try:
                ser.close()
            except Exception:
                pass
            ser = connect_serial()
            next_tick = time.monotonic()
            continue
        next_tick += INTERVAL_S
        delay = next_tick - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        else:
            # nvidia-smi подвис или система перегружена: не догоняем пропуски.
            next_tick = time.monotonic()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
