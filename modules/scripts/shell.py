import sys

class Command:
    def __init__(self, func, env):
        self.func = func
        self.env = env

    def execute(self, *args):
        try:
            self.func(self.env, *args)
        except Exception as e:
            print("\r\n[!] App Exception Caught:")
            sys.print_exception(e)

class Shell:
    def __init__(self, env):
        self.env = env
        self.apps = {}
        self.running = True

    def register(self, name, func):
        self.apps[name] = Command(func, self.env)

    def execute(self, cmd_name, *args):
        if cmd_name in self.apps:
            self.apps[cmd_name].execute(*args)
            return True
        else:
            print(f"{cmd_name}: command not found")
            return False

    def _read_line(self, prompt):
        print(prompt, end="")
        buffer = ""

        while True:
            char = sys.stdin.read(1)
            if not char:
                continue

            if char in ('\r', '\n'):
                print("\r")
                return buffer.strip()

            elif char in ('\x08', '\x7f'):
                if len(buffer) > 0:
                    buffer = buffer[:-1]
                    print('\b \b', end='')

            elif ord(char) >= 32 and ord(char) <= 126:
                buffer += char
                print(char, end='')

    def run(self):
        ver = sys.implementation.version
        version_str = f"{ver[0]}.{ver[1]}.{ver[2]}"
        # TODO: move versioning to makefile
        print(f"MPY_VT v0.1.6; MicroPython v{version_str}\nType 'help' to see commands.")

        while self.running:
            try:
                user_input = self._read_line("\033[38;5;85m$\033[0m ")
            except KeyboardInterrupt:
                print("\r\nType 'exit' to quit.")
                continue

            if not user_input:
                continue

            parts = user_input.split()
            cmd_name = parts[0]
            args = parts[1:]

            if cmd_name == "exit":
                break
            elif cmd_name == "help":
                sorted_apps = sorted(self.apps.keys())
                print("Available commands:", ", ".join(sorted_apps))
                continue

            self.execute(cmd_name, *args)
