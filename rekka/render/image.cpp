#include "image.h"
#include "image_manager.h"

NS_REK_BEGIN

static JSClass image_class = {
	"Image", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	ObjectWrap::destructor
};
void Image::jsb_register(JSContext *ctx, JS::HandleObject parent)
{
	static JSPropertySpec image_properties[] = {
		JS_PROP_DEF(Image, src),
		JS_PROP_GET_DEF(Image, width),
		JS_PROP_GET_DEF(Image, height),
		JS_PROP_GET_DEF(Image, complete),
		JS_PS_END
	};
	JS_InitClass(ctx, parent, nullptr, &image_class, Image::constructor, 0, image_properties, 0, 0, 0);
}
bool Image::is_js_instance(JS::HandleObject obj)
{
	return JS_GetClass(obj) == &image_class;
}

Image::Image()
: _src(""), _texture(nullptr)
{
}
Image::~Image()
{	
	if (_texture) gpu::FreeImage(_texture);
}

bool Image::constructor(JSContext *ctx, unsigned argc, JS::Value *vp)
{
	JS_BEGIN_ARG;
	JS::RootedObject obj(ctx, JS_NewObjectForConstructor(ctx, &image_class, args));	
	Image* image = new Image();
	image->wrap(ctx, obj);
	JS_RET(obj);	
}

JS_PROPGET_IMPL(Image, src)
{	
	JS_BEGIN_ARG_THIS(Image);
	JS_RET(pthis->_src.c_str());
}

void Image::fetchCallback(gpu::Image * image)
{
	JSContext* ctx = ScriptCore::getInstance()->getGlobalContext();
	JS::RootedObject jsthis(ctx, handle());
	JS::RootedValue jscallback(ctx);
	unref();
	if (image) {
		_texture = image;		
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

JS_PROPSET_IMPL(Image, src)
{
	JS_BEGIN_ARG_THIS(Image);
	JS_STRING_ARG(fileName, 0);	
	decodeURIComponent(fileName);	
	pthis->_src = fileName;
	if (pthis->_texture) {
		gpu::FreeImage(pthis->_texture);
		pthis->_texture = nullptr;
	}	
	pthis->ref(ctx);
	ImageManager::getInstance()->fetchImageAsync(pthis->_src, std::bind(&Image::fetchCallback, pthis, std::placeholders::_1));
	JS_RETURN;
}

JS_PROPGET_IMPL(Image, width)
{
	JS_BEGIN_ARG_THIS(Image);
	int width = pthis->_texture ? pthis->_texture->base_w : 0;
	JS_RET(width);	
}

JS_PROPGET_IMPL(Image, height)
{
	JS_BEGIN_ARG_THIS(Image);
	int height = pthis->_texture ? pthis->_texture->base_h : 0;
	JS_RET(height);
}

JS_PROPGET_IMPL(Image, complete)
{
	JS_BEGIN_ARG_THIS(Image);
	JS_RET(pthis->_texture != nullptr);
}

NS_REK_END