#
# MicroPython SSH Client (wolfSSH-backed, via the modssh C module)
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import select
import time

import modssh

_CONNECT_TIMEOUT_MS = 15000
_POLL_INTERVAL_S = 0.01
_READ_CHUNK = 256

class SSHSession:
    def __init__(self, env, host, port, username, password):
        self.env = env
        self.client = modssh.Client()
        self.connected = False

        print(f"Connecting to {host}:{port}...\r\n")
        self.client.connect(host, port, username, password)

        waited_ms = 0
        while self.client.status() == modssh.CONNECTING:
            time.sleep_ms(10)
            waited_ms += 10
            if waited_ms >= _CONNECT_TIMEOUT_MS:
                self.client.disconnect()
                raise OSError("connect timed out")

        if self.client.status() != modssh.CONNECTED:
            raise OSError(
                f"connect failed (error_code={self.client.error_code()}, "
                f"auth_attempts={self.client.auth_attempts()}, "
                f"last_auth_type={self.client.last_auth_type()})"
            )

        self.connected = True
        print("Connected. Press Ctrl-] to disconnect.\r\n")

    def run(self):
        try:
            self.process()
        except KeyboardInterrupt:
            pass
        finally:
            self.close()

    def process(self):
        inbuf = bytearray(_READ_CHUNK)
        keybuf = bytearray(1)

        while self.connected:
            if self.client.status() != modssh.CONNECTED:
                print(
                    f"\r\n[remote closed the connection "
                    f"(error_code={self.client.error_code()})]\r\n"
                )
                break

            r, _, _ = select.select([self.client], [], [], _POLL_INTERVAL_S)
            if r:
                try:
                    n = self.client.readinto(inbuf)
                except OSError:
                    n = 0
                if n:
                    print(inbuf[:n])

            if self.env.kvm.readinto(keybuf):
                char_byte = keybuf[0]
                if char_byte == 0x1d:  # Ctrl-]: matches telnet-style escape
                    break
                try:
                    self.client.write(bytes([char_byte]))
                except OSError:
                    pass  # tx ring buffer momentarily full; drop and retry next key

    def close(self):
        if self.connected:
            self.connected = False
            self.client.disconnect()
            print("\r\nDisconnected.\r\n")

def main(env, args):
    host = "192.168.0.21"
    username = "admin"
    password = "admin"
    port = 2222

    try:
        session = SSHSession(env, host, port, username, password)
    except OSError as e:
        print(f"ssh: {e}")
        return

    session.run()
