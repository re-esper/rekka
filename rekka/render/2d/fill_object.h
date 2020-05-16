#pragma once

#include "rekka.h"
#include "script_core.h"

NS_REK_BEGIN

typedef enum {
	kPatternNoRepeat = 0,
	kPatternRepeatX = 1,
	kPatternRepeatY = 2,
	kPatternRepeat = 1 | 2
} PatternRepeat;
static const char *_patternRepeat_enum_names[] = {
	"no-repeat",
	"repeat-x",
	"repeat-y",
	"repeat",	
	nullptr
};

class FillObject : public ObjectWrap {
public:	
	static bool is_js_instance(JS::HandleObject obj);
	virtual JSObject* createObject(JSContext *ctx) = 0;
};

class Pattern : public FillObject {
public:	
	Pattern(gpu::Image* texture, PatternRepeat repeat);
	~Pattern();
	JSObject* createObject(JSContext *ctx);	
public:
	gpu::Image* _texture; // should be an alias image
	bool _powerof2;
private:
	PatternRepeat _repeat;
};

struct GradientColorStop {
	GPU_Color color;
	float stop;
};
class LinearGradient : public FillObject
{
public:	
	static GPU_Color SDLCALL colorFunc(float x, float y, void* userdata);
public:
	LinearGradient(float x0, float y0, float x1, float y1);
	~LinearGradient();
	JSObject* createObject(JSContext *ctx);	
	JS_FUNC_DECL(addColorStop)
private:
	float _x0, _y0, _x1, _y1;
	std::vector<GradientColorStop> _colorStops;
};

class RadialGradient : public FillObject
{
public:
	static GPU_Color SDLCALL colorFunc(float x, float y, void* userdata);
public:
	RadialGradient(float x0, float y0, float r0, float x1, float y1, float r1);
	~RadialGradient();
	JSObject* createObject(JSContext *ctx);	
	JS_FUNC_DECL(addColorStop)
private:
	float _x0, _y0, _r0, _x1, _y1, _r1;
	std::vector<GradientColorStop> _colorStops;
};


NS_REK_END