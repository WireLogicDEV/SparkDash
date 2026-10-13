import subprocess
import psutil
import serial
import time


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
      
        cmd = [
            'nvidia-smi', 
            '--query-gpu=temperature.gpu,memory.used,memory.total', 
            '--format=csv,noheader,nounits'
        ]
        
        output = subprocess.check_output(cmd, encoding='utf-8').strip()
        temp, mem_used_mib, mem_total_mib = map(float, output.split(', '))
        
       
        mem_used_gb = int(mem_used_mib / 1024)
        mem_total_gb = int(mem_total_mib / 1024)
        
        return int(temp), mem_used_gb, mem_total_gb
    except Exception as e:
        print(f"Ошибка чтения данных GPU: {e}")
        return 0, 0, 0

if __name__ == '__main__':
    print("Запуск службы аппаратного мониторинга...")
    
    arduino_serial = connect_serial()
    
    
    time.sleep(3) 

    while True:
        try:
           
            ram_percent = int(psutil.virtual_memory().percent)
            
            
            gpu_temp, gpu_used, gpu_total = get_gpu_stats()
            
            
            data_str = f"{gpu_used}/{gpu_total},{gpu_temp},{ram_percent}\n"
            
            
            arduino_serial.write(data_str.encode('utf-8'))
            
            time.sleep(1)
            
        except serial.SerialException:
            print("Связь с Arduino потеряна. Пытаюсь переподключиться...")
            arduino_serial = connect_serial()
            time.sleep(3)
        except Exception as e:
            print(f"Непредвиденная ошибка в главном цикле: {e}")
            time.sleep(1)
