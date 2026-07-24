#!/usr/bin/env python3
# Patches wolfSSH's wolfssh/port.h and src/port.c so src/wolfsftp.c (built
# in via WOLFSSH_SFTP, see micropython.cmake) compiles and links.
#
# Why this is needed: wolfsftp.c isn't just modsftp.c's client-side
# primitives -- it also contains wolfSSH's SERVER-side SFTP receive
# handlers (live in this build since NO_WOLFSSH_SERVER isn't defined,
# needed by modsshd.c), which use a family of local-filesystem macros
# (WFD/WDIR/WSTAT_T/WOPEN/WSTAT/WCHMOD/WRENAME/wfopen/wPwrite/wPread/...).
# Confirmed directly against the vendor source: every one of those lives
# behind a `!defined(NO_FILESYSTEM)` guard, in both port.h (macro/inline-
# function declarations) and src/port.c (wfopen()/wPwrite()/wPread()'s
# actual definitions, in a separate guard from port.h's) -- and this
# project's user_settings.h sets NO_FILESYSTEM globally, for wolfCrypt's
# own unrelated reasons. Since none of this code is ever called from our
# own C (modsftp.c only drives the low-level Open/SendReadPacket/
# SendWritePacket/Close/LS/etc primitives), the fix is letting these
# three guards fall through to their real, working generic-POSIX
# branches instead of hand-stubbing ~20 macros ourselves (several of
# which, e.g. WSTAT_T, need a real POSIX-compatible struct layout to be
# useful, not just a stub value) -- ESP-IDF's newlib genuinely provides
# fopen/open/stat/opendir/etc, so this resolves to real, if never-
# actually-called-by-us, libc functions. Each of the three guards'
# *other* conditions already correctly scope it to exactly this case, so
# this can't newly enable anything modssh.c/modsshd.c already depend on.
# A fourth, smaller patch (WLSTAT) fixes a real gap found via an actual
# build once the three guards above were resolved -- see its own comment
# below.
#
# Invoked from micropython.cmake via execute_process(), same as the
# other patch scripts there. Plain text substitution rather than sed --
# these edits mix multi-line preprocessor conditions with literal `&&`/
# backslash-continuation characters that are fragile to get through
# CMake's own string-escaping layer correctly; Python string literals
# make the exact before/after text unambiguous. Idempotent: each
# replace() is a no-op once its target text is already gone (already
# patched), so a second run on an already-patched tree changes nothing.

import sys

wolfssh_dir = sys.argv[1]
port_h = f"{wolfssh_dir}/wolfssh/port.h"
port_c = f"{wolfssh_dir}/src/port.c"

with open(port_h, "r") as f:
    content = f.read()

content = content.replace(
    "#ifdef NO_FILESYSTEM\n",
    "#if 0 /* patched: wolfsftp.c needs the real WFOPEN/WSTAT_T/etc branch, see micropython.cmake */\n",
    1,
)
content = content.replace(
    "    !defined(NO_WOLFSSH_SERVER) && !defined(NO_FILESYSTEM)\n",
    "    !defined(NO_WOLFSSH_SERVER)\n",
    1,
)
# The generic branch's WLSTAT falls back to plain stat() already under
# USE_OSE_API (an unrelated RTOS) -- ESP-IDF's newlib doesn't declare
# lstat() at all (confirmed via an actual build: "implicit declaration
# of function 'lstat'"), so use that same already-existing fallback
# unconditionally. modsftp.c's Client.lstat() doesn't depend on genuine
# don't-follow-symlinks semantics -- this is API completeness/symmetry
# with the SFTP protocol's own LSTAT op, not something this project's
# own code relies on distinguishing from STAT.
content = content.replace(
    "        #define WLSTAT(fs,p,b) lstat((p),(b))\n",
    "        #define WLSTAT(fs,p,b) stat((p),(b))\n",
    1,
)

with open(port_h, "w") as f:
    f.write(content)

with open(port_c, "r") as f:
    content = f.read()

content = content.replace(
    "#if !defined(NO_FILESYSTEM) && !defined(WOLFSSH_USER_FILESYSTEM) && \\\n",
    "#if !defined(WOLFSSH_USER_FILESYSTEM) && \\\n",
    1,
)

with open(port_c, "w") as f:
    f.write(content)
