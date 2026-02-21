import serial
import time

ser = None

def connect(port, baudrate=115200):
    global ser
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        return True
    except serial.SerialException as e:
        print(f"Error connecting to {port}: {e}")
        return False

def disconnect():
    global ser
    if ser is not None:
        ser.close()
        ser = None
        return True
    return False

def receive():
    global ser

    if ser is None:
        return

    while ser is not None and ser.is_open:
        try:
            data = ser.readline().decode('utf-8').strip()
            parsed = parse_data(data)
            if parsed is not None:
                yield parsed
        except serial.SerialException as e:
            print(f"Error reading from serial: {e}")
            time.sleep(1)
        except OSError:
            break

def send(command):
    global ser
    if ser is None or not ser.is_open:
        return False
    try:
        ser.write((command.strip() + "\n").encode("utf-8"))
        ser.flush()
        return True
    except (serial.SerialException, OSError) as e:
        print(f"Error writing to serial: {e}")
        return False

def parse_data(data):
    try:
        if not data:
            return None
        
        data = data.split(',')

        if data[0] == '$BEC':
            return {
                'type': 'beacon',
                'id': data[1],
                'timestamp': time.time()
            }
        elif data[0] == '$POS':
            return {
                'type': 'position',
                'id': data[1],
                'lat': data[2],
                'lon': data[3],
                'timestamp': time.time()
            }
        elif data[0] == '$MSG':
            return {
                'type': 'message',
                'id': data[1],
                'message': data[2],
                'timestamp': time.time()
            }
        elif data[0] == '$REV':
            return {
                'type': 'revoke',
                'id': data[1],
                'timestamp': time.time()
            }
        elif data[0] == '$REI':
            return {
                'type': 'reinstate',
                'id': data[1],
                'timestamp': time.time()
            }
        elif data[0] == '$ID':
            return {
                'type': 'identity',
                'id': data[1],
                'timestamp': time.time()
            }
        elif data[0] == '$MYPOS':
            return {
                'type': 'mypos',
                'lat': data[1],
                'lon': data[2],
                'timestamp': time.time()
            }
        else:
            return None
    except Exception as e:
        print(f"Error parsing data: {e}")
        return None