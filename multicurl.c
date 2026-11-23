/*
 * multicurl: 1:1 bindings of the libcurl multi calls ext-curl does not expose,
 * as static methods of Multicurl\Multi that take ext-curl's own handles.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/curl/php_curl.h"
#include "zend_exceptions.h"
#include "php_multicurl.h"
#include "multicurl_arginfo.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>

/*
 * ext-curl keeps its structs private. Both begin with the libcurl handle, and
 * zend_object std is last, so handlers->offset walks back to the struct head:
 *   php_curlm { CURLM *multi; zend_llist easyh; ...; zend_object std; }
 *   php_curl  { CURL *cp; ...; zend_object std; }
 * easyh holds a zval per CurlHandle added with curl_multi_add_handle().
 */
typedef struct {
	CURLM      *multi;
	zend_llist  easyh;
} multicurl_curlm_head;

typedef struct {
	zend_object *multi;        /* always live: the entry is dropped from the multi's free_obj */
	zval         socket_cb;
	zval         socket_data;
	zval         timer_cb;
	zval         timer_data;
	HashTable    sockp;        /* curl_socket_t => zval* handed to curl_multi_assign() */
} multicurl_multi_state;

ZEND_TLS HashTable *multicurl_registry = NULL;   /* (uintptr_t) CURLM* => multicurl_multi_state* */

/* Tracked CurlMultiHandles get a copy of ext-curl's handlers whose free_obj drops their state. */
static zend_object_handlers   multicurl_multi_handlers;
static zend_object_free_obj_t multicurl_multi_free_obj_orig = NULL;
static pthread_mutex_t        multicurl_multi_handlers_lock = PTHREAD_MUTEX_INITIALIZER;

static void multicurl_sockp_dtor(zval *zv)
{
	zval *p = Z_PTR_P(zv);

	zval_ptr_dtor(p);
	efree(p);
}

static void multicurl_state_dtor(zval *zv)
{
	multicurl_multi_state *st = Z_PTR_P(zv);

	zval_ptr_dtor(&st->socket_cb);
	zval_ptr_dtor(&st->socket_data);
	zval_ptr_dtor(&st->timer_cb);
	zval_ptr_dtor(&st->timer_data);
	zend_hash_destroy(&st->sockp);
	efree(st);
}

static CURLM *multicurl_obj_multi(zend_object *obj)
{
	return ((multicurl_curlm_head *) ((char *) obj - obj->handlers->offset))->multi;
}

/* libcurl's SOCKETDATA/TIMERDATA point at the state; unhook before the state can go. */
static void multicurl_multi_unhook(CURLM *m)
{
	curl_multi_setopt(m, CURLMOPT_SOCKETFUNCTION, NULL);
	curl_multi_setopt(m, CURLMOPT_TIMERFUNCTION, NULL);
}

static void multicurl_multi_free_obj(zend_object *obj)
{
	CURLM *m = multicurl_obj_multi(obj);
	zend_ulong key = (zend_ulong) (uintptr_t) m;
	bool tracked = m && multicurl_registry && zend_hash_index_exists(multicurl_registry, key);

	if (tracked) {
		multicurl_multi_unhook(m);
	}
	multicurl_multi_free_obj_orig(obj);
	if (tracked && multicurl_registry) {
		zend_hash_index_del(multicurl_registry, key);
	}
}

static void multicurl_multi_track(zend_object *obj)
{
	pthread_mutex_lock(&multicurl_multi_handlers_lock);
	if (!multicurl_multi_free_obj_orig) {
		memcpy(&multicurl_multi_handlers, obj->handlers, sizeof(multicurl_multi_handlers));
		multicurl_multi_free_obj_orig = obj->handlers->free_obj;
		multicurl_multi_handlers.free_obj = multicurl_multi_free_obj;
	}
	pthread_mutex_unlock(&multicurl_multi_handlers_lock);

	if (obj->handlers->free_obj == multicurl_multi_free_obj_orig) {
		obj->handlers = &multicurl_multi_handlers;
	}
}

/* The CURLM* behind $multi, or NULL with an exception thrown when $multi is not exactly a CurlMultiHandle. */
static CURLM *multicurl_multi_ptr(zval *mh)
{
	if (Z_TYPE_P(mh) != IS_OBJECT || Z_OBJCE_P(mh) != curl_multi_ce) {
		zend_throw_exception(zend_ce_exception, "expected a CurlMultiHandle", 0);
		return NULL;
	}

	return multicurl_obj_multi(Z_OBJ_P(mh));
}

static multicurl_multi_state *multicurl_state_for(zval *mh, CURLM *m)
{
	multicurl_multi_state *st;

	if (!multicurl_registry) {
		ALLOC_HASHTABLE(multicurl_registry);
		zend_hash_init(multicurl_registry, 8, NULL, multicurl_state_dtor, 0);
	}

	st = zend_hash_index_find_ptr(multicurl_registry, (zend_ulong) (uintptr_t) m);
	if (!st) {
		st = ecalloc(1, sizeof(*st));
		st->multi = Z_OBJ_P(mh);
		ZVAL_UNDEF(&st->socket_cb);
		ZVAL_NULL(&st->socket_data);
		ZVAL_UNDEF(&st->timer_cb);
		ZVAL_NULL(&st->timer_data);
		zend_hash_init(&st->sockp, 8, NULL, multicurl_sockp_dtor, 0);
		zend_hash_index_add_new_ptr(multicurl_registry, (zend_ulong) (uintptr_t) m, st);
		multicurl_multi_track(st->multi);
	}

	return st;
}

static void multicurl_easy_zval(zval *out, multicurl_multi_state *st, CURL *e)
{
	multicurl_curlm_head *head = (multicurl_curlm_head *) ((char *) st->multi - st->multi->handlers->offset);
	zend_llist_position pos;
	zval *z;

	for (z = zend_llist_get_first_ex(&head->easyh, &pos); z; z = zend_llist_get_next_ex(&head->easyh, &pos)) {
		zend_object *obj;

		if (Z_TYPE_P(z) != IS_OBJECT || Z_OBJCE_P(z) != curl_ce) {
			continue;
		}
		obj = Z_OBJ_P(z);
		if (*(CURL **) ((char *) obj - obj->handlers->offset) == e) {
			ZVAL_OBJ(out, obj);
			return;
		}
	}
	ZVAL_NULL(out);
}

static int multicurl_callback_result(zval *rv, zend_result called)
{
	int res = 0;

	if (called != SUCCESS || EG(exception)) {
		res = -1;
	} else if (Z_TYPE_P(rv) != IS_UNDEF && Z_TYPE_P(rv) != IS_NULL) {
		res = (int) zval_get_long(rv);
	}
	zval_ptr_dtor(rv);

	return res;
}

/* curl_socket_callback: (CURL *easy, curl_socket_t s, int what, void *clientp, void *socketp) */
static int multicurl_socket_trampoline(CURL *e, curl_socket_t s, int what, void *clientp, void *socketp)
{
	multicurl_multi_state *st = clientp;
	zval args[5], rv;

	if (Z_ISUNDEF(st->socket_cb)) {
		return 0;
	}

	multicurl_easy_zval(&args[0], st, e);
	ZVAL_LONG(&args[1], (zend_long) s);
	ZVAL_LONG(&args[2], (zend_long) what);
	ZVAL_COPY_VALUE(&args[3], &st->socket_data);
	if (socketp) {
		ZVAL_COPY_VALUE(&args[4], (zval *) socketp);
	} else {
		ZVAL_NULL(&args[4]);
	}
	ZVAL_UNDEF(&rv);

	return multicurl_callback_result(&rv, call_user_function(NULL, NULL, &st->socket_cb, &rv, 5, args));
}

/* curl_multi_timer_callback: (CURLM *multi, long timeout_ms, void *clientp) */
static int multicurl_timer_trampoline(CURLM *m, long timeout_ms, void *clientp)
{
	multicurl_multi_state *st = clientp;
	zval args[3], rv;

	(void) m;
	if (Z_ISUNDEF(st->timer_cb)) {
		return 0;
	}

	ZVAL_OBJ(&args[0], st->multi);
	ZVAL_LONG(&args[1], (zend_long) timeout_ms);
	ZVAL_COPY_VALUE(&args[2], &st->timer_data);
	ZVAL_UNDEF(&rv);

	return multicurl_callback_result(&rv, call_user_function(NULL, NULL, &st->timer_cb, &rv, 3, args));
}

static CURLMcode multicurl_store_callback(CURLM *m, zval *slot, zval *value, CURLMoption fn_opt, CURLMoption data_opt, void *trampoline, multicurl_multi_state *st)
{
	if (Z_TYPE_P(value) != IS_NULL && !zend_is_callable(value, 0, NULL)) {
		return CURLM_BAD_FUNCTION_ARGUMENT;
	}

	zval_ptr_dtor(slot);
	ZVAL_UNDEF(slot);

	if (Z_TYPE_P(value) == IS_NULL) {
		return curl_multi_setopt(m, fn_opt, NULL);
	}

	ZVAL_COPY(slot, value);
	curl_multi_setopt(m, data_opt, st);

	return curl_multi_setopt(m, fn_opt, trampoline);
}

static void multicurl_store_data(zval *slot, zval *value)
{
	zval_ptr_dtor(slot);
	ZVAL_COPY(slot, value);
}

PHP_METHOD(Multicurl_Multi, curlMultiSetopt)
{
	zval *mh, *value;
	zend_long opt;
	multicurl_multi_state *st;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ZVAL(mh)
		Z_PARAM_LONG(opt)
		Z_PARAM_ZVAL(value)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	switch (opt) {
		case CURLMOPT_SOCKETFUNCTION:
			st = multicurl_state_for(mh, m);
			RETURN_LONG(multicurl_store_callback(m, &st->socket_cb, value, CURLMOPT_SOCKETFUNCTION, CURLMOPT_SOCKETDATA, (void *) multicurl_socket_trampoline, st));
		case CURLMOPT_TIMERFUNCTION:
			st = multicurl_state_for(mh, m);
			RETURN_LONG(multicurl_store_callback(m, &st->timer_cb, value, CURLMOPT_TIMERFUNCTION, CURLMOPT_TIMERDATA, (void *) multicurl_timer_trampoline, st));
		case CURLMOPT_SOCKETDATA:
			multicurl_store_data(&multicurl_state_for(mh, m)->socket_data, value);
			RETURN_LONG(CURLM_OK);
		case CURLMOPT_TIMERDATA:
			multicurl_store_data(&multicurl_state_for(mh, m)->timer_data, value);
			RETURN_LONG(CURLM_OK);
	}

	if (opt < CURLOPTTYPE_OBJECTPOINT) {
		RETURN_LONG(curl_multi_setopt(m, (CURLMoption) opt, (long) zval_get_long(value)));
	}

	RETURN_LONG(CURLM_UNKNOWN_OPTION);
}

PHP_METHOD(Multicurl_Multi, curlMultiSocketAction)
{
	zval *mh;
	zend_long s, ev_bitmask;
	int running = 0;
	CURLMcode res;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ZVAL(mh)
		Z_PARAM_LONG(s)
		Z_PARAM_LONG(ev_bitmask)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	res = curl_multi_socket_action(m, (curl_socket_t) s, (int) ev_bitmask, &running);

	array_init(return_value);
	add_assoc_long(return_value, "res", (zend_long) res);
	add_assoc_long(return_value, "running_handles", (zend_long) running);
}

PHP_METHOD(Multicurl_Multi, curlMultiFdset)
{
	zval *mh, zr, zw, ze;
	fd_set r, w, e;
	int max_fd = -1, i;
	CURLMcode res;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(mh)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	FD_ZERO(&r);
	FD_ZERO(&w);
	FD_ZERO(&e);
	res = curl_multi_fdset(m, &r, &w, &e, &max_fd);

	array_init(&zr);
	array_init(&zw);
	array_init(&ze);
	for (i = 0; i <= max_fd && i < FD_SETSIZE; i++) {
		if (FD_ISSET(i, &r)) add_next_index_long(&zr, i);
		if (FD_ISSET(i, &w)) add_next_index_long(&zw, i);
		if (FD_ISSET(i, &e)) add_next_index_long(&ze, i);
	}

	array_init(return_value);
	add_assoc_long(return_value, "res", (zend_long) res);
	add_assoc_zval(return_value, "read_fd_set", &zr);
	add_assoc_zval(return_value, "write_fd_set", &zw);
	add_assoc_zval(return_value, "exc_fd_set", &ze);
	add_assoc_long(return_value, "max_fd", (zend_long) max_fd);
}

PHP_METHOD(Multicurl_Multi, curlMultiTimeout)
{
	zval *mh;
	long timeout = -1;
	CURLMcode res;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(mh)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	res = curl_multi_timeout(m, &timeout);

	array_init(return_value);
	add_assoc_long(return_value, "res", (zend_long) res);
	add_assoc_long(return_value, "timeout", (zend_long) timeout);
}

/* extra_fds: list of ['fd' => int, 'events' => int] mirroring struct curl_waitfd; 'revents' is filled on return. */
static void multicurl_multi_waitfds(INTERNAL_FUNCTION_PARAMETERS, bool use_poll)
{
	zval *mh, *v, *f, zfds;
	HashTable *ht;
	zend_long timeout_ms;
	struct curl_waitfd *fds = NULL;
	unsigned int n, i = 0;
	int numfds = 0;
	CURLMcode res;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ZVAL(mh)
		Z_PARAM_ARRAY_HT(ht)
		Z_PARAM_LONG(timeout_ms)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	n = zend_hash_num_elements(ht);
	if (n) {
		fds = ecalloc(n, sizeof(*fds));
	}

	ZEND_HASH_FOREACH_VAL(ht, v) {
		ZVAL_DEREF(v);
		if (Z_TYPE_P(v) != IS_ARRAY || !(f = zend_hash_str_find(Z_ARRVAL_P(v), "fd", sizeof("fd") - 1))) {
			if (fds) efree(fds);
			zend_throw_exception(zend_ce_exception, "each extra_fds entry needs an 'fd' key", 0);
			RETURN_THROWS();
		}
		fds[i].fd = (curl_socket_t) zval_get_long(f);
		f = zend_hash_str_find(Z_ARRVAL_P(v), "events", sizeof("events") - 1);
		fds[i].events = f ? (short) zval_get_long(f) : 0;
		i++;
	} ZEND_HASH_FOREACH_END();

	res = use_poll
		? curl_multi_poll(m, fds, n, (int) timeout_ms, &numfds)
		: curl_multi_wait(m, fds, n, (int) timeout_ms, &numfds);

	array_init_size(&zfds, n);
	for (i = 0; i < n; i++) {
		zval entry;

		array_init_size(&entry, 3);
		add_assoc_long(&entry, "fd", (zend_long) fds[i].fd);
		add_assoc_long(&entry, "events", (zend_long) fds[i].events);
		add_assoc_long(&entry, "revents", (zend_long) fds[i].revents);
		add_next_index_zval(&zfds, &entry);
	}
	if (fds) {
		efree(fds);
	}

	array_init(return_value);
	add_assoc_long(return_value, "res", (zend_long) res);
	add_assoc_long(return_value, "numfds", (zend_long) numfds);
	add_assoc_zval(return_value, "extra_fds", &zfds);
}

PHP_METHOD(Multicurl_Multi, curlMultiPoll)
{
	multicurl_multi_waitfds(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

PHP_METHOD(Multicurl_Multi, curlMultiWait)
{
	multicurl_multi_waitfds(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

PHP_METHOD(Multicurl_Multi, curlMultiWakeup)
{
	zval *mh;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(mh)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) curl_multi_wakeup(m));
}

PHP_METHOD(Multicurl_Multi, curlMultiAssign)
{
	zval *mh, *sockp, *p = NULL;
	zend_long s;
	multicurl_multi_state *st;
	zend_ulong key;
	CURLMcode res;
	CURLM *m;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ZVAL(mh)
		Z_PARAM_LONG(s)
		Z_PARAM_ZVAL(sockp)
	ZEND_PARSE_PARAMETERS_END();

	if (!(m = multicurl_multi_ptr(mh))) {
		RETURN_THROWS();
	}

	st = multicurl_state_for(mh, m);
	key = (zend_ulong) s;

	if (Z_TYPE_P(sockp) != IS_NULL) {
		p = emalloc(sizeof(zval));
		ZVAL_COPY(p, sockp);
	}

	res = curl_multi_assign(m, (curl_socket_t) key, p);
	if (res != CURLM_OK) {
		if (p) {
			zval_ptr_dtor(p);
			efree(p);
		}
		RETURN_LONG((zend_long) res);
	}

	if (p) {
		zend_hash_index_update_ptr(&st->sockp, key, p);
	} else {
		zend_hash_index_del(&st->sockp, key);
	}

	RETURN_LONG((zend_long) res);
}

PHP_METHOD(Multicurl_Multi, curlMultiStrerror)
{
	zend_long code;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(code)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_STRING(curl_multi_strerror((CURLMcode) code));
}

/*
 * A PHP stream on a duplicate of curl's socket s, for a loop to wait on: what
 * php://fd/<s> does, minus its CLI-only gate. curl keeps the socket; closing
 * the stream closes only the duplicate.
 */
PHP_METHOD(Multicurl_Multi, curlSocketStream)
{
	zend_long s;
	php_stream *stream;
	int fd;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(s)
	ZEND_PARSE_PARAMETERS_END();

	fd = dup((int) s);
	if (fd < 0) {
		zend_throw_exception_ex(zend_ce_exception, 0, "curl socket " ZEND_LONG_FMT " cannot be duplicated: %s", s, strerror(errno));
		RETURN_THROWS();
	}

	stream = php_stream_fopen_from_fd(fd, "r+", NULL);
	if (!stream) {
		close(fd);
		zend_throw_exception_ex(zend_ce_exception, 0, "no stream on curl socket " ZEND_LONG_FMT, s);
		RETURN_THROWS();
	}

	php_stream_to_zval(stream, return_value);
}

PHP_MINIT_FUNCTION(multicurl)
{
	register_class_Multicurl_Multi();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(multicurl)
{
#if defined(ZTS) && defined(COMPILE_DL_MULTICURL)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(multicurl)
{
	HashTable *registry = multicurl_registry;
	multicurl_multi_state *st;

	if (!registry) {
		return SUCCESS;
	}

	/* Multis still alive are freed after RSHUTDOWN; their free_obj then sees no registry. */
	multicurl_registry = NULL;
	ZEND_HASH_FOREACH_PTR(registry, st) {
		multicurl_multi_unhook(multicurl_obj_multi(st->multi));
	} ZEND_HASH_FOREACH_END();

	zend_hash_destroy(registry);
	FREE_HASHTABLE(registry);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(multicurl)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "multicurl support", "enabled");
	php_info_print_table_row(2, "Version", PHP_MULTICURL_VERSION);
	php_info_print_table_end();
}

static const zend_module_dep multicurl_deps[] = {
	ZEND_MOD_REQUIRED("curl")
	ZEND_MOD_END
};

zend_module_entry multicurl_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	multicurl_deps,
	"multicurl",
	NULL,
	PHP_MINIT(multicurl),
	NULL,
	PHP_RINIT(multicurl),
	PHP_RSHUTDOWN(multicurl),
	PHP_MINFO(multicurl),
	PHP_MULTICURL_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MULTICURL
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(multicurl)
#endif
