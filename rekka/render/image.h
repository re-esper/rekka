#pragma once

#include "rekka.h"
#include "script_core.h"

NS_REK_BEGIN

class Image : public ObjectWrap {
public:
	Image();
	~Image();
	static void jsb_register(JSContext *ctx, JS::HandleObject parent);
	static bool is_js_instance(JS::HandleObject obj);
private:
	static bool constructor(JSContext *ctx, unsigned argc, JS::Value *vp);
	JS_PROPGET_DECL(src)
	JS_PROPSET_DECL(src)
	JS_PROPGET_DECL(width)
	JS_PROPGET_DECL(height)
	JS_PROPGET_DECL(complete)
private:
	void fetchCallback(gpu::Image* image);
public:	
	gpu::Image* _texture;
private:
	std::string _src;	
};

NS_REK_END