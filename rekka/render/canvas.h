#pragma once

#include "rekka.h"
#include "canvas_context.h"
#include "script_core.h"

NS_REK_BEGIN

class Canvas : public ObjectWrap {
	static bool _hasScreenCanvas;
public:	
	Canvas(bool offscreen);
	~Canvas();
	static void jsb_register(JSContext *ctx, JS::HandleObject parent);
	static bool is_js_instance(JS::HandleObject obj);
public:
	static bool constructor(JSContext *ctx, unsigned argc, JS::Value *vp);
	JS_PROPGET_DECL(width)
	JS_PROPSET_DECL(width)
	JS_PROPGET_DECL(height)
	JS_PROPSET_DECL(height)	
	JS_FUNC_DECL(getContext)
	// style
	JS_PROPGET_DECL(styleWidth)
	JS_PROPSET_DECL(styleWidth)
	JS_PROPGET_DECL(styleHeight)
	JS_PROPSET_DECL(styleHeight)
	JS_PROPGET_DECL(styleLeft)
	JS_PROPSET_DECL(styleLeft)
	JS_PROPGET_DECL(styleTop)
	JS_PROPSET_DECL(styleTop)
	// Rekka Extra
	JS_PROPGET_DECL(src)
	JS_PROPSET_DECL(src)
private:
	void resize();
	void fetchCallback(gpu::Image* image);
public:		
	gpu::Image* _texture;
	gpu::Target* _target;
	void makeTarget();
private:
	CanvasContext* _context;
	bool _isOffscreen;
	// 对于主屏画布, width/height是VirtualResolution, 真实像素数由style决定
	// 对离屏画布, width/height是实际大小
	int _width;
	int _height;
	int _styleWidth;
	int _styleHeight;
	int _styleLeft;
	int _styleTop;
	std::string _src;
};

NS_REK_END