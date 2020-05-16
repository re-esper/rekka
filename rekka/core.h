#pragma once

#include "rekka.h"
#include "system/local_storage.h"

NS_REK_BEGIN

class Core {
private:
	Core();
	static Core* s_sharedRekkaCore;
public:
	~Core();
	static Core *getInstance();
	static void destroyInstance();
	bool init();
	gpu::Target* _screen;
	SDL_Window* _window;
	std::string _prefPath;
public:
	void run();
	void pause();
	void resume();
public:
	static bool js_forceGC(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_include(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_print(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_requestAnimationFrame(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_setTimeout(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_setInterval(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_clearTimer(JSContext* ctx, unsigned argc, JS::Value* vp);
	// Rekka object
	static bool js_get_innerWidth(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_get_innerHeight(JSContext* ctx, unsigned argc, JS::Value* vp);	
	static bool js_get_platform(JSContext* ctx, unsigned argc, JS::Value* vp);

	static bool js_performanceNow(JSContext* ctx, unsigned argc, JS::Value* vp);
	static bool js_loadFont(JSContext* ctx, unsigned argc, JS::Value* vp);
private:
	bool initManifest();
	void setAppName(const char* appName);
	void jsb_register();
	void initRekkaEnvironment();	

	JS::PersistentRootedValue* _requestAnimationFrameFunc;
	JS::PersistentRootedValue* _mouseMoveCallback;
	JS::PersistentRootedValue* _mouseClickCallback;
	JS::PersistentRootedValue* _keyboardCallback;
	JS::PersistentRootedValue* _touchStartCallback;
	JS::PersistentRootedValue* _touchEndCallback;
	JS::PersistentRootedValue* _touchMoveCallback;
	bool fireRequestAnimationFrame(float deltaTime);
	bool fireMouseMove(int x, int y);
	bool fireMouseClick(int button, int x, int y, bool isdown);
	bool fireKeyboard(int key, int mod, bool isdown);
	bool fireTouchEvent(SDL_TouchID id, int x, int y, int eventType);
private:
	bool _paused;
	int _taskGarbageCollection;
	LocalStorage _localStorage;
	// 对移动设备innerWidth/innerHeight为设备尺寸
	// 对桌面系统窗口化时为窗口尺寸
	int _innerWidth;
	int _innerHeight;
	std::string _appName;	
	std::string _startUrl;
private:
	inline void traslatePoint(float& x, float& y);
	inline void traslateTouchPoint(float& x, float& y);
	void setFullscreen(bool enable);
	void setFullscreenVirtualResolution();
	bool _isFullscreen;	
	int _screenWidth;
	int _screenHeight;
	float _reciprocalScale;
	GPU_Rect _viewport;
	bool _isLandscape;
};

NS_REK_END
