import serial
import serial.tools.list_ports

print("STM32 PWM Controller")

ports = [
    p.device for p in serial.tools.list_ports.comports()
    if "usbmodem" in p.device.lower()
]

if not ports:
    print("ERROR: No STM32 USB serial port detected.")
    print("Check BOOT0 is LOW and reset the STM32.")
    raise SystemExit(1)

port = ports[0]
print("Using port:", port)

with serial.Serial(port, 115200, timeout=2) as ser:
    print("Enter frequency and duty cycle.")
    print("Example: 20000 50")
    print("Type q to quit.")

    while True:
        command = input("PWM > ").strip()

        if command.lower() == "q":
            break

        parts = command.split()

        if len(parts) != 2:
            print("Example: 20000 50")
            continue

        try:
            frequency, duty = map(int, parts)
        except ValueError:
            print("Please enter two integers.")
            continue

        if not (10 <= frequency <= 100000):
            print("Frequency must be 10-100000 Hz.")
            continue

        if not (0 <= duty <= 100):
            print("Duty cycle must be 0-100%.")
            continue

        message = f"PWM {frequency} {duty}\n"
        ser.write(message.encode("ascii"))
        ser.flush()

        print(f"Sent: {frequency} Hz, {duty}%")
