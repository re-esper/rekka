#include "script_core.h"
#include "SDL.h"

NS_REK_BEGIN

ScriptCore* ScriptCore::s_sharedScriptCore = nullptr;
ScriptCore* ScriptCore::getInstance()
{
	if (!s_sharedScriptCore) s_sharedScriptCore = new (std::nothrow) ScriptCore();
	return s_sharedScriptCore;
}

void ScriptCore::destroyInstance()
{
	SAFE_DELETE(s_sharedScriptCore);
}

ScriptCore::ScriptCore()
: _rt(nullptr), _ctx(nullptr), _global(nullptr)
{
}

bool ScriptCore::initEngine(uint32_t maxBytes, size_t stackSize)
{
	JS_Init();
	_rt = JS_NewRuntime(maxBytes);
	_ctx = JS_NewContext(_rt, stackSize);
	JS_SetErrorReporter(_rt, ScriptCore::reportError);

	static JSClass global_class = {
		"global", JSCLASS_GLOBAL_FLAGS,
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
		JS_GlobalObjectTraceHook
	};	
	_global = new JS::PersistentRootedObject(_ctx, JS_NewGlobalObject(_ctx, &global_class, nullptr, JS::FireOnNewGlobalHook));
	JS_EnterCompartment(_ctx, *_global);
	JS_InitStandardClasses(_ctx, *_global);	
	return true;
}

ScriptCore::~ScriptCore()
{
	JS_LeaveCompartment(_ctx, nullptr);
	SAFE_DELETE(_global);	
	JS_DestroyContext(_ctx);
	JS_DestroyRuntime(_rt);
	JS_ShutDown();
}

void ScriptCore::printJSStack(JSContext *ctx)
{
	JS::RootedObject stack(ctx);
	if (JS::CaptureCurrentStack(ctx, &stack, 32)) {
		JS::RootedString stackStr(ctx);
		if (JS::BuildStackString(ctx, stack, &stackStr, 2)) {
			JSAutoByteString jsautostr;
			fputs("Stack:\n", stderr);
			fputs(jsautostr.encodeUtf8(ctx, stackStr), stderr);
		}
	}
}

void ScriptCore::forceGC()
{
	JS_GC(_rt);	
}

void ScriptCore::reportError(JSContext* ctx, const char* message, JSErrorReport* report)
{
	std::string fileName = report->filename ? report->filename : "<no filename>";
	int32_t lineno = report->lineno;
	std::string msg = message != nullptr ? message : "";
	SDL_LogError(0, "%s:%u: %s", fileName.c_str(), report->lineno, msg.c_str());
}

bool ScriptCore::runScript(const char* fileName)
{
	JS::RootedValue rval(_ctx);
	JS::CompileOptions opts(_ctx);
	opts.setUTF8(true);
	opts.setFileAndLine(fileName, 1);
	size_t dataBytes = 0;
	char* data = fetchFileContent(fileName, &dataBytes);
	if (data == nullptr) return false;	
	bool ret = JS::Evaluate(_ctx, opts, data, dataBytes, &rval);
	free(data);
	return ret;
}

NS_REK_END