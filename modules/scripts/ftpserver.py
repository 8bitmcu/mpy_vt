import socket
import network
import os

def get_ip():
    """Retrieve the active IP address of the device."""
    sta = network.WLAN(network.STA_IF)
    if sta.active():
        return sta.ifconfig()[0]
    ap = network.WLAN(network.AP_IF)
    if ap.active():
        return ap.ifconfig()[0]
    return "127.0.0.1"

def format_list_item(name, stat):
    """Format an os.stat() item for the FTP LIST command."""
    is_dir = (stat[0] & 0x4000) != 0
    size = stat[6]
    d = 'd' if is_dir else '-'
    return f"{d}rw-r--r-- 1 owner group {size:8} Jan 01 00:00 {name}\r\n"

def resolve_path(cwd, arg):
    """Resolve absolute and relative paths."""
    if arg == "..":
        parent = cwd.rsplit("/", 1)[0]
        return parent if parent else "/"
    elif arg.startswith("/"):
        return arg
    else:
        return (cwd + "/" + arg).replace("//", "/")

def start(port=21, user="admin", password="admin"):
    server_ip = get_ip()
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listen_sock.bind(('', port))
    listen_sock.listen(1)

    print(f"FTP Server running on {server_ip}:{port}")

    pasv_port = 10000 

    while True:
        cl, addr = listen_sock.accept()
        print(f"Connection from {addr}")
        cl.sendall(b"220 MicroPython FTP Server Ready.\r\n")

        cwd = "/"
        authenticated = False
        data_listen_sock = None
        data_sock = None
        rename_from = None # State tracker for RNFR/RNTO sequence

        try:
            while True:
                data = cl.recv(256)
                if not data:
                    break

                cmd_line = data.decode('utf-8').strip()
                if not cmd_line:
                    continue

                parts = cmd_line.split(" ", 1)
                cmd = parts[0].upper()
                arg = parts[1] if len(parts) > 1 else ""

                # --- Authentication ---
                if cmd == "USER":
                    if arg == user:
                        cl.sendall(b"331 Password required.\r\n")
                    else:
                        cl.sendall(b"530 Invalid user.\r\n")
                elif cmd == "PASS":
                    if arg == password:
                        authenticated = True
                        cl.sendall(b"230 Logged in.\r\n")
                    else:
                        cl.sendall(b"530 Login incorrect.\r\n")
                elif not authenticated and cmd != "QUIT":
                    cl.sendall(b"530 Please login with USER and PASS.\r\n")

                # --- Directory Navigation ---
                elif cmd == "SYST":
                    cl.sendall(b"215 UNIX Type: L8\r\n")
                elif cmd == "PWD":
                    cl.sendall(f'257 "{cwd}" is current directory.\r\n'.encode())
                elif cmd == "CWD":
                    new_cwd = resolve_path(cwd, arg)
                    try:
                        os.stat(new_cwd) # Verify it exists
                        cwd = new_cwd
                        cl.sendall(b"250 CWD command successful.\r\n")
                    except OSError:
                        cl.sendall(b"550 Directory not found.\r\n")
                elif cmd == "CDUP":
                    new_cwd = resolve_path(cwd, "..")
                    try:
                        os.stat(new_cwd)
                        cwd = new_cwd
                        cl.sendall(b"250 CDUP command successful.\r\n")
                    except OSError:
                        cl.sendall(b"550 Directory not found.\r\n")

                elif cmd == "TYPE":
                    cl.sendall(b"200 Type set to I.\r\n")

                # --- File Info & Keep-Alive ---
                elif cmd == "SIZE":
                    target = resolve_path(cwd, arg)
                    try:
                        size = os.stat(target)[6]
                        cl.sendall(f"213 {size}\r\n".encode())
                    except OSError:
                        cl.sendall(b"550 File not found.\r\n")
                elif cmd == "NOOP":
                    cl.sendall(b"200 OK.\r\n")

                # --- File System Modification ---
                elif cmd == "MKD":
                    target = resolve_path(cwd, arg)
                    try:
                        os.mkdir(target)
                        cl.sendall(f'257 "{target}" created.\r\n'.encode())
                    except OSError:
                        cl.sendall(b"550 Failed to create directory.\r\n")
                elif cmd == "RMD":
                    target = resolve_path(cwd, arg)
                    try:
                        os.rmdir(target)
                        cl.sendall(b"250 Directory removed.\r\n")
                    except OSError:
                        cl.sendall(b"550 Failed to remove directory.\r\n")
                elif cmd == "DELE":
                    target = resolve_path(cwd, arg)
                    try:
                        os.remove(target)
                        cl.sendall(b"250 File deleted.\r\n")
                    except OSError:
                        cl.sendall(b"550 Failed to delete file.\r\n")
                elif cmd == "RNFR":
                    target = resolve_path(cwd, arg)
                    try:
                        os.stat(target) # Ensure file exists
                        rename_from = target
                        cl.sendall(b"350 Ready for RNTO.\r\n")
                    except OSError:
                        cl.sendall(b"550 File not found.\r\n")
                elif cmd == "RNTO":
                    if rename_from:
                        target = resolve_path(cwd, arg)
                        try:
                            os.rename(rename_from, target)
                            cl.sendall(b"250 Rename successful.\r\n")
                        except OSError:
                            cl.sendall(b"550 Rename failed.\r\n")
                        finally:
                            rename_from = None # Reset state
                    else:
                        cl.sendall(b"503 RNFR required first.\r\n")

                # --- Data Connection Setup (Passive Mode) ---
                elif cmd == "PASV":
                    if data_listen_sock:
                        data_listen_sock.close()

                    pasv_port += 1
                    if pasv_port > 10100:
                        pasv_port = 10000

                    data_listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    data_listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    data_listen_sock.bind(('', pasv_port))
                    data_listen_sock.listen(1)

                    ip_parts = server_ip.split('.')
                    p1, p2 = (pasv_port >> 8), (pasv_port & 0xFF)

                    cl.sendall(f"227 Entering Passive Mode ({ip_parts[0]},{ip_parts[1]},{ip_parts[2]},{ip_parts[3]},{p1},{p2}).\r\n".encode())

                # --- Reject Active Mode ---
                elif cmd == "PORT":
                    cl.sendall(b"502 Active mode not supported, use PASV.\r\n")

                # --- List Directory ---
                elif cmd == "LIST":
                    list_dir = cwd
                    if arg and not arg.startswith("-"):
                        list_dir = resolve_path(cwd, arg)

                    cl.sendall(b"150 Here comes the directory listing.\r\n")
                    data_sock, _ = data_listen_sock.accept()

                    try:
                        for name in os.listdir(list_dir):
                            path = list_dir + "/" + name if list_dir != "/" else "/" + name
                            try:
                                stat = os.stat(path)
                            except OSError:
                                stat = (0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0)

                            data_sock.sendall(format_list_item(name, stat).encode())

                        cl.sendall(b"226 Directory send OK.\r\n")
                    except Exception as e:
                        print(f"LIST error: {e}")
                        cl.sendall(b"550 Failed to list directory.\r\n")
                    finally:
                        data_sock.close()
                        data_listen_sock.close()
                        data_listen_sock = None

                # --- Download ---
                elif cmd == "RETR":
                    file_path = resolve_path(cwd, arg)
                    try:
                        with open(file_path, "rb") as f:
                            cl.sendall(b"150 Opening data connection.\r\n")
                            data_sock, _ = data_listen_sock.accept()
                            while True:
                                chunk = f.read(1024)
                                if not chunk:
                                    break
                                data_sock.sendall(chunk)
                            cl.sendall(b"226 Transfer complete.\r\n")
                    except OSError:
                        cl.sendall(b"550 File not found.\r\n")
                    finally:
                        if data_sock: data_sock.close()
                        if data_listen_sock: data_listen_sock.close()
                        data_listen_sock = None

                # --- Upload ---
                elif cmd == "STOR":
                    file_path = resolve_path(cwd, arg)
                    try:
                        cl.sendall(b"150 Ok to send data.\r\n")
                        data_sock, _ = data_listen_sock.accept()
                        with open(file_path, "wb") as f:
                            while True:
                                chunk = data_sock.recv(1024)
                                if not chunk:
                                    break
                                f.write(chunk)
                        cl.sendall(b"226 Transfer complete.\r\n")
                    except OSError:
                        cl.sendall(b"550 Error writing file.\r\n")
                    finally:
                        if data_sock: data_sock.close()
                        if data_listen_sock: data_listen_sock.close()
                        data_listen_sock = None

                # --- Disconnect ---
                elif cmd == "QUIT":
                    cl.sendall(b"221 Goodbye.\r\n")
                    break
                else:
                    cl.sendall(b"502 Command not implemented.\r\n")

        except Exception as e:
            print(f"Client error: {e}")
        finally:
            if data_listen_sock: data_listen_sock.close()
            if data_sock: data_sock.close()
            cl.close()
            print("Client disconnected.")
