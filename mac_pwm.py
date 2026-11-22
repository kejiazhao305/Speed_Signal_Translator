import serial
from serial.tools import list_ports


def controller():
    ports = [p.device for p in list_ports.comports() if "usbmodem" in p.device.lower()]
    if len(ports) == 1:
        port = ports[0]
    else:
        print("Detected ports:", ", ".join(ports) or "none")
        port = input("Serial device path: ").strip()
    with serial.Serial(port, 115200, timeout=2, write_timeout=2) as link:
        print("Connected to", port)
        while True:
            command = input("Frequency duty | r for RPM | q to quit > ").strip().lower()
            if command == "q":
                return
            if command == "r":
                message = "RPM?"
            else:
                try:
                    frequency, duty = map(int, command.split())
                    if not (10 <= frequency <= 100000 and 0 <= duty <= 100):
                        raise ValueError
                except ValueError:
                    print("Enter 10–100000 Hz and 0–100 percent as two integers.")
                    continue
                message = f"PWM {frequency} {duty}"
            link.write((message + "\n").encode("ascii"))
            response = link.readline()
            if not response.endswith(b"\n"):
                print("Response timed out. Command outcome unknown. Closing connection.")
                return
            print(response.decode("ascii", errors="replace").strip())


if __name__ == "__main__":
    try:
        controller()
    except (serial.SerialException, OSError, EOFError, KeyboardInterrupt) as error:
        print("Controller closed.", error)
