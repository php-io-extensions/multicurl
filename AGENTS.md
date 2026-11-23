# Agent guidance — php-io-extensions/multicurl

1. **Read [`.okf/index.md`](.okf/index.md) first** before changing the API, the C, or packaging. Open only the concepts the change touches.
2. **libcurl multi-interface bindings, 1:1.** One public method = one `curl_multi_*` call, named after it (`curl_multi_socket_action` → `Multicurl\Multi::curlMultiSocketAction`). Out-parameters come back as `['res' => CURLMcode, <libcurl param name> => …]`. Linux + macOS; Windows excluded. See [`.okf/api/multi.md`](.okf/api/multi.md).
3. **Not opinionated.** No event loop, no helpers that aren't a libcurl call (`curlSocketStream()` is the one exception, for `php://fd`'s CLI-only gate), no throwing on non-`CURLM_OK` codes. The only exceptions thrown are for a non-`CurlMultiHandle` argument, malformed `extra_fds`, and a socket `curlSocketStream()` cannot duplicate.
4. **Plain C on the Zend API.** No Zephir, no FFI. Sources: `multicurl.c`, `php_multicurl.h`.
5. **The stub is the declaration.** Edit `multicurl.stub.php`, regenerate `multicurl_arginfo.h` with gen_stub, commit both. Never hand-edit arginfo. See [`.okf/runbooks/build.md`](.okf/runbooks/build.md).
6. **Constants** (`CURLMOPT_*`, `CURL_POLL_*`, `CURL_CSELECT_*`, `CURL_SOCKET_TIMEOUT`) live **outside** the extension — callers keep them in their own int-backed enums. C may use libcurl's macros internally.
7. **Handles are ext-curl objects.** `CURLM*` / `CURL*` are read through `zend_object.handlers->offset` after an exact `curl_multi_ce` class check. Never widen that check. Callback state lives in a `ZEND_TLS` registry released by a `free_obj` hook and at RSHUTDOWN. See [`.okf/architecture/module.md`](.okf/architecture/module.md).
8. **Never link libcurl.** `config.m4` takes only libcurl's headers; `curl_multi_*` resolve at load time to the libcurl ext-curl already uses. The module requires ext-curl (`ZEND_MOD_REQUIRED("curl")`).
9. **NTS and ZTS.** Every change builds and passes Pest on Homebrew PHP 8.4 NTS and ZTS (`./install-macos.sh` installs both) and in CI; a module built with one PHP's `phpize` does not load in the other.
10. **Durable facts go in `.okf`.** Update the matching concept, bump its `generated.at`, append `.okf/log.md`. The bundle documents the package, never a session.
