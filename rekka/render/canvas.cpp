#include "canvas.h"
#include "core.h"
#include "2d/context_2d.h"
#include "image_manager.h"


NS_REK_BEGIN

static JSClass canvas_class = {
	"Canvas", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
static JSPropertySpec canvas_properties[] = {
	JS_PROP_DEF(Canvas, width),
	JS_PROP_DEF(Canvas, height),
	JS_PROP_DEF(Canvas, src),
	JS_PS_END
};
static JSFunctionSpec canvas_funcs[] = {
	JS_FUNC_DEF(Canvas, getContext),
	JS_FS_END
};

void Canvas::jsb_register(JSContext *ctx, JS::HandleObject parent)
{
	JS_InitClass(ctx, parent, nullptr, &canvas_class, Canvas::constructor, 0, canvas_properties, canvas_funcs, 0, 0);
}
bool Canvas::is_js_instance(JS::HandleObject obj)
{
	return JS_GetClass(obj) == &canvas_class;
}

static JSClass canvas_style_class = {
	"CSSStyleDeclaration", JSCLASS_HAS_PRIVATE
};
static JSPropertySpec canvas_style_properties[] = {
	JS_PSGS("width", Canvas::js_get_styleWidth, Canvas::js_set_styleWidth, JSPROP_PERMANENT | JSPROP_ENUMERATE),
	JS_PSGS("height", Canvas::js_get_styleHeight, Canvas::js_set_styleHeight, JSPROP_PERMANENT | JSPROP_ENUMERATE),
	JS_PSGS("left", Canvas::js_get_styleLeft, Canvas::js_set_styleLeft, JSPROP_PERMANENT | JSPROP_ENUMERATE),
	JS_PSGS("top", Canvas::js_get_styleTop, Canvas::js_set_styleTop, JSPROP_PERMANENT | JSPROP_ENUMERATE),
	JS_PS_END
};
bool Canvas::_hasScreenCanvas = false;
bool Canvas::constructor(JSContext *ctx, unsigned argc, JS::Value *vp)
{
	JS_BEGIN_ARG;	
	JS::RootedObject obj(ctx, JS_NewObjectForConstructor(ctx, &canvas_class, args));	
	Canvas* canvas = new Canvas(_hasScreenCanvas);
	canvas->wrap(ctx, obj);
	if (!canvas->_isOffscreen) {		
		canvas->_target = Core::getInstance()->_screen;		
		gpu::GetVirtualResolution(canvas->_target, (uint16_t*)&canvas->_width, (uint16_t*)&canvas->_height);
		SDL_GetWindowPosition(Core::getInstance()->_window, &canvas->_styleLeft, &canvas->_styleTop);
		SDL_GetWindowSize(Core::getInstance()->_window, &canvas->_styleWidth, &canvas->_styleHeight);

		JS::RootedObject styleobj(ctx, JS_NewObject(ctx, &canvas_style_class));		
		JS_DefineProperties(ctx, styleobj, canvas_style_properties);
		JS_SetPrivate(styleobj, canvas);
		JS::RootedObject parent(ctx, obj);
		JS_DefineProperty(ctx, parent, "style", styleobj, JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE);
	}	
	JS_RET(obj);
}

Canvas::Canvas(bool offscreen)
: _target(nullptr), _context(nullptr), _texture(nullptr), _isOffscreen(offscreen), _width(0), _height(0), _src("")
{
	if (!offscreen) _hasScreenCanvas = true;
}

Canvas::~Canvas()
{
	SAFE_DELETE(_context);
	if (_texture) gpu::FreeImage(_texture);	
}

void Canvas::resize()
{
	if (_isOffscreen) {
		if (_texture) {
			gpu::FreeImage(_texture);
			_texture = nullptr;
		}
		if (_width > 0 && _height > 0) {
			_texture = gpu::CreateImage(_width, _height, gpu::FORMAT_RGBA);
		}
	}
	else {
		gpu::SetVirtualResolution(_target, _width, _height);
	}
}

void Canvas::makeTarget()
{
	if (!_target && _texture) {
		_target = gpu::LoadTarget(_texture);
		if (_context) _context->_target = _target;
	}
}

JS_PROPGET_IMPL(Canvas, width)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_width);	
}

JS_PROPSET_IMPL(Canvas, width)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(width, 0);
	width = std::max<int>(width, 0);
	if (pthis->_width != width) {
		pthis->_width = width;
		pthis->resize();
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, height)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_height);
}

JS_PROPSET_IMPL(Canvas, height)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(height, 0);
	height = std::max<int>(height, 0);
	if (pthis->_height != height) {
		pthis->_height = height;
		pthis->resize();
	}
	JS_RETURN;
}

JS_FUNC_IMPL(Canvas, getContext)
{
	JS_BEGIN_ARG_THIS(Canvas);	
	JS_STRING_ARG(kind, 0);
	if (argc > 1) {
	}
	if (strcmp(kind, "2d") == 0) {
		if (pthis->_context) {
			if (pthis->_context->getType() == kContextType2D) {				
				JS::RootedValue context(ctx);
				if (JS_GetProperty(ctx, js_this, "_context", &context)) {
					JS_RET(context);
				}
				else {
					JS_FAIL("Fatal Error: Invalid wrap object");
				}
			}
		}
		else {
			CanvasContext2D* context2d = new CanvasContext2D(pthis);			
			pthis->_context = context2d;
			JS::Rooted<JSObject*> robj(ctx, context2d->createObject(ctx));
			JS_DefineProperty(ctx, js_this, "_context", robj, JSPROP_PERMANENT | JSPROP_READONLY);			
			JS_RET(robj);
		}
	}
	else if (strcmp(kind, "webgl") == 0 || strcmp(kind, "experimental-webgl") == 0) {
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, styleWidth)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_styleWidth);	
}

JS_PROPSET_IMPL(Canvas, styleWidth)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(width, 0);
	pthis->_styleWidth = std::max<int>(width, 0);
	gpu::SetWindowResolution(pthis->_styleWidth, pthis->_styleHeight);
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, styleHeight)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_styleHeight);	
}

JS_PROPSET_IMPL(Canvas, styleHeight)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(height, 0);
	pthis->_styleHeight = std::max<int>(height, 0);
	gpu::SetWindowResolution(pthis->_styleWidth, pthis->_styleHeight);
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, styleLeft)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_styleLeft);	
}

JS_PROPSET_IMPL(Canvas, styleLeft)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(left, 0);
	pthis->_styleLeft = left;
	//SDL_SetWindowPosition(Core::getInstance()->_window, pthis->_styleLeft, pthis->_styleTop);
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, styleTop)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_styleTop);	
}

JS_PROPSET_IMPL(Canvas, styleTop)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_INT_ARG(top, 0);
	pthis->_styleTop = top;
	//SDL_SetWindowPosition(Core::getInstance()->_window, pthis->_styleLeft, pthis->_styleTop);
	JS_RETURN;
}

JS_PROPGET_IMPL(Canvas, src)
{
	JS_BEGIN_ARG_THIS(Canvas);
	JS_RET(pthis->_src.c_str());
}

void Canvas::fetchCallback(gpu::Image * image)
{
	JSContext* ctx = ScriptCore::getInstance()->getGlobalContext();
	JS::RootedObject jsthis(ctx, handle());
	JS::RootedValue jscallback(ctx);
	unref();
	if (image) {
		_texture = image;
		_width = image->w;
		_height = image->h;
		JS_GetProperty(ctx, jsthis, "onload", &jscallback);
	}
	else {
		JS_GetProperty(ctx, jsthis, "onerror", &jscallback);
	}
	if (jscallback.isObject()) { // is a Function object?						
		JS::RootedValue rval(ctx);
		JS_CallFunctionValue(ctx, jsthis, jscallback, JS::HandleValueArray::empty(), &rval);
	}
}

JS_PROPSET_IMPL(Canvas, src)
{
	JS_BEGIN_ARG_THIS(Canvas);
	if (pthis->_isOffscreen) {
		JS_STRING_ARG(fileName, 0);
		decodeURIComponent(fileName);		
		pthis->_src = fileName;
		if (pthis->_texture) {
			gpu::FreeImage(pthis->_texture);
			pthis->_texture = nullptr;
		}
		pthis->ref(ctx);
		ImageManager::getInstance()->fetchImageAsync(pthis->_src, std::bind(&Canvas::fetchCallback, pthis, std::placeholders::_1));
	}
	JS_RETURN;
}

NS_REK_END