# Create an INTERFACE library for our C module.
add_library(usermod_modssh INTERFACE)

# Add our source files to the lib
target_sources(usermod_modssh INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modssh.c)

# Add the current directory as an include directory, plus tdeck_i2s for
# the shared ring_buf.h (same cross-task producer/consumer buffer already
# used by audioplayer.c/audiorecorder.c), plus our own wolfssl_shim (see
# below).
target_include_directories(usermod_modssh INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../tdeck_i2s
    ${CMAKE_CURRENT_LIST_DIR}/wolfssl_shim
)

# Link against the wolfSSL/wolfSSH ESP-IDF components (see
# modules/idf_component.yml and boards/LILYGO_T_DECK/sdkconfig.board's
# CONFIG_ESP_ENABLE_WOLFSSH).
#
# Actual on-disk layout of wolfssl__wolfssl (verified directly, not
# assumed): COMPONENT_DIR/wolfssl/{wolfcrypt,openssl} for <wolfssl/...>
# includes, COMPONENT_DIR/include for user_settings.h. There is no
# port/ directory and no generated options.h in this packaging -- see
# wolfssl_shim/wolfssl/options.h, which redirects wolfssh's unconditional
# `#include <wolfssl/options.h>` to the real user_settings.h instead.
idf_component_get_property(wolfssl_dir wolfssl__wolfssl COMPONENT_DIR)
idf_component_get_property(wolfssl_lib wolfssl__wolfssl COMPONENT_LIB)
target_include_directories(usermod_modssh INTERFACE
    ${wolfssl_dir}
    ${wolfssl_dir}/include
)
target_link_libraries(usermod_modssh INTERFACE ${wolfssl_lib})

idf_component_get_property(wolfssh_dir wolfssl__wolfssh COMPONENT_DIR)
idf_component_get_property(wolfssh_lib wolfssl__wolfssh COMPONENT_LIB)
target_include_directories(usermod_modssh INTERFACE
    ${wolfssh_dir}
)
target_link_libraries(usermod_modssh INTERFACE ${wolfssh_lib})

# wolfSSH's client connect state machine (src/ssh.c,
# CONNECT_CLIENT_CHANNEL_AGENT_REQUEST_SENT case) only sends a pty-req
# (SendChannelTerminalRequest(), needed for our interactive shell use --
# see modssh.c's wolfSSH_SetChannelType(..., WOLFSSH_SESSION_TERMINAL,
# ...)) when built with `!defined(NO_FILESYSTEM)` -- and
# SendChannelTerminalRequest's own *definition*, plus its CreateMode()
# helper, are wrapped in the exact same check in src/internal.c, so
# patching just the ssh.c call site alone compiles but leaves the
# function itself out of the binary (undefined reference at link time).
# This project requires NO_FILESYSTEM globally
# (see wolfssl__wolfssl's user_settings.h) for many other, genuinely
# filesystem-dependent wolfSSL/wolfCrypt paths, so undefining it
# project-wide isn't viable -- but this specific code path has no hard
# filesystem dependency: GetTerminalInfo() already falls back to sane
# 80x24 defaults without HAVE_SYS_IOCTL_H, and getenv("TERM") works fine
# on this target's newlib. The one genuinely POSIX-specific bit --
# CreateMode()'s tcgetattr(STDIN_FILENO, ...) call, which wouldn't make
# sense here anyway (no real controlling tty on this target, and it'd be
# querying this device's own local console settings rather than
# anything about the remote session) -- is already separately guarded by
# `!defined(NO_TERMIOS)` with a safe dummy-38400-baud fallback, so
# defining NO_TERMIOS below sidesteps it cleanly rather than needing yet
# another patch. Patch both call sites (ssh.c and internal.c) rather
# than the function/global macro. Idempotent (sed's search text no
# longer matches once patched, so a second run is a silent no-op) so
# this reapplies safely on every fresh `make init` re-fetch of the
# component.
execute_process(
    COMMAND sed -i
        "s/#if defined(WOLFSSH_TERM) && !defined(NO_FILESYSTEM)/#if defined(WOLFSSH_TERM)/"
        ${wolfssh_dir}/src/ssh.c
)
execute_process(
    COMMAND sed -i
        "s/#if defined(WOLFSSH_TERM) && !defined(NO_FILESYSTEM)/#if defined(WOLFSSH_TERM)/"
        ${wolfssh_dir}/src/internal.c
)
target_compile_definitions(${wolfssh_lib} PRIVATE NO_TERMIOS)

# wolfSSH's client always prefers keyboard-interactive over password
# whenever a server offers both, with no fallback -- see
# patch_wolfssh_auth_priority.py for the full rationale (many real
# servers' keyboard-interactive backends reject wolfSSH's bare
# negotiation outright, breaking auth even though plain password would
# have worked). Patches src/internal.c to skip keyboard-interactive
# whenever password is also on offer.
execute_process(
    COMMAND python3
        ${CMAKE_CURRENT_LIST_DIR}/patch_wolfssh_auth_priority.py
        ${wolfssh_dir}
)

# wolfssl/wolfcrypt/settings.h (via port/Espressif/esp-sdk-lib.h) hard
# #errors unless WOLFSSL_USER_SETTINGS is defined -- neither component's
# own build defines this for itself, only (maybe) for dependents, so
# wolfSSL's and wolfSSH's own source files fail to compile without it.
# Applied directly to both COMPONENT_LIB targets (not just our own
# usermod_modssh INTERFACE) so it reaches their own compilation units, not
# just ours.
target_compile_definitions(${wolfssl_lib} PUBLIC WOLFSSL_USER_SETTINGS)
target_compile_definitions(${wolfssh_lib} PUBLIC WOLFSSL_USER_SETTINGS)

# wolfSSL's fast-math bignum backend (wolfcrypt/src/tfm.c) exports a
# global `mp_init(mp_int *)` -- pure coincidence of naming convention
# ("mp" = multi-precision there, vs "MicroPython" in py/runtime.c, which
# also exports a global `mp_init(void)` for VM startup). Both are real,
# needed, unrelated symbols, so this is a link-time collision, not a bug
# in either project. Can't touch either vendor's source, so rename
# wolfSSL's via a textual macro at the compiler level -- applied PRIVATE
# to just these two component targets (every wolfSSL/wolfSSH source file
# that declares or calls mp_init picks up the same rename, so they stay
# internally consistent) and never to usermod_modssh/modssh.c or MicroPython
# itself, which must keep seeing the real mp_init.
target_compile_definitions(${wolfssl_lib} PRIVATE mp_init=wc_fastmath_mp_init)
target_compile_definitions(${wolfssh_lib} PRIVATE mp_init=wc_fastmath_mp_init)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_modssh)
