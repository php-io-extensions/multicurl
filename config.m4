PHP_ARG_ENABLE([multicurl],
  [whether to enable multicurl support],
  [AS_HELP_STRING([--enable-multicurl], [Enable libcurl multi-interface bindings])],
  [no])

if test "$PHP_MULTICURL" != "no"; then
  PKG_CHECK_MODULES([LIBCURL], [libcurl >= 7.69.0])

  dnl multicurl hands ext-curl's CURLM* to libcurl, so it must call the libcurl the PHP
  dnl process already carries, never a second copy. The shared extension links no libcurl
  dnl on any OS: curl_multi_* resolve at load time from the host, which is the libcurl PHP
  dnl or its ext-curl links (Homebrew's dylib, the distribution's .so) or the libcurl compiled
  dnl into a static micro binary. A linked copy on Linux binds multicurl's versioned
  dnl references to the system .so while ext-curl's handles come from the static one.
  dnl Only the headers come from pkg-config.
  PHP_EVAL_INCLINE([$LIBCURL_CFLAGS])

  PHP_NEW_EXTENSION([multicurl], [multicurl.c], [$ext_shared],, [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1])
  PHP_ADD_EXTENSION_DEP([multicurl], [curl])
fi
