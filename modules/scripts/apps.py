def _ensure_tui(env):
    if not hasattr(env, 'tui') or env.tui is None:
        import vttui
        env.tui = vttui.VTTUI(env, env.cols, env.rows)

def _app(module, tui=False):
    def _run(env, *args):
        if tui:
            _ensure_tui(env)
        __import__(module).main(env, args)
    return _run

def last_reset_cause(env, *args):
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

def clear_screen(env, *args):
    print("\033[2J\033[H", end="")

def register_apps(sh):
    sh.register("cls",         clear_screen)
    sh.register("debug_reset", last_reset_cause)
    sh.register("vi",          _app("vi"))
    sh.register("ftps",        _app("ftpserver"))
    sh.register("telnet",      _app("telnet"))
    sh.register("zm",          _app("zm"))
    sh.register("nm",          _app("netmgr",  tui=True))
    sh.register("fm",          _app("filemgr", tui=True))
