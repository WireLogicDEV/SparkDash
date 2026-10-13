import subprocess
import psutil
import serial
import time

# --- НАСТРОЙКИ ---
# Внимательно проверьте порт! На Linux Arduino Nano часто 
# определяется как /dev/ttyUSB0, но иногда как /dev/ttyACM0
PORT = '/dev/ttyUSB0' 
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
    """Сбор реальных метрик с видеокарты NVIDIA"""
    try:
        # Запрашиваем: температуру, занятую память, всего памяти
        cmd = [
            'nvidia-smi', 
            '--query-gpu=temperature.gpu,memory.used,memory.total', 
            '--format=csv,noheader,nounits'
        ]
        
        output = subprocess.check_output(cmd, encoding='utf-8').strip()
        temp, mem_used_mib, mem_total_mib = map(float, output.split(', '))
        
        # Переводим мегабайты в гигабайты
        mem_used_gb = int(mem_used_mib / 1024)
        mem_total_gb = int(mem_total_mib / 1024)
        
        return int(temp), mem_used_gb, mem_total_gb
    except Exception as e:
        print(f"Ошибка чтения данных GPU: {e}")
        return 0, 0, 0

if __name__ == '__main__':
    print("Запуск службы аппаратного мониторинга...")
    
    arduino_serial = connect_serial()
    
    # Пауза, чтобы Arduino успела перезагрузиться и показать стартовую анимацию
    time.sleep(3) 

    while True:
        try:
            # 1. Читаем загрузку ОЗУ ПК
            ram_percent = int(psutil.virtual_memory().percent)
            
            # 2. Читаем данные видеокарты NVIDIA
            gpu_temp, gpu_used, gpu_total = get_gpu_stats()
            
            # 3. Формируем строку строго по протоколу
            # Например: "12/24,65,45\n" (занято 12 из 24 Гб, темп 65C, ОЗУ 45%)
            data_str = f"{gpu_used}/{gpu_total},{gpu_temp},{ram_percent}\n"
            
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