#!/usr/bin/env python3
# Patches wolfSSH's client auth-method selection (src/internal.c,
# SendUserAuthRequest()) to prefer password over keyboard-interactive
# when a server offers both, instead of wolfSSH's built-in default of
# always preferring keyboard-interactive with no fallback to password.
#
# Why this is needed: many OpenSSH servers offer "publickey,password"
# and "keyboard-interactive" together even with plain password auth
# enabled (PAM routes it that way by default). wolfSSH's client always
# picks keyboard-interactive when offered, sending a bare (empty
# submethods) declaration -- some servers' keyboard-interactive/PAM
# backends reject that outright rather than sending back real prompts,
# which breaks auth entirely even though plain password would have
# worked fine. This patch makes the client skip keyboard-interactive
# whenever password is also on offer, while leaving it intact as a
# genuine fallback for servers that offer ONLY keyboard-interactive
# (modssh.c's ssh_user_auth() already answers real prompts correctly
# for that case).
#
# Invoked from micropython.cmake via execute_process(), same as the
# NO_FILESYSTEM pty-req patches there. Idempotent: checks for its own
# marker comment before patching, so a second run on an already-patched
# tree is a no-op.

import sys

wolfssh_dir = sys.argv[1]
path = f"{wolfssh_dir}/src/internal.c"

MARKER = "modssh: prefer password over keyboard-interactive"
ANCHOR = "keySig_ptr->heap = ssh->ctx->heap;"

with open(path, "r") as f:
    lines = f.readlines()

if not any(MARKER in line for line in lines):
    anchor_idx = None
    indent = ""
    for i, line in enumerate(lines):
        if ANCHOR in line:
            anchor_idx = i
            indent = line[: len(line) - len(line.lstrip())]
            break

    if anchor_idx is None:
        sys.exit(f"modssh patch: anchor text not found in {path}")

    patch_lines = [
        f"{indent}if ((authType & WOLFSSH_USERAUTH_PASSWORD) &&\n",
        f"{indent}        (authType & WOLFSSH_USERAUTH_KEYBOARD)) {{\n",
        f"{indent}    authType &= ~WOLFSSH_USERAUTH_KEYBOARD; /* {MARKER} */\n",
        f"{indent}}}\n",
    ]
    lines[anchor_idx + 1 : anchor_idx + 1] = patch_lines

    with open(path, "w") as f:
        f.writelines(lines)
