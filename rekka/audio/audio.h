#pragma once

#include "rekka.h"
#include "SDL_mixer.h"
#include "script_core.h"

/*
首先实现:
- new Audio([URLString], [bool]static);
- play()
- pause()
- canPlayType()
- loop
- duration
- currentTime
- ended event
*/

NS_REK_BEGIN

class Audio : public ObjectWrap {
public:
	Audio(bool isstatic);
	~Audio();
	static void jsb_register(JSContext *ctx, JS::HandleObject parent);
	static bool is_js_instance(JS::HandleObject obj);
private:
	void setSrc(char* src);	
public:	
	static bool constructor(JSContext *ctx, unsigned argc, JS::Value *vp);
	JS_PROPGET_DECL(src)
	JS_PROPSET_DECL(src)
	JS_PROPGET_DECL(loop)
	JS_PROPSET_DECL(loop)
	JS_PROPGET_DECL(currentTime)
	JS_PROPSET_DECL(currentTime)
	JS_PROPGET_DECL(duration)
	JS_PROPGET_DECL(volume)
	JS_PROPSET_DECL(volume)	
	JS_FUNC_DECL(canPlayType)
	JS_FUNC_DECL(play)
	JS_FUNC_DECL(pause)
	// Exensions
	JS_PROPGET_DECL(playing)
	JS_FUNC_DECL(stop)
	JS_FUNC_DECL(fadeIn)
	JS_FUNC_DECL(fadeOut)
public:
	int _channel;
	bool _static;
private:	
	Mix_Chunk* _chunk;
	Mix_Music* _music;	
	std::string _src;
	bool _loop;
	int _volume;
	double _currentTime;
};

NS_REK_END