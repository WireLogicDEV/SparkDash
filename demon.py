import subprocess
import psutil
import serial
import time

# --- НАСТРОЙКИ ---
# Постоянный USB-путь платы: не зависит от номера /dev/ttyUSB*.
PORT = '/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0'
BAUD = 115200

def connect_serial():
    """Безопасное подключение: если кабель не вставлен, скрипт будет ждать"""
    while True:
        try:
            ser = serial.Serial(PORT, BAUD, timeout=1)
            print(f"Успешное подключение к Arduino на порту {PORT}")
            return ser
        except Exception as e:
            print(f"Ожидание Arduino на {PORT}... (Ошибка: {e})")
            time.sleep(5)

def get_gpu_stats():
    """Температура и загрузка GPU; None означает недоступную загрузку."""
    try:
        # На Spark загрузка доступна, в отличие от отдельной VRAM.
        cmd = [
            'nvidia-smi', 
            '--query-gpu=temperature.gpu,utilization.gpu',
            '--format=csv,noheader,nounits'
        ]
        
        output = subprocess.check_output(cmd, encoding='utf-8').strip()
        temp_text, utilization_text = (value.strip() for value in output.split(','))
        temp = 0 if temp_text in ('[N/A]', 'N/A') else int(float(temp_text))
        utilization = (
            None if utilization_text in ('[N/A]', 'N/A')
            else int(float(utilization_text))
        )
        if utilization is not None and not 0 <= utilization <= 100:
            raise ValueError('Загрузка GPU вне диапазона 0–100%')
        return temp, utilization
    except Exception as e:
        print(f"Ошибка чтения данных GPU: {e}")
        return 0, None

if __name__ == '__main__':
    print("Запуск службы аппаратного мониторинга...")
    
    arduino_serial = connect_serial()
    
    # Пауза, чтобы Arduino успела перезагрузиться и показать стартовую анимацию
    time.sleep(3) 

    while True:
        try:
            # 1. Общая память системы (на Spark используется CPU и GPU).
            # Исключаем доступную память, включая освобождаемый кэш.
            memory = psutil.virtual_memory()
            memory_used_gib = (memory.total - memory.available) / (1024 ** 3)
            memory_total_gib = memory.total / (1024 ** 3)

            # 2. Температура и реальная загрузка GPU (не процент занятой ОЗУ).
            gpu_temp, gpu_utilization = get_gpu_stats()
            gpu_percent = 'N/A' if gpu_utilization is None else str(gpu_utilization)
            
            # 3. Формируем строку строго по протоколу
            # Память used/total (целые ГиБ), °C, GPU % или N/A.
            data_str = (
                f"{memory_used_gib:.0f}/{memory_total_gib:.0f},"
                f"{gpu_temp},{gpu_percent}\n"
            )
            
            # 4. Отправляем в Arduino
            arduino_serial.write(data_str.encode('utf-8'))
            
            time.sleep(1)
            
        except serial.SerialException:
            print("Связь с Arduino потеряна. Пытаюсь переподключиться...")
            arduino_serial = connect_serial()
            time.sleep(3)
        except Exception as e:
            print(f"Непредвиденная ошибка в главном цикле: {e}")
            time.sleep(1)
