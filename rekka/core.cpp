#include "core.h"
#include "scheduler.h"
#include "render/font_manager.h"
#include "render/image.h"
#include "render/canvas.h"
#include "render/2d/context_2d.h"
#include "render/extra.h"
#include "system/xml_http_request.h"
#include "audio/audio_manager.h"
#include "audio/audio.h"
#include "rapidjson/document.h"
#include "stb_image.h"

NS_REK_BEGIN

Core* Core::s_sharedRekkaCore = nullptr;
Core* Core::getInstance()
{
	if (!s_sharedRekkaCore) s_sharedRekkaCore = new (std::nothrow) Core();
	return s_sharedRekkaCore;
}

void Core::destroyInstance()
{
	SAFE_DELETE(s_sharedRekkaCore);	
}

Core::Core()
: _paused(false), _taskGarbageCollection(0), _requestAnimationFrameFunc(nullptr),
_isFullscreen(false), _isLandscape(true), _appName("Game"), _prefPath("")
{	
}
Core::~Core()
{
	SAFE_DELETE(_requestAnimationFrameFunc);
	SAFE_DELETE(_mouseMoveCallback);
	SAFE_DELETE(_mouseClickCallback);
	SAFE_DELETE(_keyboardCallback);
	SAFE_DELETE(_touchStartCallback);
	SAFE_DELETE(_touchEndCallback);
	SAFE_DELETE(_touchMoveCallback);
	gpu::Quit();
}

bool Core::init()
{
	initManifest();
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);	

	SDL_Rect r;
	SDL_GetDisplayBounds(0, &r);
	_screenWidth = r.w;
	_screenHeight = r.h;

	_innerWidth = 816;
	_innerHeight = 624;

#ifdef SINGLE_BUFFERED
	gpu::SetPreInitFlags(gpu::INIT_DISABLE_DOUBLE_BUFFER);
#endif
	if (_isFullscreen) {
		_screen = gpu::Init(_innerWidth, _innerHeight, SDL_WINDOW_FULLSCREEN_DESKTOP);
		setFullscreenVirtualResolution();
	}
	else {
		_screen = gpu::Init(_innerWidth, _innerHeight, GPU_DEFAULT_INIT_FLAGS);
	}

	if (!_screen) {
		SDL_LogError(0, "Unable to create screen: %s", SDL_GetError());
		return false;
	}
	_window = SDL_GetWindowFromID(gpu::GetInitWindow());	

	SDL_SetWindowTitle(_window, "Rekka");
	gpu::SetDefaultAnchor(0, 0);

#ifdef _DEBUG
	gpu::SetDebugLevel(gpu::DEBUG_LEVEL_MAX);
#endif
#if !defined(SINGLE_BUFFERED)
	SDL_GL_SetSwapInterval(0);
#endif

	AudioManager::getInstance()->initialze();
	CanvasExtra::initialize();

	jsb_register();

	return true;
}

void Core::run()
{
	auto scheduler = Scheduler::getInstance();
	auto fontmgr = FontManager::getInstance();
	auto scripter = ScriptCore::getInstance();

	initRekkaEnvironment();

	bool done = false;
	double prev_t = scheduler->performanceNow();
	double fps_t = 0;
	int frameCount = 0;
	
	while (!done) {
		double now = scheduler->performanceNow();
		double deltaTime = (now - prev_t) * 0.001f;
		prev_t = now;

		scheduler->update(deltaTime);
		fontmgr->update(deltaTime);

		SDL_Event event;
		float x, y;
		int keycode = 0;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {			
			case SDL_MOUSEBUTTONDOWN:				
			case SDL_MOUSEBUTTONUP:
				x = event.button.x;
				y = event.button.y;
				traslatePoint(x, y);
				fireMouseClick(event.button.button, x, y, event.type == SDL_MOUSEBUTTONDOWN);
				break;
			case SDL_MOUSEMOTION:
				x = event.button.x;
				y = event.button.y;
				traslatePoint(x, y);
				fireMouseMove(x, y);
				break;
			case SDL_KEYDOWN:				
				if (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)) {
					setFullscreen(!_isFullscreen);
				}
				keycode = SDLKeyToHtmlKey(event.key.keysym.sym);
				if (keycode) fireKeyboard(keycode, event.key.keysym.mod, true);
				break;
			case SDL_KEYUP:
				keycode = SDLKeyToHtmlKey(event.key.keysym.sym);
				if (keycode) fireKeyboard(keycode, event.key.keysym.mod, false);
				break;
			case SDL_FINGERDOWN:
				x = event.tfinger.x;
				y = event.tfinger.y;
				traslateTouchPoint(x, y);				
				fireTouchEvent(event.tfinger.touchId, x, y, 0);
				break;
			case SDL_FINGERUP:
				x = event.tfinger.x;
				y = event.tfinger.y;
				traslateTouchPoint(x, y);
				fireTouchEvent(event.tfinger.touchId, x, y, 1);
				break;
			case SDL_FINGERMOTION:
				x = event.tfinger.x;
				y = event.tfinger.y;
				traslateTouchPoint(x, y);
				fireTouchEvent(event.tfinger.touchId, x, y, 2);
				break;
			case SDL_QUIT:
				done = true;
				break;
			}
		}

		if (_taskGarbageCollection > 0) {
			_taskGarbageCollection--;
			scripter->forceGC();
		}
		bool skip = fireRequestAnimationFrame(deltaTime);		
		if (!skip) gpu::Flip(_screen);

		fps_t += deltaTime;
		frameCount++;
		if (fps_t > 1.0) {
#ifdef REKKA_DESKTOP
			static char title[0x20];
			sprintf(title, "Rekka %.2f FPS", (float)frameCount / fps_t);
			SDL_SetWindowTitle(_window, title);
#else
			SDL_Log("Rekka %.2f FPS", (float)frameCount / fps_t);
#endif
			fps_t = 0;
			frameCount = 0;
		}
	}
}

void Core::pause()
{
}

void Core::resume()
{
}

bool Core::initManifest()
{
	size_t dataLength = 0;
	char* data = fetchFileContent("manifest.json", &dataLength);
	if (data == nullptr) return false;

	rapidjson::Document d;
	d.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(data);
	free(data);
	if (d.HasParseError()) {		
		SDL_LogError(0, "Error in parsing manifest.json, error %d", d.GetParseError());
		return false;
	}

	setAppName(d.HasMember("name") && d["name"].IsString() ? d["name"].GetString() : "Game");	
	_startUrl = d.HasMember("start_url") && d["start_url"].IsString() ? d["start_url"].GetString() : "index.html";
	if (d.HasMember("display") && d["display"].IsString()) {
		_isFullscreen = strcasecmp(d["display"].GetString(), "fullscreen") == 0;
	}	
	if (d.HasMember("orientation") && d["orientation"].IsString()) {
		_isLandscape = strcasecmp(d["orientation"].GetString(), "portrait") != 0;
	}
	SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
#ifndef REKKA_DESKTOP
	_isFullscreen = true; // always fullscreen at Mobile
	SDL_SetHint(SDL_HINT_ORIENTATIONS, _isLandscape ? "LandscapeLeft LandscapeRight" : "Portrait PortraitUpsideDown");	
#endif
	return true;
}

void Core::setAppName(const char* appName)
{
	_appName = appName;
	char* prefpath = SDL_GetPrefPath("Rekka", appName);
	if (!prefpath) {
		prefpath = SDL_GetPrefPath("Rekka", "Game");
		if (!prefpath) {
			SDL_LogError(0, "Failed to initialize pref path");
			_prefPath = "";
			return;
		}
	}	
	_prefPath = prefpath;
	SDL_free(prefpath);	
}

void Core::jsb_register()
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	
	_requestAnimationFrameFunc = new JS::PersistentRootedValue(ctx, JS::NullValue());

	JS_DefineFunction(ctx, global, "forceGC", (JSNative)js_forceGC, 0, 0);
	JS_DefineFunction(ctx, global, "print", (JSNative)js_print, 0, 0);
	JS_DefineFunction(ctx, global, "requestAnimationFrame", (JSNative)js_requestAnimationFrame, 0, 0);
	JS_DefineFunction(ctx, global, "setTimeout", (JSNative)js_setTimeout, 0, 0);
	JS_DefineFunction(ctx, global, "setInterval", (JSNative)js_setInterval, 0, 0);
	JS_DefineFunction(ctx, global, "clearTimeout", (JSNative)js_clearTimer, 0, 0);
	JS_DefineFunction(ctx, global, "clearInterval", (JSNative)js_clearTimer, 0, 0);

	static JSPropertySpec rekkacore_properties[] = {
		JS_PROP_GET_DEF(Core, innerWidth),
		JS_PROP_GET_DEF(Core, innerHeight),
		JS_PROP_GET_DEF(Core, platform),
		JS_PS_END
	};
	static JSFunctionSpec rekkacore_funcs[] = {
		JS_FUNC_DEF(Core, include),
		JS_FUNC_DEF(Core, performanceNow),
		JS_FUNC_DEF(Core, loadFont),
		JS_FS_END
	};	
	JS::RootedObject rekkaobj(ctx, JS_NewObject(ctx, nullptr));	
	JS_DefineProperty(ctx, global, "Rekka", rekkaobj, JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE);
	JS_DefineFunctions(ctx, rekkaobj, rekkacore_funcs);
	JS_DefineProperties(ctx, rekkaobj, rekkacore_properties);
	
	Image::jsb_register(ctx, rekkaobj);
	Canvas::jsb_register(ctx, rekkaobj);
	XMLHttpRequest::jsb_register(ctx, rekkaobj);
	Audio::jsb_register(ctx, rekkaobj);

	_localStorage.jsb_register(ctx, global);
}

void Core::initRekkaEnvironment()
{	
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	_mouseMoveCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_mouseMoveCallback", _mouseMoveCallback);
	_mouseClickCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_mouseClickCallback", _mouseClickCallback);
	_keyboardCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_keyboardCallback", _keyboardCallback);
	_touchStartCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_touchStartCallback", _touchStartCallback);
	_touchEndCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_touchEndCallback", _touchEndCallback);
	_touchMoveCallback = new JS::PersistentRootedValue(ctx, JS::NullValue());
	JS_GetProperty(ctx, global, "_touchMoveCallback", _touchMoveCallback);

	// perform "window.onload"
	JS::RootedValue onloadfunc(ctx);
	JS_GetProperty(ctx, global, "onload", &onloadfunc);
	if (onloadfunc.isObject()) { // is a Function object?						
		JS::RootedValue rval(ctx);
		JS_CallFunctionValue(ctx, global, onloadfunc, JS::HandleValueArray::empty(), &rval);
	}
}

inline void Core::traslatePoint(float & x, float & y)
{
	if (_isFullscreen) {
		x = (x - _viewport.x) * _reciprocalScale;
		y = (y - _viewport.y) * _reciprocalScale;
	}
}

inline void Core::traslateTouchPoint(float & x, float & y)
{
	x = (x * (_viewport.w + _viewport.x * 2) - _viewport.x) * _reciprocalScale;
	y = (y * (_viewport.h + _viewport.y * 2) - _viewport.y) * _reciprocalScale;
}

void Core::setFullscreen(bool enable)
{
	if (_isFullscreen == enable) return;
	_isFullscreen = enable;
	gpu::SetFullscreen(enable, true);
	if (enable) {
		setFullscreenVirtualResolution();
	}
	else {
		gpu::UnsetVirtualResolution(_screen);
		gpu::UnsetViewport(_screen);
	}
}

void Core::setFullscreenVirtualResolution()
{
	gpu::SetVirtualResolution(_screen, _innerWidth, _innerHeight);
	float screenRatio = (float)_screenWidth / _screenHeight;
	float aspectRatio = (float)_innerWidth / _innerHeight;
	float scale = 1.0f;
	float x = 0, y = 0;
	if (screenRatio > aspectRatio) {
		scale = (float)_screenHeight / (float)_innerHeight;
		x = (_screenWidth - _innerWidth * scale) / 2.f;
	}
	else if (screenRatio < aspectRatio) {
		scale = (float)_screenWidth / (float)_innerWidth;
		y = (_screenHeight - _innerHeight * scale) / 2.f;
	}
	else {
		scale = (float)_screenWidth / (float)_innerWidth;
	}
	_reciprocalScale = 1 / scale;
	_viewport = { x, y, _innerWidth * scale, _innerHeight * scale };
	gpu::SetViewport(_screen, _viewport);
}

bool Core::js_forceGC(JSContext* ctx, unsigned argc, JS::Value * vp)
{	
	s_sharedRekkaCore->_taskGarbageCollection++;
	return true;
}

bool Core::js_include(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_STRING_ARG(filename, 0);
	ScriptCore::getInstance()->runScript(filename);
	JS_RETURN;
}

bool Core::js_print(JSContext* ctx, unsigned argc, JS::Value* vp)
{	
	JS_BEGIN_ARG;
	std::string message;
	for (unsigned i = 0; i < args.length(); i++) {		
		JS::Rooted<JSString*> str(ctx, JS::ToString(ctx, args[i]));
		char* bytes = JS_EncodeStringToUTF8(ctx, str);
		if (i > 0) message += " ";
		message += bytes;
		JS_free(ctx, bytes);
	}	
	SDL_Log("%s", message.c_str());
	JS_RETURN;
}

bool Core::js_requestAnimationFrame(JSContext* ctx, unsigned argc, JS::Value* vp)
{
	JS_BEGIN_ARG;
	*(s_sharedRekkaCore->_requestAnimationFrameFunc) = args[0];
	JS_RETURN;
}

bool Core::fireRequestAnimationFrame(float deltaTime)
{
	if (_requestAnimationFrameFunc->isNullOrUndefined()) return false;
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	JS::RootedValue funcval(ctx, *_requestAnimationFrameFunc);
	JS::RootedValue rval(ctx);
	JS::AutoValueArray<1> argv(ctx);	
	argv[0].setDouble(deltaTime);
	_requestAnimationFrameFunc->setNull();
	JS_CallFunctionValue(ctx, global, funcval, argv, &rval);
	return rval.toBoolean();
}

bool Core::fireMouseMove(int x, int y)
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();	
	JS::RootedValue rval(ctx);
	JS::AutoValueArray<2> argv(ctx);
	argv[0].setInt32(x);
	argv[1].setInt32(y);	
	return JS_CallFunctionValue(ctx, global, *_mouseMoveCallback, argv, &rval);		
}

bool Core::fireMouseClick(int button, int x, int y, bool isdown)
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	JS::RootedValue rval(ctx);
	JS::AutoValueArray<4> argv(ctx);
	argv[0].setInt32(button - 1);
	argv[1].setInt32(x);
	argv[2].setInt32(y);
	argv[3].setBoolean(isdown);
	return JS_CallFunctionValue(ctx, global, *_mouseClickCallback, argv, &rval);	
}

bool Core::fireKeyboard(int key, int mod, bool isdown)
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	JS::RootedValue rval(ctx);
	JS::AutoValueArray<3> argv(ctx);
	argv[0].setInt32(key);
	argv[1].setInt32(mod);	
	argv[2].setBoolean(isdown);
	return JS_CallFunctionValue(ctx, global, *_keyboardCallback, argv, &rval);
}

bool Core::fireTouchEvent(SDL_TouchID id, int x, int y, int eventType)
{
	auto ctx = ScriptCore::getInstance()->getGlobalContext();
	auto global = ScriptCore::getInstance()->getGlobalObject();
	JS::RootedValue rval(ctx);
	JS::AutoValueArray<3> argv(ctx);
	argv[0].setInt32(id);
	argv[1].setInt32(x);
	argv[2].setInt32(y);
	if (eventType == 0)
		return JS_CallFunctionValue(ctx, global, *_touchStartCallback, argv, &rval);
	else if (eventType == 1)
		return JS_CallFunctionValue(ctx, global, *_touchEndCallback, argv, &rval);
	else
		return JS_CallFunctionValue(ctx, global, *_touchMoveCallback, argv, &rval);
}

bool Core::js_setTimeout(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_REF_ARG(callback, 0);	
	JS_INT_ARG(delay, 1);
	int timerId = Scheduler::getInstance()->scheduleCallback(callback, delay * 0.001, false);
	JS_RET(timerId);
}

bool Core::js_setInterval(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_REF_ARG(callback, 0);
	JS_INT_ARG(interval, 1);
	int timerId = Scheduler::getInstance()->scheduleCallback(callback, interval * 0.001, true);
	JS_RET(timerId);
}

bool Core::js_clearTimer(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_INT_ARG(timerId, 0);
	Scheduler::getInstance()->cancel(timerId);
	JS_RETURN;
}

bool Core::js_get_innerWidth(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_RET(s_sharedRekkaCore->_innerWidth);	
}

bool Core::js_get_innerHeight(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_RET(s_sharedRekkaCore->_innerHeight);
}

bool Core::js_get_platform(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_RET(SDL_GetPlatform());	
}

bool Core::js_performanceNow(JSContext* ctx, unsigned argc, JS::Value* vp)
{
	JS_BEGIN_ARG;	
	double realTime = Scheduler::getInstance()->performanceNow();
	JS_RET(realTime);	
}

bool Core::js_loadFont(JSContext* ctx, unsigned argc, JS::Value * vp)
{
	JS_BEGIN_ARG;
	JS_STRING_ARG(filename, 0);
	if (argc == 2) {
		JS_STRING_ARG(family, 1);
		FontManager::getInstance()->loadFont(filename, family);
	}
	else {
		FontManager::getInstance()->loadFont(filename);
	}
	JS_RETURN;
}

NS_REK_END