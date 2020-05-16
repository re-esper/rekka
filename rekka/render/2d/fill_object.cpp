#include "fill_object.h"
#include "script_core.h"
#include "render/extra.h"

NS_REK_BEGIN

static JSClass pattern_class = {
	"CanvasPattern", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
static JSClass linear_gradient_class = {
	"CanvasLinearGradient", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
static JSClass radial_gradient_class = {
	"CanvasRadialGradient", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};

bool FillObject::is_js_instance(JS::HandleObject obj)
{
	const JSClass* klass = JS_GetClass(obj);
	return klass == &pattern_class || klass == &linear_gradient_class || klass == &radial_gradient_class;
}

Pattern::Pattern(gpu::Image * texture, PatternRepeat repeat)
{
	_texture = gpu::CreateAliasImage(texture);
	_repeat = repeat;
	_powerof2 = isPowerOfTwo(texture->texture_w) && isPowerOfTwo(texture->texture_h);	
	if (CanvasExtra::supportNPOTRepeat || _powerof2) {
		switch (repeat) {
		case kPatternNoRepeat:
			gpu::SetWrapMode(_texture, gpu::WRAP_NONE, gpu::WRAP_NONE);
			break;
		case kPatternRepeatX:
			gpu::SetWrapMode(_texture, gpu::WRAP_REPEAT, gpu::WRAP_NONE);
			break;
		case kPatternRepeatY:
			gpu::SetWrapMode(_texture, gpu::WRAP_NONE, gpu::WRAP_REPEAT);
			break;
		case kPatternRepeat:
			gpu::SetWrapMode(_texture, gpu::WRAP_REPEAT, gpu::WRAP_REPEAT);
			break;
		}
	}
}
Pattern::~Pattern()
{
	if (_texture) gpu::FreeImage(_texture);	
}

JSObject* Pattern::createObject(JSContext* ctx)
{	
	JS::RootedObject robj(ctx, JS_NewObject(ctx, &pattern_class));
	wrap(ctx, robj);
	return robj;
}

LinearGradient::LinearGradient(float x0, float y0, float x1, float y1)
: _x0(x0), _y0(y0), _x1(x1), _y1(y1)
{	
}

LinearGradient::~LinearGradient()
{	
}

JSObject* LinearGradient::createObject(JSContext* ctx)
{	
	JS::RootedObject robj(ctx, JS_NewObject(ctx, &linear_gradient_class));
	JS_DefineFunction(ctx, robj, "addColorStop", LinearGradient::js_addColorStop, 0, 0);
	wrap(ctx, robj);
	return robj;
}

JS_FUNC_IMPL(LinearGradient, addColorStop)
{
	JS_BEGIN_ARG_THIS(LinearGradient);
	JS_DOUBLE_ARG(stop, 0);
	JS_STRING_ARG(colorstr, 1);
	stop = std::min<double>(std::max<double>(stop, 0), 1);	
	auto itr = pthis->_colorStops.begin();
	for ( ; itr != pthis->_colorStops.end(); ++itr) {
		if (itr->stop > stop) break;
	}
	const SDL_Color& sc = HtmlColorToColor(colorstr);
	GPU_Color color = { sc.r / 255.0f, sc.g / 255.0f, sc.b / 255.0f, sc.a / 255.0f };
	pthis->_colorStops.insert(itr, { color, (float)stop });
	JS_RETURN;
}

inline GPU_Color _calculateLinearColor(float p, float p0, float p1, std::vector<GradientColorStop>& colorStops)
{
	float diff = p1 - p0;
	GradientColorStop *curclrstop, *prevclrstop;
	for (int i = 0; i < colorStops.size(); ++i) {
		curclrstop = &colorStops[i];
		float stop = curclrstop->stop * diff + p0;
		if (p <= stop) {
			if (i == 0) return curclrstop->color;
			float prev_p = prevclrstop->stop * diff + p0;
			float factor = (p - prev_p) / (stop - prev_p);
			auto& clr = curclrstop->color;
			auto& prevclr = prevclrstop->color;
			return { factor * (clr.r - prevclr.r) + prevclr.r, factor * (clr.g - prevclr.g) + prevclr.g,
				factor * (clr.b - prevclr.b) + prevclr.b,factor * (clr.a - prevclr.a) + prevclr.a };
		}
		prevclrstop = curclrstop;
	}
	return curclrstop->color;
}
GPU_Color SDLCALL LinearGradient::colorFunc(float x, float y, void * userdata)
{
	LinearGradient* pthis = (LinearGradient*)userdata;
	if (pthis->_colorStops.size() == 0) return{ 1, 1, 1, 1 };
	if (pthis->_colorStops.size() == 1) return pthis->_colorStops[0].color;
	if (pthis->_x0 == pthis->_x1 && pthis->_y0 == pthis->_y1) return pthis->_colorStops[0].color;
	if (pthis->_x0 == pthis->_x1) return _calculateLinearColor(y, pthis->_y0, pthis->_y1, pthis->_colorStops);
	if (pthis->_y0 == pthis->_y1) return _calculateLinearColor(x, pthis->_x0, pthis->_x1, pthis->_colorStops);
	GPU_Color color_x = _calculateLinearColor(x, pthis->_x0, pthis->_x1, pthis->_colorStops);
	GPU_Color color_y = _calculateLinearColor(y, pthis->_y0, pthis->_y1, pthis->_colorStops);
	return{ sqrt((color_x.r * color_x.r + color_y.r * color_y.r) / 2),
		sqrt((color_x.g * color_x.g + color_y.g * color_y.g) / 2),
		sqrt((color_x.b * color_x.b + color_y.b * color_y.b) / 2),
		sqrt((color_x.a * color_x.a + color_y.a * color_y.a) / 2) };
}

RadialGradient::RadialGradient(float x0, float y0, float r0, float x1, float y1, float r1)
: _x0(x0), _y0(y0), _r0(r0), _x1(x1), _y1(y1), _r1(r1)
{
}

RadialGradient::~RadialGradient()
{
}

JSObject* RadialGradient::createObject(JSContext* ctx)
{	
	JS::RootedObject robj(ctx, JS_NewObject(ctx, &radial_gradient_class));
	JS_DefineFunction(ctx, robj, "addColorStop", RadialGradient::js_addColorStop, 0, 0);
	wrap(ctx, robj);
	return robj;
}

JS_FUNC_IMPL(RadialGradient, addColorStop)
{
	JS_BEGIN_ARG_THIS(RadialGradient);
	JS_DOUBLE_ARG(stop, 0);
	JS_STRING_ARG(colorstr, 1);
	stop = std::min<double>(std::max<double>(stop, 0), 1);
	auto itr = pthis->_colorStops.begin();
	for (; itr != pthis->_colorStops.end(); ++itr) {
		if (itr->stop > stop) break;
	}
	const SDL_Color& sc = HtmlColorToColor(colorstr);
	GPU_Color color = { sc.r / 255.0f, sc.g / 255.0f, sc.b / 255.0f, sc.a / 255.0f };
	pthis->_colorStops.insert(itr, { color, (float)stop });
	JS_RETURN;
}

inline float _distance(float x1, float y1, float x2, float y2)
{
	float dx = x2 - x1, dy = y2 - y1;
	return sqrt(dx * dx + dy * dy);
}

GPU_Color SDLCALL RadialGradient::colorFunc(float x, float y, void * userdata)
{
	RadialGradient* pthis = (RadialGradient*)userdata;
	if (pthis->_colorStops.size() == 0) return{ 1, 1, 1, 1 };
	if (pthis->_colorStops.size() == 1) return pthis->_colorStops[0].color;	
	float dist_0 = _distance(x, y, pthis->_x0, pthis->_y0);
	float dist_1 = _distance(x, y, pthis->_x1, pthis->_y1);
	float dist = _distance(pthis->_x0, pthis->_y0, pthis->_x1, pthis->_y1);
	auto& colorStops = pthis->_colorStops;
	if (dist_0 < dist_1 && dist_1 - dist_0 > dist) return colorStops.front().color;
	if (dist_1 < dist_0 && dist_0 - dist_1 > dist) return colorStops.back().color;
	float ratio = dist_0 / (dist_0 + dist_1);
	GradientColorStop *curclrstop, *prevclrstop;
	for (int i = 0; i < colorStops.size(); ++i) {
		curclrstop = &colorStops[i];		
		if (ratio <= curclrstop->stop) {
			if (i == 0) return curclrstop->color;			
			float factor = (ratio - prevclrstop->stop) / (curclrstop->stop - prevclrstop->stop);
			auto& clr = curclrstop->color;
			auto& prevclr = prevclrstop->color;
			return { factor * (clr.r - prevclr.r) + prevclr.r, factor * (clr.g - prevclr.g) + prevclr.g,
				factor * (clr.b - prevclr.b) + prevclr.b,factor * (clr.a - prevclr.a) + prevclr.a };
		}
		prevclrstop = curclrstop;
	}
	return curclrstop->color;
}

NS_REK_END