---
type: Architecture
title: Module
description: Scope, file layout, how ext-curl's handles are reached, the callback state and its lifetime, the ext-curl dependency, and why no libcurl is linked.
resource: multicurl.c
tags: [multicurl, c, zend, zts, libcurl, linking]
status: draft
generated: { by: claude-code/claude-opus-5.5, at: 2026-11-23T21:26:10Z }
sources:
  - id: c
    resource: multicurl.c
    title: multicurl.c
  - id: header
    resource: php_multicurl.h
    title: php_multicurl.h
  - id: config
    resource: config.m4
    title: config.m4
  - id: install
    resource: install-macos.sh
    title: install-macos.sh (libcurl header resolution)
  - id: linux-link
    resource: "scope: aarch64 Linux, static glibc micro runtime, 2026-10-08"
    title: Linked libcurl.so.4 measurement
---

# Scope

libcurl's multi interface, 1:1, plain C on the Zend API: one static class `Multicurl\Multi` whose ten methods bind the `curl_multi_*` calls ext-curl leaves out, plus `curlSocketStream()`. It works on ext-curl's own `CurlMultiHandle` and `CurlHandle` objects. No event loop, no constants, no FFI, no throwing on a non-`CURLM_OK` code.[^c] Linux and macOS; Windows is excluded in `composer.json`.

# Files

| File | Holds |
|---|---|
| `multicurl.c` | Everything: handle access, callback state, trampolines, the ten methods, MINIT/RINIT/RSHUTDOWN/MINFO, module entry |
| `php_multicurl.h` | `PHP_MULTICURL_VERSION`, module entry extern, TSRMLS cache |
| `multicurl.stub.php` | The declaration of `Multicurl\Multi`; gen_stub turns it into `multicurl_arginfo.h` |
| `config.m4` | `--enable-multicurl`, libcurl headers, the dependency on ext-curl |

Through 0.11.0 the package was Zephir (`multicurl/multi.zep`, optimizers, a generated `ext/` tree); the C in `src/multi-api.c` behind it is what `multicurl.c` carries now, unchanged in behaviour.

# Reaching libcurl handles

ext-curl's structs are private, but both start with the libcurl handle and end with `zend_object std`:[^c]

* `php_curlm { CURLM *multi; zend_llist easyh; …; zend_object std; }`
* `php_curl  { CURL *cp; …; zend_object std; }`

So `(char *) obj - obj->handlers->offset` is the struct head. `multicurl_multi_ptr()` does this only after `Z_OBJCE_P(mh) == curl_multi_ce`, an exact match; anything else throws `Exception("expected a CurlMultiHandle")`. The socket trampoline maps a `CURL *` back to its `CurlHandle` by walking `easyh` (the zvals ext-curl adds on `curl_multi_add_handle`), dereferencing only entries whose class is exactly `curl_ce`.

# Callback state

A `ZEND_TLS HashTable *` registry keyed by `CURLM *` holds one `multicurl_multi_state { multi, socket_cb, socket_data, timer_cb, timer_data, sockp }` per multi that has used setopt's callback options or `curlMultiAssign()`. `ZEND_TLS` makes it per-thread under ZTS. The registry's destructor owns every zval in a state.[^c]

* libcurl's `SOCKETDATA`/`TIMERDATA` always point at the state; the PHP `*DATA` values live in the state.
* `sockp` owns the zvals given to `curl_multi_assign` (socket → `zval *`); reassigning frees the old one after libcurl accepts the new pointer.
* **free_obj hook.** When a state is created, its multi's `handlers` are swapped for a process-global copy of ext-curl's handlers (copied once, under a pthread mutex) whose `free_obj` is `multicurl_multi_free_obj`. That hook unhooks both `*FUNCTION` options, calls ext-curl's own `free_obj` (`curl_multi_cleanup`), then deletes the state. Every registry entry therefore belongs to a live multi, no state survives address reuse, and the state holds no reference to its multi. `handlers->offset` is copied with the table, so handle access still works.
* RSHUTDOWN detaches the registry (sets it to NULL first, so frees during teardown see none), unhooks every multi still alive, then destroys the states. Multis freed after RSHUTDOWN go straight to ext-curl's `free_obj`.
* The cycle collector cannot see into the registry. A callback that captures its own multi (`use ($m)`) forms `$m → state → closure → $m`, and that multi lives until the option is set back to `null` or the request ends. The timer callback receives the multi as its first argument, so it never needs to capture it; a socket callback that does should be set to `null` before the multi is dropped. A multi whose callbacks don't reference it is freed at once, callbacks and all.

# Dependency on ext-curl

`multicurl.c` uses ext-curl's exported `curl_multi_ce` and `curl_ce` and walks its structs. The module entry declares `ZEND_MOD_REQUIRED("curl")`, so PHP refuses to start multicurl without ext-curl, and `config.m4` adds `PHP_ADD_EXTENSION_DEP([multicurl], [curl])` for static builds.[^c][^config] Where curl is a shared module (Debian's `20-curl.ini`), its ini sorts before `40-multicurl.ini`.

# libcurl: headers only, never linked

The `CURLM*` and `CURL*` handed to multicurl come from the libcurl ext-curl uses, so multicurl must call that same library. `config.m4` takes only the header flags from `pkg-config libcurl` (7.69.0 or newer: `CURLM_BAD_FUNCTION_ARGUMENT` first appears there) and links no libcurl on any OS: `curl_multi_*` resolve at load time from the libcurl the process already carries, which is the dylib Homebrew PHP links, the `.so` a distribution PHP or its `curl.so` links, or the libcurl compiled into a static micro binary.[^config]

A linked copy breaks this. On macOS, linking `/usr/lib/libcurl` (what `curl-config` on `PATH` reports) next to Homebrew's puts two libcurls in one process, with multicurl calling one on the other's structs. On Linux, measured on aarch64 Linux in a static glibc micro runtime: a module linked to `libcurl.so.4` carried versioned references (`curl_multi_socket_action@CURL_OPENSSL_4`) that bound to the system `.so` while the `CURLM*` came from the runtime's libcurl, and the first `curlMultiSocketAction` segfaulted. Unlinked, the references are unversioned and bind to the runtime's libcurl.[^linux-link] The headers should still match the libcurl PHP uses; `install-macos.sh` points `pkg-config` at it from `otool -L` on the PHP binary.[^install]

[^c]: multicurl.c
[^header]: php_multicurl.h
[^config]: config.m4
[^install]: install-macos.sh
[^linux-link]: Linked libcurl.so.4 measurement, 2026-10-08
