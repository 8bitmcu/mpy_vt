/*
 * MicroPython SSH client (wolfSSH-backed)
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 *
 * First real pass, not yet battle-tested. Password and keyboard-interactive
 * auth only, no host key verification yet (matches PocketSSH's own
 * documented limitation: "accepts any server"), client mode only.
 *
 * Architecture: wolfSSH's handshake and steady-state read/write all
 * happen on a dedicated FreeRTOS task pinned to core 1 (APP CPU), same
 * pattern as tdeck_i2s/audioplayer.c's decode task -- leaves mp_task
 * (core 0) completely free. wolfSSL's own examples warn that its crypto
 * operations are heavy enough to trip a watchdog if run on a task that's
 * also expected to do other things promptly (WOLFSSL_ESP_NO_WATCHDOG in
 * wolfSSL's user_settings.h documents the same concern independently).
 *
 * Data crosses the core boundary through two ring_buf_t instances (the
 * same mutex-protected single-producer/single-consumer buffer already
 * shared by audioplayer.c/audiorecorder.c) -- the SSH task never touches
 * MicroPython's own heap/GC directly, only raw bytes through the ring
 * buffers. Keep that boundary absolute; sharing MicroPython objects
 * directly across it is a reliable source of hard-to-diagnose bugs.
 */

#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// xTaskCreatePinnedToCore moved here in ESP-IDF 5.3+ (upstream FreeRTOS SMP
// merge); see modules/tdeck_i2s/audioplayer.c for the same pattern.
#include "freertos/idf_additions.h"

#include "ring_buf.h"

#include <wolfssh/ssh.h>
#include <wolfssh/error.h>
#include <wolfssh/settings.h> // WOLFSSH_MAX_PROMPTS

#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// FreeRTOS task stack is in WORDS here, not bytes (see
// tdeck_i2s/audioplayer.c's AUDIO_TASK_STACK_WORDS) -- 16384 words is
// ~64KB, comfortable headroom for wolfSSH/wolfCrypt's ECC/Ed25519 work
// plus keyboard-interactive auth (WOLFSSH_USERAUTH_KEYBOARD, below),
// which does somewhat more per auth round than plain password.
#define SSH_TASK_STACK_WORDS 16384
#define SSH_TASK_PRIORITY 5

// Matches the DEFAULT_WINDOW_SZ (2000 bytes) wolfSSL's own
// user_settings.h sets once CONFIG_ESP_ENABLE_WOLFSSH is on -- no point
// buffering much more than the SSH window itself allows in flight.
#define SSH_RB_SIZE 4096

// How long the steady-state loop's own select() (see ssh_task() below)
// waits for the socket to become readable before giving up for this
// iteration and draining the tx ring buffer instead. NOT a socket-level
// SO_RCVTIMEO -- that approach reliably broke wolfSSH_stream_read()
// (see ssh_task()'s comment on the steady-state loop for what that
// looked like and how it was diagnosed), regardless of the value used.
// This is plain select() on our own fd, entirely outside wolfSSH's
// control, so it's safe to keep short/responsive.
#define SSH_RECV_TIMEOUT_MS 100

typedef enum {
  SSH_STATE_IDLE = 0,
  SSH_STATE_CONNECTING,
  SSH_STATE_CONNECTED,
  SSH_STATE_FAILED,
  SSH_STATE_CLOSED,
} ssh_state_t;

typedef struct _ssh_client_obj_t {
  mp_obj_base_t base;

  TaskHandle_t task;
  volatile ssh_state_t state;
  volatile bool stop_request;

  ring_buf_t rx; // SSH task (producer) -> MicroPython (consumer)
  ring_buf_t tx; // MicroPython (producer) -> SSH task (consumer)

  // Copied into the object (rather than referencing the Python str
  // objects directly) so they stay valid for the task's whole lifetime,
  // independent of MicroPython's own GC -- the task must never touch a
  // Python object directly.
  char host[64];
  int port;
  char username[32];
  char password[64];

  int sockfd;
  WOLFSSH_CTX *ctx;
  WOLFSSH *ssh;

  // Diagnostic only -- set on the SSH task before it fails, read from
  // Python via error_code() after status() reports FAILED. Negative
  // sentinels (SSH_ERR_*) below cover failures before a WOLFSSH* exists
  // to ask wolfSSH_get_error() about; once one exists, this holds the
  // real WS_* code from wolfssh/error.h.
  int last_error;

  // Diagnostic only -- incremented/set from ssh_user_auth() each time
  // wolfSSH's client engine asks for credentials. Plain struct writes,
  // no console I/O, so safe to set here and read from Python once
  // status() reports FAILED.
  int auth_attempts;
  int last_auth_type;

  // Scratch space for answering WOLFSSH_USERAUTH_KEYBOARD prompts (see
  // ssh_user_auth() below) -- sized to WOLFSSH_MAX_PROMPTS, wolfSSH's
  // own hard cap on how many prompts a single keyboard-interactive
  // round can carry (enforced in DoUserAuthInfoRequest() before our
  // callback ever sees it). Per-instance rather than static/shared so
  // two concurrent Client sessions can't clobber each other's data.
  byte *kb_responses[WOLFSSH_MAX_PROMPTS];
  word32 kb_response_lengths[WOLFSSH_MAX_PROMPTS];
} ssh_client_obj_t;

// Sentinels for failures that happen before wolfSSH_new() gives us a
// WOLFSSH* to call wolfSSH_get_error() on. wolfssh/error.h's own WS_*
// codes run continuously from -1 to at least -1097 as of the wolfSSH
// version this project pins -- these must stay well clear of that
// entire span (not just its current end) so a future wolfSSH release
// adding more codes can't silently collide with one of these.
#define SSH_ERR_DNS -2000
#define SSH_ERR_SOCKET -2001
#define SSH_ERR_CONNECT -2002
#define SSH_ERR_CTX_NEW -2003
#define SSH_ERR_SSH_NEW -2004

const mp_obj_type_t ssh_client_type;

// wolfSSH_SetUserAuth callback. ctx is the owning ssh_client_obj_t
// itself (see wolfSSH_SetUserAuthCtx() below), not just self->password,
// so this can also record auth_attempts/last_auth_type and reach the
// kb_responses scratch space for Python to read back after a failure.
//
// Handles both WOLFSSH_USERAUTH_PASSWORD and WOLFSSH_USERAUTH_KEYBOARD.
// The keyboard-interactive case is required, not optional, even though
// this project only ever sends a plain password: per
// wolfssh/src/internal.c's SendUserAuthRequest(), once a server's
// USERAUTH_FAILURE response offers "keyboard-interactive" alongside
// "password" (which is what PAM-backed OpenSSH servers commonly do even
// with plain password auth enabled), wolfSSH's client unconditionally
// prefers keyboard-interactive with no fallback back to password -- so
// a password-only callback here means every auth round goes through
// keyboard-interactive and this callback never even gets asked for
// PASSWORD. Answering every prompt with the stored password covers the
// overwhelmingly common real-world case (server presents a single
// "Password:" prompt via keyboard-interactive).
static int ssh_user_auth(byte authType, WS_UserAuthData *authData, void *ctx) {
  ssh_client_obj_t *self = (ssh_client_obj_t *)ctx;
  self->auth_attempts++;
  self->last_auth_type = (int)authType;

  if (authType == WOLFSSH_USERAUTH_PASSWORD) {
    authData->sf.password.password = (byte *)self->password;
    authData->sf.password.passwordSz = (word32)strlen(self->password);
    return WOLFSSH_USERAUTH_SUCCESS;
  }

  if (authType == WOLFSSH_USERAUTH_KEYBOARD) {
    // DoUserAuthInfoRequest() (internal.c) already rejects any server
    // that sends more than WOLFSSH_MAX_PROMPTS prompts before this
    // callback ever runs, so promptCount is always within
    // kb_responses'/kb_response_lengths' bounds here.
    word32 count = authData->sf.keyboard.promptCount;
    word32 passwordSz = (word32)strlen(self->password);
    for (word32 i = 0; i < count; i++) {
      self->kb_responses[i] = (byte *)self->password;
      self->kb_response_lengths[i] = passwordSz;
    }
    authData->sf.keyboard.responseCount = count;
    authData->sf.keyboard.responses = self->kb_responses;
    authData->sf.keyboard.responseLengths = self->kb_response_lengths;
    return WOLFSSH_USERAUTH_SUCCESS;
  }

  return WOLFSSH_USERAUTH_FAILURE;
}

static void ssh_task(void *arg) {
  ssh_client_obj_t *self = (ssh_client_obj_t *)arg;

  static bool wolfssh_lib_initialized = false;
  if (!wolfssh_lib_initialized) {
    wolfSSH_Init();
    wolfssh_lib_initialized = true;
  }

  self->sockfd = -1;
  self->ctx = NULL;
  self->ssh = NULL;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", self->port);

  struct addrinfo *res = NULL;
  if (getaddrinfo(self->host, port_str, &hints, &res) != 0 || res == NULL) {
    self->last_error = SSH_ERR_DNS;
    self->state = SSH_STATE_FAILED;
    goto done;
  }

  self->sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (self->sockfd < 0) {
    freeaddrinfo(res);
    self->last_error = SSH_ERR_SOCKET;
    self->state = SSH_STATE_FAILED;
    goto done;
  }

  if (connect(self->sockfd, res->ai_addr, res->ai_addrlen) != 0) {
    freeaddrinfo(res);
    self->last_error = SSH_ERR_CONNECT;
    self->state = SSH_STATE_FAILED;
    goto done;
  }
  freeaddrinfo(res);

  self->ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
  if (self->ctx == NULL) {
    self->last_error = SSH_ERR_CTX_NEW;
    self->state = SSH_STATE_FAILED;
    goto done;
  }

  wolfSSH_SetUserAuth(self->ctx, ssh_user_auth);

  self->ssh = wolfSSH_new(self->ctx);
  if (self->ssh == NULL) {
    self->last_error = SSH_ERR_SSH_NEW;
    self->state = SSH_STATE_FAILED;
    goto done;
  }

  wolfSSH_SetUserAuthCtx(self->ssh, (void *)self);
  wolfSSH_set_fd(self->ssh, self->sockfd);
  wolfSSH_SetUsername(self->ssh, self->username);

  // Interactive shell/PTY, not a one-shot command -- matches this
  // project's telnet.py-style "bridge a remote session into env.term"
  // use case rather than exec-one-command-and-exit. Must happen BEFORE
  // wolfSSH_connect(), not after -- wolfSSH_connect()'s own internal
  // state machine (src/ssh.c) drives the entire channel-open +
  // terminal-request + shell-request sequence itself, all before it
  // returns, checking ssh->sendTerminalRequest (set by this call) partway
  // through. Calling this after connect() returns is too late: that flag
  // is still 0 when the state machine checks it, so it sends a bare
  // "shell" request with no pty-req instead.
  wolfSSH_SetChannelType(self->ssh, WOLFSSH_SESSION_TERMINAL, NULL, 0);

  {
    int rc = wolfSSH_connect(self->ssh);
    if (rc != WS_SUCCESS) {
      self->last_error = wolfSSH_get_error(self->ssh);
      self->state = SSH_STATE_FAILED;
      goto done;
    }
  }

  self->state = SSH_STATE_CONNECTED;

  // Steady state: deliberately NOT using SO_RCVTIMEO here -- a
  // socket-level receive timeout makes wolfSSH's OWN internal recv()
  // call return EAGAIN/EWOULDBLOCK, and wolfSSH_stream_read() mishandles
  // that specific case: it returns the generic WS_ERROR instead of
  // WS_WANT_READ, killing the session outright, regardless of the
  // timeout's length. Instead, the socket stays plain blocking with no
  // timeout at all, and THIS loop uses its own select() to decide
  // whether there's actually data worth reading before ever calling
  // into wolfSSH -- so wolfSSH's
  // internal recv() only ever runs when data is already sitting in the
  // socket buffer (returns essentially immediately; the buggy timeout
  // path never triggers), while still giving this loop a chance to
  // drain the tx ring buffer on a short, predictable interval that's
  // entirely outside wolfSSH's own control.
  uint8_t iobuf[256];
  while (!self->stop_request) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(self->sockfd, &rfds);
    struct timeval tv;
    tv.tv_sec = SSH_RECV_TIMEOUT_MS / 1000;
    tv.tv_usec = (SSH_RECV_TIMEOUT_MS % 1000) * 1000;
    int sel = select(self->sockfd + 1, &rfds, NULL, NULL, &tv);

    if (sel > 0 && FD_ISSET(self->sockfd, &rfds)) {
      int n = wolfSSH_stream_read(self->ssh, iobuf, sizeof(iobuf));
      if (n > 0) {
        rb_write(&self->rx, iobuf, (size_t)n);
      } else if (n != WS_WANT_READ) {
        // WS_EOF / WS_CHANNEL_CLOSED / WS_DISCONNECT / any other error --
        // all mean this session is over.
        self->last_error = wolfSSH_get_error(self->ssh);
        break;
      }
    }

    size_t avail = rb_available(&self->tx);
    if (avail > 0) {
      size_t chunk = avail < sizeof(iobuf) ? avail : sizeof(iobuf);
      size_t got = rb_read(&self->tx, iobuf, chunk);
      if (got > 0) {
        wolfSSH_stream_send(self->ssh, iobuf, (word32)got);
      }
    }
  }

  self->state = SSH_STATE_CLOSED;

done:
  if (self->ssh) {
    wolfSSH_shutdown(self->ssh);
    wolfSSH_free(self->ssh);
    self->ssh = NULL;
  }
  if (self->ctx) {
    wolfSSH_CTX_free(self->ctx);
    self->ctx = NULL;
  }
  if (self->sockfd >= 0) {
    close(self->sockfd);
    self->sockfd = -1;
  }
  self->task = NULL;
  vTaskDelete(NULL);
}

static mp_obj_t ssh_client_make_new(const mp_obj_type_t *type, size_t n_args,
                                    size_t n_kw, const mp_obj_t *args) {
  ssh_client_obj_t *self = m_new_obj(ssh_client_obj_t);
  self->base.type = type;
  self->task = NULL;
  self->state = SSH_STATE_IDLE;
  self->stop_request = false;
  self->sockfd = -1;
  self->ctx = NULL;
  self->ssh = NULL;
  self->last_error = 0;
  self->auth_attempts = 0;
  self->last_auth_type = -1;

  if (!rb_init(&self->rx, SSH_RB_SIZE) || !rb_init(&self->tx, SSH_RB_SIZE)) {
    mp_raise_msg(&mp_type_MemoryError,
                MP_ERROR_TEXT("failed to allocate ssh ring buffers"));
  }

  return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t ssh_client_connect(size_t n_args, const mp_obj_t *args) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(args[0]);

  if (self->state == SSH_STATE_CONNECTING || self->state == SSH_STATE_CONNECTED) {
    mp_raise_msg(&mp_type_RuntimeError,
                MP_ERROR_TEXT("already connecting or connected"));
  }

  const char *host = mp_obj_str_get_str(args[1]);
  mp_int_t port = mp_obj_get_int(args[2]);
  const char *username = mp_obj_str_get_str(args[3]);
  const char *password = mp_obj_str_get_str(args[4]);

  strncpy(self->host, host, sizeof(self->host) - 1);
  self->host[sizeof(self->host) - 1] = '\0';
  self->port = (int)port;
  strncpy(self->username, username, sizeof(self->username) - 1);
  self->username[sizeof(self->username) - 1] = '\0';
  strncpy(self->password, password, sizeof(self->password) - 1);
  self->password[sizeof(self->password) - 1] = '\0';

  rb_reset(&self->rx);
  rb_reset(&self->tx);
  self->stop_request = false;
  self->last_error = 0;
  self->auth_attempts = 0;
  self->last_auth_type = -1;
  self->state = SSH_STATE_CONNECTING;

  BaseType_t ok = xTaskCreatePinnedToCore(
      ssh_task, "sshclient", SSH_TASK_STACK_WORDS, self, SSH_TASK_PRIORITY,
      &self->task, 1 /* APP CPU -- leave PRO CPU/core 0 for MicroPython */);

  if (ok != pdPASS) {
    self->state = SSH_STATE_FAILED;
    mp_raise_msg(&mp_type_RuntimeError,
                MP_ERROR_TEXT("failed to start ssh task"));
  }

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ssh_client_connect_obj, 5, 5,
                                           ssh_client_connect);

static mp_obj_t ssh_client_status(mp_obj_t self_in) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  return mp_obj_new_int(self->state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(ssh_client_status_obj, ssh_client_status);

// Diagnostic accessor for the last_error field set by ssh_task() -- see
// its declaration above for what the SSH_ERR_* sentinels vs. real WS_*
// codes (from wolfssh/error.h) mean. Only meaningful once status() has
// reported FAILED.
static mp_obj_t ssh_client_error_code(mp_obj_t self_in) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  return mp_obj_new_int(self->last_error);
}
static MP_DEFINE_CONST_FUN_OBJ_1(ssh_client_error_code_obj, ssh_client_error_code);

// Diagnostic accessors for the auth_attempts/last_auth_type fields set
// by ssh_user_auth() -- see its declaration above. authType is one of
// wolfSSH's WOLFSSH_USERAUTH_* bitmask values (wolfssh/ssh.h); -1 means
// the callback was never invoked (e.g. failed before auth started).
static mp_obj_t ssh_client_auth_attempts(mp_obj_t self_in) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  return mp_obj_new_int(self->auth_attempts);
}
static MP_DEFINE_CONST_FUN_OBJ_1(ssh_client_auth_attempts_obj, ssh_client_auth_attempts);

static mp_obj_t ssh_client_last_auth_type(mp_obj_t self_in) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  return mp_obj_new_int(self->last_auth_type);
}
static MP_DEFINE_CONST_FUN_OBJ_1(ssh_client_last_auth_type_obj, ssh_client_last_auth_type);

static mp_obj_t ssh_client_disconnect(mp_obj_t self_in) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  self->stop_request = true;
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(ssh_client_disconnect_obj, ssh_client_disconnect);

// --- Stream protocol: readinto() drains decrypted output, write() queues
// keystrokes to send. Both just move bytes through the ring buffers --
// the actual wolfSSH calls only ever happen on the dedicated task.

static mp_uint_t ssh_client_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                                 int *errcode) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  size_t n = rb_read(&self->rx, (uint8_t *)buf, size);
  if (n == 0) {
    *errcode = MP_EAGAIN;
    return MP_STREAM_ERROR;
  }
  return n;
}

static mp_uint_t ssh_client_write(mp_obj_t self_in, const void *buf,
                                  mp_uint_t size, int *errcode) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  size_t n = rb_write(&self->tx, (const uint8_t *)buf, size);
  if (n == 0) {
    *errcode = MP_EAGAIN;
    return MP_STREAM_ERROR;
  }
  return n;
}

static mp_uint_t ssh_client_ioctl(mp_obj_t self_in, mp_uint_t request,
                                  uintptr_t arg, int *errcode) {
  ssh_client_obj_t *self = MP_OBJ_TO_PTR(self_in);
  if (request == MP_STREAM_POLL) {
    uintptr_t flags = arg;
    uintptr_t ret = 0;
    if ((flags & MP_STREAM_POLL_RD) && rb_available(&self->rx) > 0) {
      ret |= MP_STREAM_POLL_RD;
    }
    if ((flags & MP_STREAM_POLL_WR) && rb_free_space(&self->tx) > 0) {
      ret |= MP_STREAM_POLL_WR;
    }
    return ret;
  }
  return 0;
}

static const mp_stream_p_t ssh_client_stream_p = {
    .read = ssh_client_read,
    .write = ssh_client_write,
    .ioctl = ssh_client_ioctl,
    .is_text = false,
};

static const mp_rom_map_elem_t ssh_client_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&ssh_client_connect_obj)},
    {MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&ssh_client_status_obj)},
    {MP_ROM_QSTR(MP_QSTR_error_code), MP_ROM_PTR(&ssh_client_error_code_obj)},
    {MP_ROM_QSTR(MP_QSTR_auth_attempts), MP_ROM_PTR(&ssh_client_auth_attempts_obj)},
    {MP_ROM_QSTR(MP_QSTR_last_auth_type), MP_ROM_PTR(&ssh_client_last_auth_type_obj)},
    {MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&ssh_client_disconnect_obj)},
    {MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&mp_stream_ioctl_obj)},
};
static MP_DEFINE_CONST_DICT(ssh_client_locals_dict, ssh_client_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(ssh_client_type, MP_QSTR_Client,
                         MP_TYPE_FLAG_ITER_IS_STREAM, make_new,
                         ssh_client_make_new, protocol, &ssh_client_stream_p,
                         locals_dict, &ssh_client_locals_dict);

static const mp_rom_map_elem_t modssh_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_modssh)},
    {MP_ROM_QSTR(MP_QSTR_Client), MP_ROM_PTR(&ssh_client_type)},
    {MP_ROM_QSTR(MP_QSTR_IDLE), MP_ROM_INT(SSH_STATE_IDLE)},
    {MP_ROM_QSTR(MP_QSTR_CONNECTING), MP_ROM_INT(SSH_STATE_CONNECTING)},
    {MP_ROM_QSTR(MP_QSTR_CONNECTED), MP_ROM_INT(SSH_STATE_CONNECTED)},
    {MP_ROM_QSTR(MP_QSTR_FAILED), MP_ROM_INT(SSH_STATE_FAILED)},
    {MP_ROM_QSTR(MP_QSTR_CLOSED), MP_ROM_INT(SSH_STATE_CLOSED)},
};
static MP_DEFINE_CONST_DICT(modssh_module_globals, modssh_module_globals_table);

const mp_obj_module_t modssh_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&modssh_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_modssh, modssh_module);
