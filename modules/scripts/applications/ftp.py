#
# MicroPython FTP Client that mounts it's content as a VFS
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import socket
import os

try:
    import uio as io
except ImportError:
    import io
import uctypes

# Stream ioctl request codes (see MicroPython py/stream.h)
_MP_STREAM_FLUSH = 1
_MP_STREAM_SEEK  = 2
_MP_STREAM_POLL  = 3
_MP_STREAM_CLOSE = 4
_SEEK_STRUCT = {
    "offset": 0 | uctypes.INT32,
    "whence": 4 | uctypes.INT32,
}

class FTPFile(io.IOBase):
    """ A file-like object backed by an in-memory buffer, implementing
    MicroPython's native stream protocol (io.IOBase) rather than just
    plain Python read()/write() methods.

    Read-mode files are fully downloaded up front, so all of this just
    operates on local bytes; no mid-transfer FTP juggling needed.
    Write-mode files buffer everything and upload in one STOR on close(). """
    def __init__(self, ftp, mode, file_path, data=b''):
        self.ftp = ftp
        self.mode = mode
        self.file_path = file_path
        self.pos = 0
        self._closed = False
        if 'r' in mode:
            self.buf = data
        else:
            self.buf = bytearray()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def readinto(self, buf):
        if 'r' not in self.mode:
            return -1
        n = min(len(buf), len(self.buf) - self.pos)
        if n <= 0:
            return 0
        buf[:n] = self.buf[self.pos:self.pos + n]
        self.pos += n
        return n

    def read(self, size=-1):
        if 'r' not in self.mode:
            raise OSError("file not opened for reading")
        if size == -1 or self.pos + size > len(self.buf):
            data = self.buf[self.pos:]
        else:
            data = self.buf[self.pos:self.pos + size]
        self.pos += len(data)
        return data

    def write(self, data):
        if 'w' not in self.mode:
            return -1
        if type(data) == str:
            data = data.encode('utf-8')
        # Extend the buffer if writing past the current end, then splice
        # the new bytes in at pos - supports both append and overwrite.
        end = self.pos + len(data)
        if end > len(self.buf):
            self.buf.extend(bytes(end - len(self.buf)))
        self.buf[self.pos:end] = data
        self.pos = end
        return len(data)

    def tell(self):
        return self.pos

    def seek(self, offset, whence=0):
        """ Python-level convenience seek. Native callers don't hit this,
        they go through ioctl() below instead. """
        if whence == 0:
            target = offset
        elif whence == 1:
            target = self.pos + offset
        elif whence == 2:
            target = len(self.buf) + offset
        else:
            raise ValueError("invalid whence")
        if target < 0:
            raise ValueError("negative seek position")
        self.pos = target
        return self.pos

    def ioctl(self, req, arg):
        if req == _MP_STREAM_SEEK:
            try:
                s = uctypes.struct(arg, _SEEK_STRUCT, uctypes.NATIVE)
                self.pos = self.seek(s.offset, s.whence)
                s.offset = self.pos
                return 0
            except Exception:
                return -1
        if req == _MP_STREAM_CLOSE:
            self.close()
            return 0
        if req == _MP_STREAM_FLUSH:
            return 0
        if req == _MP_STREAM_POLL:
            return 0
        return -1

    def close(self):
        if self._closed:
            return
        self._closed = True
        if 'w' in self.mode:
            self.ftp._store_all(self.file_path, self.buf)

class FTP_VFS:
    def __init__(self, host, port=21, user="anonymous", password=""):
        self.host = host
        self.port = port
        self.user = user
        self.password = password
        self.cwd = "/"
        self.sock = None
        self.sock_file = None
        self._stat_cache = {}

    def _send_cmd(self, cmd, expected_code=None):
        """ Sends a command and returns the string response """
        self.sock.send((cmd + "\r\n").encode('utf-8'))
        resp = self.sock_file.readline().decode('utf-8').strip()

        while '-' in resp[:4]: 
            resp = self.sock_file.readline().decode('utf-8').strip()

        if expected_code and not resp.startswith(str(expected_code)):
            raise OSError(f"FTP Error: Expected {expected_code}, got {resp}")
        return resp

    def _pasv(self):
        """ Requests a passive data connection and returns a connected socket """
        resp = self._send_cmd("PASV", 227)
        start = resp.find('(') + 1
        end = resp.find(')')
        parts = resp[start:end].split(',')

        pasv_ip = ".".join(parts[0:4])
        pasv_port = (int(parts[4]) << 8) + int(parts[5])

        data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        addr = socket.getaddrinfo(pasv_ip, pasv_port)[0][-1]
        data_sock.connect(addr)
        return data_sock

    def mount(self, readonly, mkfs):
        addr = socket.getaddrinfo(self.host, self.port)[0][-1]
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(addr)
        self.sock_file = self.sock.makefile('rb')

        resp = self.sock_file.readline().decode('utf-8')
        while '-' in resp[:4]:
            resp = self.sock_file.readline().decode('utf-8')

        self._send_cmd(f"USER {self.user}", 331)
        self._send_cmd(f"PASS {self.password}", 230)
        # Binary mode: essential for correct byte-for-byte transfer of
        # non-text files, and SIZE/REST are only well-defined in this mode.
        self._send_cmd("TYPE I", 200)

    def umount(self):
        try:
            self._send_cmd("QUIT")
        except:
            pass
        if self.sock:
            self.sock.close()

    def chdir(self, dir_name):
        self._send_cmd(f"CWD {dir_name}", 250)
        self.cwd = dir_name

    def getcwd(self):
        resp = self._send_cmd("PWD", 257)
        return resp.split('"')[1]

    def ilistdir(self, path):
        if path == "": path = "."

        self._stat_cache.clear()

        data_sock = self._pasv()
        self._send_cmd(f"LIST {path}", 150)

        data_file = data_sock.makefile('rb')
        while True:
            line = data_file.readline()
            if not line:
                break
            line = line.decode('utf-8').strip()
            if not line:
                continue

            parts = line.split()
            name = " ".join(parts[8:]) 
            size = int(parts[4])
            is_dir = line.startswith('d')
            file_type = 0x4000 if is_dir else 0x8000

            clean_path = "" if path == "." else path
            cache_key = f"{clean_path}/{name}".strip("/")
            self._stat_cache[cache_key] = file_type

            yield (name, file_type, 0, size)

        data_sock.close()
        self.sock_file.readline()

    def open(self, file_path, mode):
        if 'r' in mode:
            data = self._retr_all(file_path)
            return FTPFile(self, mode, file_path, data=data)
        elif 'w' in mode:
            return FTPFile(self, mode, file_path)
        else:
            raise ValueError(f"Unsupported mode: {mode}")

    def _retr_all(self, file_path):
        """ Downloads a file fully into memory and returns it as bytes. """
        data_sock = self._pasv()
        self._send_cmd(f"RETR {file_path}", 150)
        chunks = []
        while True:
            chunk = data_sock.read(1024)
            if not chunk:
                break
            chunks.append(chunk)
        data_sock.close()
        resp = self.sock_file.readline()
        if not resp.startswith(b'226'):
            print("FTP Warning: Expected 226 after transfer, got:", resp)
        return b''.join(chunks)

    def _store_all(self, file_path, data):
        """ Uploads an in-memory buffer as the full contents of a file. """
        data_sock = self._pasv()
        self._send_cmd(f"STOR {file_path}", 150)
        data_sock.write(bytes(data))
        data_sock.close()
        resp = self.sock_file.readline()
        if not resp.startswith(b'226'):
            print("FTP Warning: Expected 226 after transfer, got:", resp)

    def remove(self, path):
        self._send_cmd(f"DELE {path}", 250)

    def mkdir(self, path):
        self._send_cmd(f"MKD {path}", 257)

    def rmdir(self, path):
        self._send_cmd(f"RMD {path}", 250)

    def rename(self, old_path, new_path):
        self._send_cmd(f"RNFR {old_path}", 350)
        self._send_cmd(f"RNTO {new_path}", 250)
        self._stat_cache.clear()

    def stat(self, path):
        if path == "" or path == "/":
            return (0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0)

        clean_path = path.strip("/")
        if clean_path in self._stat_cache:
            mode = self._stat_cache[clean_path]
            return (mode, 0, 0, 0, 0, 0, 0, 0, 0, 0)

        return (0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0)

def main(env, args):
    mount = args[0] if len(args) > 0 else None
    addr = args[1] if len(args) > 1 else None
    port = int(args[2]) if len(args) > 2 else 21
    auth_user = args[3] if len(args) > 3 else "anonymous"
    auth_pass = args[4] if len(args) > 4 else ""

    if mount is None or addr is None:
        print("Usage: ftp <mount> <host> [port] [user] [password]")
        return

    ftp_mount = FTP_VFS(addr, port, auth_user, auth_pass)
    os.mount(ftp_mount, "/" + mount)
    print(f"FTP {addr}:{port} successfully mounted at /{mount}")
