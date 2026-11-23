#ifndef PHP_MULTICURL_H
#define PHP_MULTICURL_H

extern zend_module_entry multicurl_module_entry;
#define phpext_multicurl_ptr &multicurl_module_entry

#define PHP_MULTICURL_VERSION "0.11.1"

#if defined(ZTS) && defined(COMPILE_DL_MULTICURL)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_MULTICURL_H */
