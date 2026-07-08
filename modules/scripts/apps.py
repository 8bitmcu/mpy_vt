def vi_app(env, *args):
    import vi as _vi
    _vi.Vi(env, args)

def ftp_server(env, *args):
    import ftpserver
    ftpserver.start()

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

def telnet_client(env, *args):
    import telnet
    client = telnet.TelnetClient(env, *args)
    client.run()

def zm_app(env, *args):
    import zm
    m = zm.ZMachine(env, args)
    m.run()

def network_manager(env, *args):
    if not hasattr(env, 'tui') or env.tui is None:
        import vttui
        env.tui = vttui.VTTUI(env)

    import netmgr
    netmgr.main(env)

def file_manager(env, *args):
    if not hasattr(env, 'tui') or env.tui is None:
        import vttui
        env.tui = vttui.VTTUI(env)

    import filemgr
    filemgr.main(env)

def register_apps(sh):
    sh.register("cls", clear_screen)
    sh.register("vi", vi_app)
    sh.register("ftps", ftp_server)
    sh.register("debug_reset", last_reset_cause)
    sh.register("telnet", telnet_client)
    sh.register("zm", zm_app)
    sh.register("nm", network_manager)
    sh.register("fm", file_manager)
