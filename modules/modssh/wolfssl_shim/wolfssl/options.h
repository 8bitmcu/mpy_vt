/*
 * Shim for wolfssh's ssh.h, which does an unconditional
 * `#include <wolfssl/options.h>` -- that file doesn't exist in this
 * ESP-IDF packaging of wolfSSL (wolfssl__wolfssl only ships
 * include/user_settings.h, the WOLFSSL_USER_SETTINGS model, no generated
 * options.h). This redirects to the real, already-correctly-configured
 * user_settings.h instead of needing to patch wolfssh itself.
 */
#include "user_settings.h"
