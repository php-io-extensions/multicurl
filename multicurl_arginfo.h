/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: c814c04ebb3dadf999972bbd198956206781fa35 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiSetopt, 0, 3, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, option, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiSocketAction, 0, 3, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, s, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, evBitmask, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiFdset, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Multicurl_Multi_curlMultiTimeout arginfo_class_Multicurl_Multi_curlMultiFdset

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiPoll, 0, 3, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, extraFds, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, timeoutMs, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Multicurl_Multi_curlMultiWait arginfo_class_Multicurl_Multi_curlMultiPoll

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiWakeup, 0, 1, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiAssign, 0, 3, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, multi, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, s, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, sockp, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Multicurl_Multi_curlMultiStrerror, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, code, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Multicurl_Multi_curlSocketStream, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, s, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(Multicurl_Multi, curlMultiSetopt);
ZEND_METHOD(Multicurl_Multi, curlMultiSocketAction);
ZEND_METHOD(Multicurl_Multi, curlMultiFdset);
ZEND_METHOD(Multicurl_Multi, curlMultiTimeout);
ZEND_METHOD(Multicurl_Multi, curlMultiPoll);
ZEND_METHOD(Multicurl_Multi, curlMultiWait);
ZEND_METHOD(Multicurl_Multi, curlMultiWakeup);
ZEND_METHOD(Multicurl_Multi, curlMultiAssign);
ZEND_METHOD(Multicurl_Multi, curlMultiStrerror);
ZEND_METHOD(Multicurl_Multi, curlSocketStream);

static const zend_function_entry class_Multicurl_Multi_methods[] = {
	ZEND_ME(Multicurl_Multi, curlMultiSetopt, arginfo_class_Multicurl_Multi_curlMultiSetopt, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiSocketAction, arginfo_class_Multicurl_Multi_curlMultiSocketAction, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiFdset, arginfo_class_Multicurl_Multi_curlMultiFdset, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiTimeout, arginfo_class_Multicurl_Multi_curlMultiTimeout, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiPoll, arginfo_class_Multicurl_Multi_curlMultiPoll, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiWait, arginfo_class_Multicurl_Multi_curlMultiWait, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiWakeup, arginfo_class_Multicurl_Multi_curlMultiWakeup, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiAssign, arginfo_class_Multicurl_Multi_curlMultiAssign, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlMultiStrerror, arginfo_class_Multicurl_Multi_curlMultiStrerror, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Multicurl_Multi, curlSocketStream, arginfo_class_Multicurl_Multi_curlSocketStream, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_Multicurl_Multi(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "Multicurl", "Multi", class_Multicurl_Multi_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, 0);

	return class_entry;
}
