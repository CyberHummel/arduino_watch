import serial
import time
from datetime import datetime, timedelta

def wait_for_arduino_ready(ser):
    while True:
        line = ser.readline().decode('utf-8').strip()
        if line:
            print(f"[Arduino] {line}")
            if line == "Arduino is ready to receive time updates.":
                print("Arduino meldet: Bereit für Zeit-Updates.")
                break

def main():
    ser = serial.Serial('COM6', 9600, timeout=1)
    time.sleep(2)
    wait_for_arduino_ready(ser)
    while True:
        now = datetime.now()
        current_time_str = now.strftime("%H:%M")
        weekday_str = now.strftime("%A")
        month_day_str = now.strftime("%m-%d")
        data_to_send = f"{current_time_str},{weekday_str},{month_day_str}\n"
        ser.write(data_to_send.encode('utf-8'))
        print(f"[PC->Arduino] {data_to_send.strip()}")
        line = ser.readline().decode('utf-8').strip()
        if line == "TIME_RECEIVED":
            print("Arduino bestätigt Zeitempfang mit 'TIME_RECEIVED'.")
        else:
            print(f"Unerwartete/keine Antwort vom Arduino: '{line}'")
        next_minute = (now + timedelta(minutes=1)).replace(second=0, microsecond=0)
        sleep_duration = (next_minute - datetime.now()).total_seconds()
        if sleep_duration > 0:
            time.sleep(sleep_duration)
        else:
            time.sleep(1)

if __name__ == '__main__':
    main()
