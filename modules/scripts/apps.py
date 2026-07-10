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

def register_apps(sh):
    sh.register("ftps",        _app("ftpserver"))
    sh.register("telnet",      _app("telnet"))
    sh.register("vi",          _app("vi"))
    sh.register("zm",          _app("zm"))
    sh.register("nm",          _app("netmgr",  tui=True))
    sh.register("fm",          _app("filemgr", tui=True))
    sh.register("irc",         _app("irc",     tui=True))
