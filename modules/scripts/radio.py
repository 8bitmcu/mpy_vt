import lora
import time
import board

radio = None

def send_message(msg):
    data = msg.encode('utf-8')
    radio.transmit(data)
    print(f"Sent: {msg}")

def listen():
    print("Listening for packets...")
    radio.start_receive()
    while True:
        packet = radio.receive()
        if packet:
            print(f"Received: {packet.decode('utf-8')}")
            print(f"RSSI: {radio.rssi()} dBm\n")

            # The chip drops back to standby after a read; re-arm to keep listening.
            radio.start_receive()
        time.sleep(0.1)

def main(env, args):
    global radio

    if not args or args[0] not in ("send", "recv"):
        print("Usage: radio <send|recv|diag> [message]")
        return

    radio = lora.LoRa(cs=board.RADIO_CS,
                      dio1=board.RADIO_DIO1,
                      rst=board.RADIO_RST,
                      busy=board.RADIO_BUSY,
                      sck=board.SPI_SCK,
                      miso=board.SPI_MISO,
                      mosi=board.SPI_MOSI)

    try:
        radio.begin(freq=board.RADIO_FREQ,
                    bw=board.RADIO_BANDWIDTH,
                    sf=9, cr=7,
                    sync_word=0x12, power=10)

    except Exception as e:
        print(f"Radio initialization failed: {e}")
        return

    errors = radio.get_device_errors()
    if errors != 0x0000:
        print(f"SX1262 device errors: 0x{errors:04X}")
        return

    if args[0] == "send":
        send_message(args[1])
    elif args[0] == "recv":
        listen()
