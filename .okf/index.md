---
okf_version: "0.2"
---

# ext-multicurl

PHP extension (Linux + macOS, NTS + ZTS, PHP 8.4+), plain C on the Zend API: one static class `Multicurl\Multi` whose methods are 1:1 bindings for the libcurl multi calls ext-curl does not expose — `curl_multi_socket_action`, the `CURLMOPT_SOCKETFUNCTION`/`TIMERFUNCTION` callbacks, `curl_multi_fdset`, `curl_multi_timeout`, `curl_multi_poll`, `curl_multi_wait`, `curl_multi_wakeup`, `curl_multi_assign`, `curl_multi_strerror` — plus `curlSocketStream`, a stream on a duplicate of a curl socket (`php://fd` without its CLI-only gate). It works on ext-curl's own `CurlMultiHandle`/`CurlHandle` objects. No FFI. Version **0.11.1**.

All concepts are `status: draft` pending maintainer verification.

* [Module](architecture/module.md) - Scope, files, handle access via `handlers->offset`, callback state and its lifetime, the ext-curl dependency, no linked libcurl.

# API

* [Multicurl\\Multi](api/multi.md) - Methods, return shapes, setopt and callback signatures, errors.

# Runbooks

* [Build, install, test](runbooks/build.md) - gen_stub after a stub edit, builds per PHP, the Mac installer, Pest on NTS and ZTS, CI matrix, PIE.
