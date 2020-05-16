#include "context_2d.h"
#include "script_core.h"
#include "render/image.h"
#include "render/canvas.h"
#include "render/extra.h"

NS_REK_BEGIN

static JSClass context2d_class = {
	"CanvasContext2D", JSCLASS_HAS_PRIVATE,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
static JSPropertySpec context2d_properties[] = {
	JS_PROP_DEF(CanvasContext2D, globalCompositeOperation),
	JS_PROP_DEF(CanvasContext2D, globalAlpha),
	JS_PROP_DEF(CanvasContext2D, fillStyle),
	JS_PROP_DEF(CanvasContext2D, strokeStyle),
	JS_PROP_DEF(CanvasContext2D, lineWidth),
	JS_PROP_DEF(CanvasContext2D, font),
	JS_PROP_DEF(CanvasContext2D, textAlign),
	JS_PROP_DEF(CanvasContext2D, textBaseline),
	JS_PS_END
};
static JSFunctionSpec context2d_funcs[] = {
	JS_FUNC_DEF(CanvasContext2D, save),
	JS_FUNC_DEF(CanvasContext2D, restore),
	JS_FUNC_DEF(CanvasContext2D, fillText),
	JS_FUNC_DEF(CanvasContext2D, strokeText),
	JS_FUNC_DEF(CanvasContext2D, measureText),
	JS_FUNC_DEF(CanvasContext2D, setTransform),
	JS_FUNC_DEF(CanvasContext2D, rotate),
	JS_FUNC_DEF(CanvasContext2D, scale),
	JS_FUNC_DEF(CanvasContext2D, translate),
	JS_FUNC_DEF(CanvasContext2D, drawImage),
	JS_FUNC_DEF(CanvasContext2D, clearRect),
	JS_FUNC_DEF(CanvasContext2D, fillRect),
	JS_FUNC_DEF(CanvasContext2D, strokeRect),
	JS_FUNC_DEF(CanvasContext2D, createImageData),
	JS_FUNC_DEF(CanvasContext2D, getImageData),
	JS_FUNC_DEF(CanvasContext2D, putImageData),
	JS_FUNC_DEF(CanvasContext2D, beginPath),
	JS_FUNC_DEF(CanvasContext2D, closePath),
	JS_FUNC_DEF(CanvasContext2D, fill),
	JS_FUNC_DEF(CanvasContext2D, stroke),
	JS_FUNC_DEF(CanvasContext2D, moveTo),
	JS_FUNC_DEF(CanvasContext2D, lineTo),
	JS_FUNC_DEF(CanvasContext2D, rect),
	JS_FUNC_DEF(CanvasContext2D, bezierCurveTo),
	JS_FUNC_DEF(CanvasContext2D, quadraticCurveTo),
	JS_FUNC_DEF(CanvasContext2D, arc),
	JS_FUNC_DEF(CanvasContext2D, arcTo),
	JS_FUNC_DEF(CanvasContext2D, clip),
	JS_FUNC_DEF(CanvasContext2D, createPattern),
	JS_FUNC_DEF(CanvasContext2D, createLinearGradient),
	JS_FUNC_DEF(CanvasContext2D, createRadialGradient),
	JS_FUNC_DEF(CanvasContext2D, tintImage),
	JS_FS_END
};


#define PREPARE_IMAGE_OPERATION(context, image) \
	context->checkTarget(); \
	int op = context->_state->globalCompositeOperation; \
	gpu::BlendFuncEnum src = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].source; \
	gpu::BlendFuncEnum dest = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].destination; \
	if (image->blend_mode.source_color != src || image->blend_mode.dest_color != dest) \
		gpu::SetBlendFunction(image, src, dest, src, dest)
#define PREPARE_FILL_OPERATION(context) \
	context->checkTarget(); \
	int op = context->_state->globalCompositeOperation; \
	gpu::BlendFuncEnum src = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].source; \
	gpu::BlendFuncEnum dest = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].destination; \
	gpu::SetShapeBlendFunction(src, dest, src, dest)
#define PREPARE_STROKE_OPERATION(context) \
	context->checkTarget(); \
	gpu::SetLineThickness(context->_state->lineWidth); \
	int op = context->_state->globalCompositeOperation; \
	gpu::BlendFuncEnum src = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].source; \
	gpu::BlendFuncEnum dest = (gpu::BlendFuncEnum)_compositeOperationFuncs[op].destination; \
	gpu::SetShapeBlendFunction(src, dest, src, dest)
#define PREPARE_TARGET(context) context->checkTarget()
CanvasContext2D::CanvasContext2D(Canvas* canvas) : CanvasContext(canvas)
{
	Context2DState state;
	_stateStack.push_back(state);
	_state = &_stateStack.back();	
}
CanvasContext2D::~CanvasContext2D()
{
}

JSObject* CanvasContext2D::createObject(JSContext* ctx)
{
	JS::RootedObject robj(ctx, JS_NewObject(ctx, &context2d_class));
	JS_DefineFunctions(ctx, robj, context2d_funcs);
	JS_DefineProperties(ctx, robj, context2d_properties);
	JS_SetPrivate(robj, this);
	return robj;
}

JS_PROPGET_IMPL(CanvasContext2D, globalCompositeOperation)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_RET(_globalCompositeOperation_enum_names[pthis->_state->globalCompositeOperation]);		
}
JS_PROPSET_IMPL(CanvasContext2D, globalCompositeOperation)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(op, 0);
	for (int idx = 0; _globalCompositeOperation_enum_names[idx]; ++idx) {
		if (strcmp(_globalCompositeOperation_enum_names[idx], op) == 0) {
			pthis->_state->globalCompositeOperation = (CompositeOperation)idx;
			JS_RETURN;
		}
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, globalAlpha)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_RET(pthis->_state->globalAlpha);	
}

JS_PROPSET_IMPL(CanvasContext2D, globalAlpha)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(alpha, 0);	
	if (pthis->_target && pthis->_state->globalAlpha != alpha) {
		gpu::SetTargetRGBA(pthis->_target, 0xff, 0xff, 0xff, std::min<int>(255, std::max<int>(alpha * 255, 0)));
	}
	pthis->_state->globalAlpha = alpha;	
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, fillStyle)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	if (pthis->_state->fillObject) {
		JS_RET(pthis->_state->fillObject->handle());
	}
	JS_RET(ColorToHtmlColor(pthis->_state->fillColor));	
}

JS_PROPSET_IMPL(CanvasContext2D, fillStyle)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	if (args[0].isString()) {
		JS_STRING_ARG(fillstyle, 0);
		pthis->_state->fillColor = HtmlColorToColor(fillstyle);
		pthis->_state->fillObject = nullptr;
	}
	else {
		JS_ROOT_OBJECT_ARG(obj, 0);
		if (FillObject::is_js_instance(obj)) {			
			FillObject* fillobj = (FillObject*)JS_GetPrivate(obj);
			pthis->_state->fillObject = fillobj;			
		}
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, strokeStyle)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	if (pthis->_state->strokeObject) {
		JS_RET(pthis->_state->strokeObject->handle());
	}	
	JS_RET(ColorToHtmlColor(pthis->_state->strokeColor));	
}

JS_PROPSET_IMPL(CanvasContext2D, strokeStyle)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);	
	if (args[0].isString()) {
		JS_STRING_ARG(strokestyle, 0);
		pthis->_state->strokeColor = HtmlColorToColor(strokestyle);
		pthis->_state->strokeObject = nullptr;
	}
	else {
		JS_ROOT_OBJECT_ARG(obj, 0);
		if (FillObject::is_js_instance(obj)) {			
			FillObject* fillobj = (FillObject*)JS_GetPrivate(obj);
			pthis->_state->strokeObject = fillobj;	
		}
	}	
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, lineWidth)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_RET(pthis->_state->lineWidth);	
}

JS_PROPSET_IMPL(CanvasContext2D, lineWidth)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(width, 0);
	pthis->_state->lineWidth = width;
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, setTransform)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(a, 0);
	JS_DOUBLE_ARG(b, 1);
	JS_DOUBLE_ARG(c, 2);
	JS_DOUBLE_ARG(d, 3);
	JS_DOUBLE_ARG(tx, 4);
	JS_DOUBLE_ARG(ty, 5);
	pthis->_state->transform = gpu::AffineTransformMake(a, b, c, d, tx, ty);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, rotate)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(r, 0);
	pthis->_state->transform = gpu::AffineTransformRotate(&pthis->_state->transform, r);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, scale)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(sx, 0);
	JS_DOUBLE_ARG(sy, 1);
	pthis->_state->transform = gpu::AffineTransformScale(&pthis->_state->transform, sx, sy);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, translate)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(tx, 0);
	JS_DOUBLE_ARG(ty, 1);
	pthis->_state->transform = gpu::AffineTransformTranslate(&pthis->_state->transform, tx, ty);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, drawImage)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_ROOT_OBJECT_ARG(obj, 0);
	void* ptr = JS_GetPrivate(obj);
	gpu::Image* image = nullptr;
	if (Image::is_js_instance(obj)) {
		image = ((Image*)ptr)->_texture;
	}
	else if (Canvas::is_js_instance(obj)) {
		image = ((Canvas*)ptr)->_texture;
	}
	if (!image) JS_RETURN;

	PREPARE_IMAGE_OPERATION(pthis, image);	
	if (argc == 3) { // drawImage(image, dx, dy)
		JS_DOUBLE_ARG(x, 1);
		JS_DOUBLE_ARG(y, 2);
		gpu::BlitTransformA(image, nullptr, pthis->_target, x, y, &pthis->_state->transform);		
	}
	else if (argc == 5) { // drawImage(image, dx, dy, dw, dh)
		JS_DOUBLE_ARG(x, 1);
		JS_DOUBLE_ARG(y, 2);
		JS_DOUBLE_ARG(w, 3);
		JS_DOUBLE_ARG(h, 4);		
		gpu::BlitTransformAScale(image, nullptr, pthis->_target, x, y, &pthis->_state->transform, w / image->w, h / image->h);
	}
	else if (argc == 9) { // drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh)
		JS_DOUBLE_ARG(sx, 1);
		JS_DOUBLE_ARG(sy, 2);
		JS_DOUBLE_ARG(sw, 3);
		JS_DOUBLE_ARG(sh, 4);
		JS_DOUBLE_ARG(x, 5);
		JS_DOUBLE_ARG(y, 6);
		JS_DOUBLE_ARG(w, 7);
		JS_DOUBLE_ARG(h, 8);
		GPU_Rect texrect = { sx, sy, sw, sh };
		gpu::BlitTransformAScale(image, &texrect, pthis->_target, x, y, &pthis->_state->transform, w / texrect.w, h / texrect.h);
	}
	else {
		JS_FAIL("Wrong number of arguments: %d, was expecting 3, 5 or 9", argc);		
	}
	JS_RETURN;
}

void CanvasContext2D::clearRect(float x, float y, float w, float h)
{
	auto transform = &_state->transform;
	if (IsAffineTransformIdentity(transform)) {
		if (x == 0 && y == 0 && w == _target->w && h == _target->h) {			
			gpu::ClearRGBA(_target, 0, 0, 0, 0);
		}
		else {			
			gpu::SetClip(_target, x, y, w, h);
			gpu::ClearRGBA(_target, 0, 0, 0, 0);
			gpu::UnsetClip(_target);
			if (_state->clipRect.w > 0 && _state->clipRect.h > 0) {
				gpu::SetClipRect(_target, _state->clipRect);
			}
		}
	}
	else {		
		const auto& p1 = gpu::PointApplyAffineTransform(x, y, transform);
		const auto& p2 = gpu::PointApplyAffineTransform(x + w, y, transform);
		const auto& p3 = gpu::PointApplyAffineTransform(x + w, y + h, transform);
		const auto& p4 = gpu::PointApplyAffineTransform(x, y + h, transform);
		float points[] = { p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y };
		gpu::SetShapeBlendFunction(gpu::FUNC_ZERO, gpu::FUNC_ONE_MINUS_SRC_ALPHA, gpu::FUNC_ZERO, gpu::FUNC_ONE_MINUS_SRC_ALPHA);
		gpu::PolygonFilled(_target, 4, points, { 0xff, 0xff, 0xff, 0xff });		
	}
}

JS_FUNC_IMPL(CanvasContext2D, clearRect)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	JS_DOUBLE_ARG(w, 2);
	JS_DOUBLE_ARG(h, 3);
	PREPARE_TARGET(pthis);
	pthis->clearRect(x, y, w, h);
	JS_RETURN;
}

void CanvasContext2D::fillRect(float x, float y, float w, float h, const SDL_Color & color)
{
	auto transform = &_state->transform;
	if (IsAffineTransformAxisAligned(transform)) {
		if (_state->globalCompositeOperation == kCompositeOperationSourceOver && _state->globalAlpha == 1.0) {
			if (x == 0 && y == 0 && w == _target->w && h == _target->h) {
				gpu::ClearColor(_target, color);
			}
			else {
				gpu::SetClip(_target, x, y, w, h);
				gpu::ClearColor(_target, color);
				gpu::UnsetClip(_target);
				if (_state->clipRect.w > 0 && _state->clipRect.h > 0) {
					gpu::SetClipRect(_target, _state->clipRect);
				}
			}
		}
		else {
			const auto& topleft = gpu::PointApplyAffineTransform(x, y, transform);
			const auto& bottomright = gpu::PointApplyAffineTransform(x + w, y + h, transform);
			gpu::RectangleFilled(_target, topleft.x, topleft.y, bottomright.x, bottomright.y, color);
		}
	}
	else {
		const auto& p1 = gpu::PointApplyAffineTransform(x, y, transform);
		const auto& p2 = gpu::PointApplyAffineTransform(x + w, y, transform);
		const auto& p3 = gpu::PointApplyAffineTransform(x + w, y + h, transform);
		const auto& p4 = gpu::PointApplyAffineTransform(x, y + h, transform);
		float points[] = { p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y };
		gpu::PolygonFilled(_target, 4, points, color);
	}
}

JS_FUNC_IMPL(CanvasContext2D, fillRect)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	JS_DOUBLE_ARG(w, 2);
	JS_DOUBLE_ARG(h, 3);
	PREPARE_FILL_OPERATION(pthis);
	if (pthis->_state->fillObject) {
		Path path;
		auto transform = &pthis->_state->transform;		
		path.moveTo(x, y, transform);
		path.lineTo(x + w, y, transform);
		path.lineTo(x + w, y + h, transform);
		path.lineTo(x, y + h, transform);
		path.closePath();		
		path.fill(pthis->_target, pthis->_state->fillObject, transform);		
	}
	else {		
		pthis->fillRect(x, y, w, h, pthis->_state->fillColor);
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, strokeRect)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	JS_DOUBLE_ARG(w, 2);
	JS_DOUBLE_ARG(h, 3);
	PREPARE_STROKE_OPERATION(pthis);
	Path path;
	auto transform = &pthis->_state->transform;	
	path.moveTo(x, y, transform);
	path.lineTo(x + w, y, transform);
	path.lineTo(x + w, y + h, transform);
	path.lineTo(x, y + h, transform);
	path.closePath();
	if (pthis->_state->fillObject) {
		path.stroke(pthis->_target, pthis->_state->strokeObject, transform);
	}
	else {
		path.stroke(pthis->_target, pthis->_state->strokeColor);
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, createImageData)
{
	JS_BEGIN_ARG;
	JS_INT_ARG(w, 0);
	JS_INT_ARG(h, 1);
	if (w > 0 && h > 0) {		
		JS::RootedObject imagedata(ctx, JS_NewObject(ctx, nullptr));
		JS::RootedValue jsdata(ctx, JS::ObjectOrNullValue(JS_NewUint8ClampedArray(ctx, w * h * 4)));
		JS::RootedValue jsw(ctx), jsh(ctx);
		jsw.setInt32(w);
		jsh.setInt32(h);
		JS_SetProperty(ctx, imagedata, "data", jsdata);
		JS_SetProperty(ctx, imagedata, "width", jsw);
		JS_SetProperty(ctx, imagedata, "height", jsh);
		JS_RET(imagedata);
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, getImageData)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_INT_ARG(x, 0);
	JS_INT_ARG(y, 1);
	JS_INT_ARG(w, 2);
	JS_INT_ARG(h, 3);
	PREPARE_TARGET(pthis);
	if (x >= 0 && y >= 0 && w > 0 && h > 0 &&
		x + w <= pthis->_target->w && y + h <= pthis->_target->h) {
		auto bufferarray = JS_NewUint8ClampedArray(ctx, w * h * 4);
		uint8_t* data;
		uint32_t length;
		bool isSharedMemory;		
		JS_GetObjectAsUint8ClampedArray(bufferarray, &length, &isSharedMemory, &data);	
		if (!gpu::GetImageData(pthis->_target, data, x, y, w, h)) {
			JS_FAIL("Failed to fetch image data");
		}
		JS::RootedObject imagedata(ctx, JS_NewObject(ctx, nullptr));		
		JS::RootedValue jsdata(ctx, JS::ObjectOrNullValue(bufferarray));
		JS::RootedValue jsw(ctx), jsh(ctx);
		jsw.setInt32(w);
		jsh.setInt32(h);
		JS_SetProperty(ctx, imagedata, "data", jsdata);
		JS_SetProperty(ctx, imagedata, "width", jsw);
		JS_SetProperty(ctx, imagedata, "height", jsh);
		JS_RET(imagedata);		
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, putImageData)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_ROOT_OBJECT_ARG(imagedata, 0);
	JS_INT_ARG(x, 1);
	JS_INT_ARG(y, 2);
	if (!pthis->_isOffscreen) {
		JS_FAIL("To screen canvas is not supported");		
	}
	if (argc > 3) {
		JS_FAIL("dirtyX/dirtyY/dirtyWidth/dirtyHeight is not supported");		
	}
	JS::RootedValue v(ctx);
	int width = 0, height = 0;
	JS_GetProperty(ctx, imagedata, "width", &v);
	JS::ToInt32(ctx, v, &width);
	JS_GetProperty(ctx, imagedata, "height", &v);
	JS::ToInt32(ctx, v, &height);
	if (!JS_GetProperty(ctx, imagedata, "data", &v)) {
		JS_FAIL("Invalid image data");		
	}	
	uint8_t* data = nullptr;
	uint32_t length = 0;
	bool isSharedMemory;
	JS_GetObjectAsUint8ClampedArray(JS::ToObject(ctx, v), &length, &isSharedMemory, &data);	
	GPU_Rect rect = { x, y, width, height };
	gpu::UpdateImageBytes(pthis->_owner->_texture, &rect, data, 4 * width);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, beginPath)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	pthis->_path.beginPath();
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, closePath)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	pthis->_path.closePath();
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, fill)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	PREPARE_FILL_OPERATION(pthis);
	if (pthis->_state->fillObject) {
		pthis->_path.fill(pthis->_target, pthis->_state->fillObject, &pthis->_state->transform);
	}
	else {
		pthis->_path.fill(pthis->_target, pthis->_state->fillColor);
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, stroke)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	PREPARE_STROKE_OPERATION(pthis);
	if (pthis->_state->strokeObject) {
		pthis->_path.stroke(pthis->_target, pthis->_state->strokeObject, &pthis->_state->transform);
	}
	else {
		pthis->_path.stroke(pthis->_target, pthis->_state->strokeColor);
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, moveTo)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	pthis->_path.moveTo(x, y, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, lineTo)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	pthis->_path.lineTo(x, y, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, bezierCurveTo)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(cpx1, 0);
	JS_DOUBLE_ARG(cpy1, 1);
	JS_DOUBLE_ARG(cpx2, 2);
	JS_DOUBLE_ARG(cpy2, 3);
	JS_DOUBLE_ARG(x, 4);
	JS_DOUBLE_ARG(y, 5);
	pthis->_path.bezierCurveTo(cpx1, cpy1, cpx2, cpy2, x, y, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, quadraticCurveTo)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(cpx, 0);
	JS_DOUBLE_ARG(cpy, 1);
	JS_DOUBLE_ARG(x, 2);
	JS_DOUBLE_ARG(y, 3);	
	pthis->_path.quadraticCurveTo(cpx, cpy, x, y, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, arc)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	JS_DOUBLE_ARG(radius, 2);
	JS_DOUBLE_ARG(startangle, 3);
	JS_DOUBLE_ARG(endangle, 4);	
	if (argc > 5) {
		JS_BOOL_ARG(counterclockwise, 5);		
		pthis->_path.arc(x, y, radius, startangle, endangle, counterclockwise, &pthis->_state->transform);
	}
	pthis->_path.arc(x, y, radius, startangle, endangle, false, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, arcTo)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x1, 0);
	JS_DOUBLE_ARG(y1, 1);
	JS_DOUBLE_ARG(x2, 2);
	JS_DOUBLE_ARG(y2, 3);
	JS_DOUBLE_ARG(radius, 4);
	pthis->_path.arcTo(x1, y1, x2, y2, radius, &pthis->_state->transform);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, rect)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x, 0);
	JS_DOUBLE_ARG(y, 1);
	JS_DOUBLE_ARG(w, 2);
	JS_DOUBLE_ARG(h, 3);
	auto& path = pthis->_path;
	auto transform = &pthis->_state->transform;
	path.moveTo(x, y, transform);
	path.lineTo(x + w, y, transform);
	path.lineTo(x + w, y + h, transform);
	path.lineTo(x, y + h, transform);
	path.closePath();
	if (IsAffineTransformAxisAligned(transform)) {
		const auto& pt = gpu::PointApplyAffineTransform(x, y, transform);
		const auto& size = gpu::SizeApplyAffineTransform(w, h, transform);
		pthis->_state->lastPathRect = { pt.x, pt.y, size.x, size.y };
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, clip)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	auto& rect = pthis->_state->lastPathRect;
	if (rect.w > 0 && rect.h > 0) { 
		pthis->_state->clipRect = rect;
		if (pthis->_target) gpu::SetClipRect(pthis->_target, rect);
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, font)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	auto& font = pthis->_state->font;
	const char* family = FontManager::getInstance()->familyNameById(font.familyId);
	if (family) {
		std::string fontstr = font.italic ? "italic " : "";
		if (font.bold) fontstr += "bold ";
		char sizestr[10];
		sprintf(sizestr, "%dpx ", font.size);
		fontstr += sizestr;
		fontstr += family;
		JS_RET(fontstr.c_str());
	}
	JS_RETURN;
}

// 	支持 "italic bold|bolder 12px|40pt arial, sans-serif"
JS_PROPSET_IMPL(CanvasContext2D, font)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(fontdesc, 0);		
	std::vector<std::string> familyNames;	
	bool italic = false;
	bool bold = false;
	float size = 0;
	char *p = strtok(fontdesc, " ,");
	while (p) {
		if (strcasecmp(p, "italic") == 0) {
			italic = true;
		}
		else if (strcasecmp(p, "bold") == 0 || strcasecmp(p, "bolder") == 0) {
			bold = true;
		}
		else {
			int ptx = 0; // int以防止在MSC下溢出
			sscanf(p, "%fp%1[tx]", &size, (char *)&ptx);
			if (size > 0 && (ptx == 't' || ptx == 'x')) {
				if (ptx == 't') size = ceilf(size * 4.0 / 3.0);
			}
			else { // 假定是字体名
				familyNames.push_back(p);
			}
		}
		p = strtok(0, " ,");
	}
	int fontId = FontManager::getInstance()->findFontId(familyNames);
	if (fontId != -1) {		
		pthis->_state->font.italic = italic;
		pthis->_state->font.bold = bold;
		pthis->_state->font.familyId = fontId;
		pthis->_state->font.size = roundf(size);
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, textAlign)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_RET(_textAlign_enum_names[pthis->_state->textAlign]);	
}

JS_PROPSET_IMPL(CanvasContext2D, textAlign)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(align, 0);
	for (int idx = 0; _textAlign_enum_names[idx]; ++idx) {
		if (strcmp(_textAlign_enum_names[idx], align) == 0) {
			pthis->_state->textAlign = (TextAlign)idx;
			JS_RETURN;
		}
	}
	JS_RETURN;
}

JS_PROPGET_IMPL(CanvasContext2D, textBaseline)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_RET(_textBaseline_enum_names[pthis->_state->textBaseline]);	
}

JS_PROPSET_IMPL(CanvasContext2D, textBaseline)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(baseline, 0);
	for (int idx = 0; _textBaseline_enum_names[idx]; ++idx) {
		if (strcmp(_textBaseline_enum_names[idx], baseline) == 0) {
			pthis->_state->textBaseline = (TextBaseline)idx;
			JS_RETURN;
		}
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, fillText)
{	
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(text, 0);
	if (text[0] == 0) JS_RETURN;
	JS_DOUBLE_ARG(x, 1);
	JS_DOUBLE_ARG(y, 2);
	auto& state = pthis->_state;	
	auto image = FontManager::getInstance()->drawText(text, pthis->_isOffscreen, state->font,
		state->textBaseline, state->textAlign);	
	if (image == nullptr) JS_FAIL("Failed to render the text \"%s\"", text);

	PREPARE_IMAGE_OPERATION(pthis, image);
	auto fillobj = pthis->_state->fillObject;
	gpu::ColorCallback colorfunc = nullptr;
	if (fillobj && dynamic_cast<Pattern*>(fillobj) == nullptr) {
		if (dynamic_cast<LinearGradient*>(fillobj) != nullptr)
			colorfunc = LinearGradient::colorFunc;
		else if (dynamic_cast<RadialGradient*>(fillobj))
			colorfunc = RadialGradient::colorFunc;
	}
	if (colorfunc) {
		GPU_Color colors[4];
		float x1 = -image->anchor_x + x;
		float y1 = -image->anchor_y + y;
		float x2 = (image->w - image->anchor_x) + x;
		float y2 = (image->h - image->anchor_y) + y;
		colors[0] = colorfunc(x1, y1, fillobj);
		colors[1] = colorfunc(x2, y1, fillobj);
		colors[2] = colorfunc(x2, y2, fillobj);
		colors[3] = colorfunc(x1, y2, fillobj);
		gpu::BlitTransformAColor(image, pthis->_target, x, y, &state->transform, colors);
	}
	else {
		gpu::SetColor(image, state->fillColor);
		gpu::BlitTransformA(image, nullptr, pthis->_target, x, y, &state->transform);		
	}
	if (pthis->_isOffscreen) gpu::FreeImage(image);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, strokeText)
{	
	JS_BEGIN_ARG_THIS(CanvasContext2D);		
	JS_STRING_ARG(text, 0);
	if (text[0] == 0) JS_RETURN;
	JS_DOUBLE_ARG(x, 1);
	JS_DOUBLE_ARG(y, 2);
	auto& state = pthis->_state;
	auto image = FontManager::getInstance()->drawText(text, pthis->_isOffscreen, state->font,
		state->textBaseline, state->textAlign,
		state->lineWidth, state->lineCap, state->lineJoin, state->miterLimit);
	if (image == nullptr) JS_FAIL("Failed to stroke the text \"%s\"", text);

	PREPARE_IMAGE_OPERATION(pthis, image);
	auto fillobj = pthis->_state->strokeObject;
	gpu::ColorCallback colorfunc = nullptr;
	if (fillobj && dynamic_cast<Pattern*>(fillobj) == nullptr) {
		if (dynamic_cast<LinearGradient*>(fillobj) != nullptr)
			colorfunc = LinearGradient::colorFunc;
		else if (dynamic_cast<RadialGradient*>(fillobj))
			colorfunc = RadialGradient::colorFunc;
	}
	if (colorfunc) {
		GPU_Color colors[4];
		float x1 = -image->anchor_x + x;
		float y1 = -image->anchor_y + y;
		float x2 = (image->w - image->anchor_x) + x;
		float y2 = (image->h - image->anchor_y) + y;
		colors[0] = colorfunc(x1, y1, fillobj);
		colors[1] = colorfunc(x2, y1, fillobj);
		colors[2] = colorfunc(x2, y2, fillobj);
		colors[3] = colorfunc(x1, y2, fillobj);
		gpu::BlitTransformAColor(image, pthis->_target, x, y, &state->transform, colors);
	}
	else {
		gpu::SetColor(image, state->strokeColor);
		gpu::BlitTransformA(image, nullptr, pthis->_target, x, y, &state->transform);
	}
	if (pthis->_isOffscreen) gpu::FreeImage(image);	
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, measureText)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_STRING_ARG(text, 0);
	int width = FontManager::getInstance()->measureText(text, pthis->_state->font);
	JS::RootedObject result(ctx, JS_NewObject(ctx, nullptr));	
	JS::RootedValue v(ctx);
	v.setDouble(width);
	JS_SetProperty(ctx, result, "width", v);
	JS_RET(result);	
}

JS_FUNC_IMPL(CanvasContext2D, createPattern)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_ROOT_OBJECT_ARG(obj, 0);
	auto ptr = JS_GetPrivate(obj);
	gpu::Image* image = nullptr;
	if (Image::is_js_instance(obj)) {
		image = ((Image*)ptr)->_texture;
	}
	else if (Canvas::is_js_instance(obj)) {
		image = ((Canvas*)ptr)->_texture;
	}
	if (!image) JS_FAIL("Invalid image");
	JS_STRING_ARG(repeatstr, 1);
	PatternRepeat repeat;
	for (int idx = 0; _patternRepeat_enum_names[idx]; ++idx) {
		if (strcmp(_patternRepeat_enum_names[idx], repeatstr) == 0) {
			repeat = (PatternRepeat)idx;
			break;
		}
	}
	Pattern* pattern = new Pattern(image, repeat);	
	JS_RET(pattern->createObject(ctx));
}

JS_FUNC_IMPL(CanvasContext2D, createLinearGradient)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x0, 0);
	JS_DOUBLE_ARG(y0, 1);
	JS_DOUBLE_ARG(x1, 2);
	JS_DOUBLE_ARG(y1, 3);
	LinearGradient* gradient = new LinearGradient(x0, y0, x1, y1);
	JS_RET(gradient->createObject(ctx));
}

JS_FUNC_IMPL(CanvasContext2D, createRadialGradient)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_DOUBLE_ARG(x0, 0);
	JS_DOUBLE_ARG(y0, 1);
	JS_DOUBLE_ARG(r0, 2);
	JS_DOUBLE_ARG(x1, 3);
	JS_DOUBLE_ARG(y1, 4);
	JS_DOUBLE_ARG(r1, 5);
	RadialGradient* gradient = new RadialGradient(x0, y0, r0, x1, y1, r1);
	JS_RET(gradient->createObject(ctx));
}

#define FETCH_GPU_COLOR(color, part, index) { \
	JS_GetElement(ctx, color##Array, index, &jsval); \
	JS::ToNumber(ctx, jsval, &val); \
	color.part = val / 255; \
}
JS_FUNC_IMPL(CanvasContext2D, tintImage)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	JS_ROOT_OBJECT_ARG(obj, 0);
	auto ptr = JS_GetPrivate(obj);
	gpu::Image* image = nullptr;
	if (Image::is_js_instance(obj)) {
		image = ((Image*)ptr)->_texture;
	}
	else if (Canvas::is_js_instance(obj)) {
		image = ((Canvas*)ptr)->_texture;
	}
	if (!image) JS_FAIL("Invalid image");
	PREPARE_IMAGE_OPERATION(pthis, image);
	JS_DOUBLE_ARG(x, 1);
	JS_DOUBLE_ARG(y, 2);
	JS_DOUBLE_ARG(w, 3);
	JS_DOUBLE_ARG(h, 4);
	GPU_Color blendColor = { 0, 0, 0, 0 };
	GPU_Color toneColor = { 0, 0, 0, 0 };	 
	JS_ROOT_OBJECT_ARG(blendColorArray, 5);	
	bool isarray = false;
	JS_IsArrayObject(ctx, blendColorArray, &isarray);
	if (isarray) {
		uint32_t length = 0;
		JS_GetArrayLength(ctx, blendColorArray, &length);		
		JS::RootedValue jsval(ctx);
		double val = 0;
		if (length > 0) FETCH_GPU_COLOR(blendColor, r, 0);
		if (length > 1) FETCH_GPU_COLOR(blendColor, g, 1);
		if (length > 2) FETCH_GPU_COLOR(blendColor, b, 2);
		if (length > 3) FETCH_GPU_COLOR(blendColor, a, 3);
	}	
	JS_ROOT_OBJECT_ARG(toneColorArray, 6);
	JS_IsArrayObject(ctx, toneColorArray, &isarray);
	if (isarray) {
		uint32_t length = 0;
		JS_GetArrayLength(ctx, toneColorArray, &length);
		JS::RootedValue jsval(ctx);
		double val = 0;
		if (length > 0) FETCH_GPU_COLOR(toneColor, r, 0);
		if (length > 1) FETCH_GPU_COLOR(toneColor, g, 1);
		if (length > 2) FETCH_GPU_COLOR(toneColor, b, 2);
		if (length > 3) FETCH_GPU_COLOR(toneColor, a, 3);
	}
	CanvasExtra::tintImage(image, pthis->_target, x, y, w, h, blendColor, toneColor);
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, save)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	if (pthis->_state) {
		pthis->_stateStack.push_back(*pthis->_state);
		pthis->_state = &pthis->_stateStack.back();
	}
	JS_RETURN;
}

JS_FUNC_IMPL(CanvasContext2D, restore)
{
	JS_BEGIN_ARG_THIS(CanvasContext2D);
	pthis->restoreState();
	JS_RETURN;
}

static bool IsRectEqual(const GPU_Rect& r1, const GPU_Rect& r2) { return r1.x == r2.x && r1.y == r2.y && r1.w == r2.w && r1.h == r2.h; }
void CanvasContext2D::restoreState()
{
	if (_stateStack.size() < 2) return;
	auto rs = &_stateStack[_stateStack.size() - 2];
	if (_target) {
		if (rs->globalAlpha != _state->globalAlpha) {
			gpu::SetTargetRGBA(_target, 0xff, 0xff, 0xff, std::min<int>(255, std::max<int>(rs->globalAlpha * 255, 0)));			
		}
		// 若两个状态的clipRect不同, 重新设置
		if (!IsRectEqual(rs->clipRect, _state->clipRect)) {
			gpu::UnsetClip(_target);
			if (rs->clipRect.w > 0 && rs->clipRect.h > 0) gpu::SetClipRect(_target, rs->clipRect);			
		}
	}
	_stateStack.pop_back();
	if (!_stateStack.empty()) _state = &_stateStack.back();
}

void CanvasContext2D::checkTarget()
{
	if (!_target) {
		_owner->makeTarget();
		if (_state->globalAlpha != 1.0) {
			gpu::SetTargetRGBA(_target, 0xff, 0xff, 0xff, std::min<int>(255, std::max<int>(_state->globalAlpha * 255, 0)));
		}
		const GPU_Rect& rect = _state->clipRect;
		if (rect.w > 0 && rect.h > 0) gpu::SetClipRect(_target, rect);		
	}
}
NS_REK_END