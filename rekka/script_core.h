#pragma once

#include "rekka.h"
#include "spider_object_wrap.h"

#define JS_FUNC_DECL(name) static bool js_##name(JSContext *ctx, unsigned argc, JS::Value *vp);
#define JS_FUNC_IMPL(klass, name) bool klass::js_##name(JSContext *ctx, unsigned argc, JS::Value *vp)

#define JS_PROPGET_DECL(name) static bool js_get_##name(JSContext *ctx, unsigned argc, JS::Value *vp);
#define JS_PROPSET_DECL(name) static bool js_set_##name(JSContext *ctx, unsigned argc, JS::Value *vp);
#define JS_PROPGET_IMPL(klass, name) bool klass::js_get_##name(JSContext *ctx, unsigned argc, JS::Value *vp)
#define JS_PROPSET_IMPL(klass, name) bool klass::js_set_##name(JSContext *ctx, unsigned argc, JS::Value *vp)

#define JS_FUNC_DEF(klass, func) JS_FN(#func, klass::js_##func, 0, JSPROP_PERMANENT | JSPROP_ENUMERATE)
#define JS_PROP_DEF(klass, prop) JS_PSGS(#prop, klass::js_get_##prop, klass::js_set_##prop, JSPROP_PERMANENT | JSPROP_ENUMERATE)
#define JS_PROP_GET_DEF(klass, prop) JS_PSG(#prop, klass::js_get_##prop, JSPROP_PERMANENT | JSPROP_ENUMERATE)

#define JS_BEGIN_ARG JS::CallArgs args = JS::CallArgsFromVp(argc, vp)
#define JS_BEGIN_ARG_THIS(klass) JS::CallArgs args = JS::CallArgsFromVp(argc, vp); \
	JS::RootedObject js_this(ctx, args.thisv().toObjectOrNull()); \
	klass* pthis = (klass*)JS_GetPrivate(js_this)

#define JS_REF_ARG(name, index) const auto& name = args.get(index)
#define JS_ROOT_OBJECT_ARG(name, index) JS::RootedObject name(ctx, JS::ToObject(ctx, args.get(index)));

#define JS_INT_ARG(name, index) int name = 0; \
	JS::ToInt32(ctx, args.get(index), &name)
#define JS_DOUBLE_ARG(name, index) double name = 0; \
	JS::ToNumber(ctx, args.get(index), &name)
#define JS_BOOL_ARG(name, index) bool name = JS::ToBoolean(args.get(index))

#define JS_STRING_ARG(name, index) JS::RootedString js_##name(ctx, JS::ToString(ctx, args.get(index))); \
	JSAutoByteString _jsautostr_##index; \
	char* name = _jsautostr_##index.encodeUtf8(ctx, js_##name);

#define JS_RET(val) { _SetRetval(ctx, args.rval(), val); return true; }
#define JS_RETURN { args.rval().setUndefined(); return true; }
#define JS_FAIL(...) { JS_ReportError(ctx, ##__VA_ARGS__); return false; }
#define JS_FAIL_EXCEPTOPN { JS_ReportPendingException(ctx); return false; }           

inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, double val) { rval.setDouble(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, int val) { rval.setInt32(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, uint32_t val) { rval.setNumber(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, bool val) { rval.setBoolean(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, JSObject* val) { rval.setObjectOrNull(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, JS::Value val) { rval.set(val); }
inline void _SetRetval(JSContext* ctx, JS::MutableHandleValue rval, const char* val) {
	size_t sizeu16 = 0;
	char16_t* u16str = toUtf16(val, &sizeu16);
	rval.setString(JS_NewUCStringCopyN(ctx, u16str, sizeu16));
	if (u16str) free(u16str);
}

NS_REK_BEGIN

class ScriptCore {
private:
	ScriptCore();
	static ScriptCore* s_sharedScriptCore;
public:
	~ScriptCore();
	static ScriptCore *getInstance();
	static void destroyInstance();
public:
	bool initEngine(uint32_t maxBytes = 8L * 1024 * 1024, size_t stackSize = 8192);
	bool runScript(const char* fileName);
	JSContext* getGlobalContext() { return _ctx; }
	JS::PersistentRootedObject& getGlobalObject() { return *_global; }
	JS::PersistentRootedObject& getDebugGlobalObject() { return *_debugGlobal; }
	static void printJSStack(JSContext *ctx);
	void forceGC();
private:
	static void reportError(JSContext *ctx, const char *message, JSErrorReport *report);	
private:
	JSRuntime* _rt;
	JSContext* _ctx;
	JS::PersistentRootedObject*	_global;
	JS::PersistentRootedObject*	_debugGlobal;
};

NS_REK_END