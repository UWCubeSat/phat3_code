import serial
import csv
import time
import argparse

# Arguments
parser = argparse.ArgumentParser(
                    prog='RxLogger',
                    description='Logs data packets to CSV file')
parser.add_argument('-p', '--port', type=str, required=True)
parser.add_argument('-f', '--file', type=str, required=True)
args = parser.parse_args()
if not args.file.endswith(".csv"):
    print('Usage: python3 rx_logger -p <PORT> -f <CSV_FILE>;\ninvalid file extension')
    exit()

# CONFIGURATION
SERIAL_PORT = args.port
BAUD_RATE = 115200
OUTPUT_FILE = args.file
DELIMITER = ';'  # Must match the C code

CSV_HEADERS = [
    "Millis", 
    "AHT_Temp", "AHT_Hum", 
    "BMP_Temp", "BMP_Press",
    "Accel_X", "Accel_Y", "Accel_Z",
    "Rot_X", "Rot_Y", "Rot_Z",
    "SCD_CO2", "SCD_Temp", "SCD_Hum",
    "GPS_Lat", "GPS_Lon", "GPS_Time"
]

def run_logger():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {SERIAL_PORT}...")
    except Exception as e:
        print(f"Error: {e}")
        return

    # Open CSV with the specified delimiter
    with open(OUTPUT_FILE, 'a', newline='') as f:
        # csv.writer needs to know we are using semicolons
        writer = csv.writer(f, delimiter=DELIMITER)
        
        if f.tell() == 0:
            writer.writerow(CSV_HEADERS)

        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line.startswith("DATA_PKT"):
                        # Remove prefix and split by SEMICOLON
                        clean_line = line.replace("DATA_PKT;", "")
                        data_values = clean_line.split(DELIMITER)
                        
                        if len(data_values) == len(CSV_HEADERS):
                            writer.writerow(data_values)
                            f.flush()
                            print(f"[DATA] Saved: {data_values[0]} | GPS: {data_values[-1]}")
                        else:
                            print(f"[WARN] Column mismatch. Got {len(data_values)}, expected {len(CSV_HEADERS)}")
                            
                    else:
                        print(f"[ESP32] {line}")
                        
                except Exception as e:
                    print(f"Error: {e}")

if __name__ == "__main__":
    run_logger()