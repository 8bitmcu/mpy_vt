#
# MicroPython Shell
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import sys
import json

def _app(module, tui=False, audio=False):
    def _run(env, *args):
        if tui:
            if not hasattr(env, 'tui') or env.tui is None:
                import vttui
                env.tui = vttui.VTTUI(env, env.cols, env.rows)
        if audio:
            if not hasattr(env, 'audio') or env.audio is None:
                import audioplayer
                env.audio = audioplayer.AudioPlayer(bck=7, ws=5, dout=6)
        app_module = __import__(module, None, None, [''])
        app_module.main(env, args)
    return _run

class Command:
    def __init__(self, func, env):
        self.func = func
        self.env = env

    def execute(self, *args):
        try:
            self.func(self.env, *args)
            return True
        except Exception as e:
            print("\r\n[!] App Exception Caught:")
            sys.print_exception(e)
            return False

class Shell:
    _MAX_HISTORY = 20

    def __init__(self, env):
        self.env = env
        self.apps = {}
        self.running = True
        self._history = []

        self.aliases = {}
        self.alias_file = "/flash/.favs.json"
        self._load_aliases()

        # temp entries
        self.register("rec",         _app("rec"))
        self.register("radio",       _app("radio"))

        self.register("ftp",         _app("applications.ftp"))
        self.register("ftpd",        _app("applications.ftpd"))
        self.register("telnet",      _app("applications.telnet"))
        self.register("ms",          _app("applications.minesweeper"))
        self.register("menu",        _app("applications.menu",       tui=True))
        self.register("nm",          _app("applications.netmgr",     tui=True))
        self.register("fm",          _app("applications.filemgr",    tui=True))
        self.register("irc",         _app("applications.irc",        tui=True))
        self.register("rss",         _app("applications.rss",        tui=True))
        self.register("fc",          _app("applications.fontconfig", tui=True))
        self.register("play",        _app("applications.player",     tui=True, audio=True))
        self.register("vi",          _app("vi"))
        self.register("zm",          _app("zm"))

    def _load_aliases(self):
        try:
            with open(self.alias_file, "r") as f:
                self.aliases = json.load(f)
        except (OSError, ValueError):
            pass

    def _save_aliases(self):
        try:
            with open(self.alias_file, "w") as f:
                json.dump(self.aliases, f)
        except OSError as e:
            print(f"Failed to save favs: {e}")

    def register(self, name, func):
        self.apps[name] = Command(func, self.env)

    def parse_args(self, line):
        """Split a command line respecting single and double quoted strings."""
        parts = []
        current = []
        in_quote = None
        for ch in line:
            if in_quote:
                if ch == in_quote:
                    in_quote = None
                else:
                    current.append(ch)
            elif ch in ('"', "'"):
                in_quote = ch
            elif ch == ' ':
                if current:
                    parts.append(''.join(current))
                    current = []
            else:
                current.append(ch)
        if current:
            parts.append(''.join(current))
        return parts

    def execute(self, cmd_name, *args):
        if cmd_name in self.apps:
            return self.apps[cmd_name].execute(*args)
        else:
            print(f"{cmd_name}: command not found")
            return False

    def _read_line(self, prompt):
        print(prompt, end="")
        buffer = ""
        hist_idx = len(self._history)  # one past end = live input
        saved = ""                     # stash for in-progress line while browsing

        while True:
            char = sys.stdin.read(1)
            if not char:
                continue

            if char == '\x01':
                # Ctrl-A: mpremote raw REPL request. Re-inject the byte so
                # the MicroPython C REPL loop sees it after the shell exits,
                # then exit so it can handle the raw REPL handshake.
                self.env.kvm.inject('\x01')
                raise EOFError

            if char == '\x1b':
                # Consume ESC [ A/B sequences (3 bytes total)
                nxt = sys.stdin.read(1)
                if nxt == '[':
                    seq = sys.stdin.read(1)
                    if seq == 'A' and self._history:        # UP
                        if hist_idx == len(self._history):
                            saved = buffer
                        if hist_idx > 0:
                            hist_idx -= 1
                            new_buf = self._history[hist_idx]
                            print('\b \b' * len(buffer) + new_buf, end='')
                            buffer = new_buf
                    elif seq == 'B' and self._history:      # DOWN
                        if hist_idx < len(self._history):
                            hist_idx += 1
                            new_buf = self._history[hist_idx] if hist_idx < len(self._history) else saved
                            print('\b \b' * len(buffer) + new_buf, end='')
                            buffer = new_buf
                else:
                    # Bare ESC (trackball click): clear the current line
                    print('\b \b' * len(buffer), end='')
                    buffer = ""
                    hist_idx = len(self._history)
                    saved = ""
                continue

            if char in ('\r', '\n'):
                print("\r")
                return buffer.strip()

            elif char in ('\x08', '\x7f'):
                if buffer:
                    buffer = buffer[:-1]
                    print('\b \b', end='')

            elif 32 <= ord(char) <= 126:
                buffer += char
                print(char, end='')

    def run(self):
        ver = sys.implementation.version
        version_str = f"{ver[0]}.{ver[1]}.{ver[2]}"
        # TODO: move versioning to makefile
        print(f"vtOS v0.1.8; MicroPython v{version_str}\nType 'help' to see commands.")

        while self.running:
            try:
                user_input = self._read_line("\033[38;5;85m$\033[0m ")
            except EOFError:
                break  # mpremote Ctrl-A: exit cleanly, C REPL takes over
            except KeyboardInterrupt:
                print("\r\nType 'exit' to quit.")
                continue

            if not user_input:
                continue

            parts = self.parse_args(user_input)
            cmd_name = parts[0]
            args = parts[1:]

            if cmd_name in self.aliases:
                expanded_parts = self.parse_args(self.aliases[cmd_name])
                if expanded_parts:
                    cmd_name = expanded_parts[0]
                    args = expanded_parts[1:] + args

            if cmd_name == "fav":
                if not args:
                    # List all aliases
                    if not self.aliases:
                        print("No favs set. Use: fav <name> <command>")
                    for k, v in sorted(self.aliases.items()):
                        print(f"  {k} -> {v}")

                elif args[0] == "rm" and len(args) == 2:
                    # Remove an alias (e.g., fav rm myftp)
                    key = args[1]
                    if key in self.aliases:
                        del self.aliases[key]
                        self._save_aliases()
                        print(f"Removed fav '{key}'.")
                    else:
                        print(f"fav '{key}' not found.")

                elif len(args) >= 2:
                    # Create or update an alias
                    key = args[0]

                    # Reconstruct the target command, restoring quotes if spaces exist
                    val_parts = []
                    for a in args[1:]:
                        val_parts.append(f'"{a}"' if ' ' in a else a)

                    val = " ".join(val_parts)
                    self.aliases[key] = val
                    self._save_aliases()
                    print(f"Saved fav: {key} -> {val}")
                continue

            if cmd_name == "clear":
                print("\033[2J\033[H", end="")
                continue

            elif cmd_name == "dbgrst":
                import machine
                _reset_names = {
                    machine.PWRON_RESET: "PWRON_RESET (power-on)",
                    machine.HARD_RESET: "HARD_RESET (panic / external reset)",
                    machine.WDT_RESET: "WDT_RESET (watchdog timeout)",
                    machine.DEEPSLEEP_RESET: "DEEPSLEEP_RESET (woke from deep sleep)",
                    machine.SOFT_RESET: "SOFT_RESET (soft reboot)",
                }
                _reset_cause = machine.reset_cause()
                print("Last reset cause: %s [%d]" % (_reset_names.get(_reset_cause, "UNKNOWN"), _reset_cause))
                continue

            elif cmd_name == "exit":
                break

            elif cmd_name == "help":
                sorted_apps = sorted(self.apps.keys())
                print("Available commands:", ", ".join(sorted_apps))
                continue

            # Record in history, skipping consecutive duplicates
            if not self._history or self._history[-1] != user_input:
                if len(self._history) >= self._MAX_HISTORY:
                    self._history.pop(0)
                self._history.append(user_input)

            self.execute(cmd_name, *args)

