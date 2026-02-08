import sys
import select
import socket

# --- Telnet Protocol Constants ---
IAC  = 255  # "Interpret As Command"
DONT = 254
DO   = 253
WONT = 252
WILL = 251
SB   = 250  # Sub-negotiation Begin
SE   = 240  # Sub-negotiation End

# Telnet Options
OPT_ECHO = 1
OPT_SGA  = 3   # Suppress Go Ahead
OPT_NAWS = 31  # Negotiate About Window Size

class TelnetClient:
    def __init__(self, host, port, cols, rows):
        self.host = host
        self.port = port
        self.cols = cols
        self.rows = rows
        self.socket = None
        self.connected = False
        
        # State Machine Tracking
        self.buffer = b""
        self.telnet_command_mode = False
        self.current_option_cmd = None
        self.subnegotiation_buffer = bytearray()
        self.in_subnegotiation = False

    def connect(self):
        print(f"Connecting to {self.host}:{self.port}...")
        addr = socket.getaddrinfo(self.host, self.port)[0][-1]
        self.socket = socket.socket()
        self.socket.connect(addr)
        self.socket.setblocking(False)
        self.connected = True
        print("Connected!")

    def send_window_size(self):
        """
        RFC 1073: Sends the terminal size to the server.
        Format: IAC SB NAWS <width_high> <width_low> <height_high> <height_low> IAC SE
        """
        # Pack width and height into 16-bit big-endian integers
        w_h, w_l = (self.cols >> 8) & 0xFF, self.cols & 0xFF
        h_h, h_l = (self.rows >> 8) & 0xFF, self.rows & 0xFF

        payload = bytes([IAC, SB, OPT_NAWS, w_h, w_l, h_h, h_l, IAC, SE])
        self.socket.send(payload)
        print(f"REPORTED SIZE: {self.cols}x{self.rows}")

    def process(self):
        try:
            r, _, _ = select.select([self.socket], [], [], 0)
            if not r: return

            data = self.socket.recv(1024)
            if not data:
                self.connected = False
                return

            # Check if there is ANY Telnet command (0xFF) in this chunk
            if IAC not in data:
                # FAST PATH: No commands, just raw text/ANSI
                print(data.decode('utf-8', 'ignore'), end='')
            else:
                # SLOW PATH: Only run the byte-by-byte logic if we see an IAC
                self._process_complex_data(data)
        except OSError:
            pass

        # Check if there is data waiting in the serial buffer (stdin)
        # The [sys.stdin] tells select to watch the keyboard stream
        r, _, _ = select.select([sys.stdin], [], [], 0)
        if r:
            # Read the available character
            key = sys.stdin.read(1)

            if key:
                # Send the key directly to the socket
                # Note: Telnet usually expects \r\n for the Enter key
                if key == '\r' or key == '\n':
                    # Telnet standard for "Enter" is CR + LF
                    client.socket.send(b'\r\n')
                else:
                    # Send the character as bytes
                    client.socket.send(key.encode())


    def _process_complex_data(self, data):
        """
        Only called when the 'Fast Path' detects an IAC (255) byte.
        This handles the state machine for negotiations.
        """
        clean_data = bytearray()
        i = 0
        while i < len(data):
            byte = data[i]

            if not self.telnet_command_mode and byte != IAC:
                clean_data.append(byte)
                i += 1
            elif not self.telnet_command_mode and byte == IAC:
                self.telnet_command_mode = True
                i += 1
            elif self.telnet_command_mode:
                # Sub-negotiation logic
                if byte == SB:
                    self.in_subnegotiation = True
                    self.subnegotiation_buffer = bytearray()
                    i += 1
                elif self.in_subnegotiation:
                    if byte == SE:
                        self._handle_subnegotiation(self.subnegotiation_buffer)
                        self.in_subnegotiation = False
                        self.telnet_command_mode = False
                    else:
                        if byte != IAC: self.subnegotiation_buffer.append(byte)
                    i += 1
                # Simple Command logic (DO/DONT/WILL/WONT)
                elif byte in (251, 252, 253, 254): # WILL, WONT, DO, DONT
                    self.current_option_cmd = byte
                    i += 1
                elif self.current_option_cmd:
                    self._handle_negotiation(self.current_option_cmd, byte)
                    self.current_option_cmd = None
                    self.telnet_command_mode = False
                    i += 1
                else:
                    # Catch-all for other commands (like NOP)
                    self.telnet_command_mode = False
                    i += 1

        if clean_data:
            print(clean_data.decode('utf-8', 'ignore'), end='')


    def _handle_negotiation(self, command, option):
        """Decide how to reply to server requests"""
        response = bytearray()

        if command == DO and option == OPT_NAWS:
            # Server asks: "Do you support Window Size?"
            # We reply: "WILL" (Yes) and immediately send the size
            response.extend([IAC, WILL, OPT_NAWS])
            self.socket.send(response)
            self.send_window_size()
            return

        if command == DO and option == OPT_SGA:
            # Suppress Go Ahead (Standard for character mode)
            response.extend([IAC, WILL, OPT_SGA])
            self.socket.send(response)
            return

        # Default: Refuse everything else to keep it simple
        if command == DO:
            response.extend([IAC, WONT, option])
        elif command == WILL:
            response.extend([IAC, DONT, option])

        if response:
            self.socket.send(response)

    def _handle_subnegotiation(self, data):
        # We don't need to process complex sub-requests for now
        pass

