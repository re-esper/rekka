#pragma once

#include "rekka.h"
#include "render/canvas_context.h"
#include "render/font_manager.h"
#include "path.h"
#include "fill_object.h"
#include "script_core.h"

NS_REK_BEGIN

typedef enum {
	kLineCapButt = 0,
	kLineCapRound,
	kLineCapSquare
} LineCap;

typedef enum {
	kLineJoinMiter = 0,
	kLineJoinBevel,
	kLineJoinRound
} LineJoin;

typedef enum {
	kCompositeOperationSourceOver = 0,
	kCompositeOperationLighter,
	kCompositeOperationLighten,
	kCompositeOperationDarker,
	kCompositeOperationDarken,
	kCompositeOperationDestinationOut,
	kCompositeOperationDestinationOver,
	kCompositeOperationSourceAtop,
	kCompositeOperationXOR,
	kCompositeOperationCopy,
	kCompositeOperationSourceIn,
	kCompositeOperationDestinationIn,
	kCompositeOperationSourceOut,
	kCompositeOperationDestinationAtop
} CompositeOperation;
static const char *_globalCompositeOperation_enum_names[] = {
	"source-over",
	"lighter",
	"lighten",
	"darker",
	"darken",
	"destination-out",
	"destination-over",
	"source-atop",
	"xor",
	"copy",
	"source-in",
	"destination-in",
	"source-out",
	"destination-atop",
	nullptr
};
static const struct { unsigned int source; unsigned int destination; } _compositeOperationFuncs[] = {
	{ gpu::FUNC_ONE, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // source-over
	{ gpu::FUNC_ONE, gpu::FUNC_DST_ALPHA }, // lighter
	{ gpu::FUNC_ONE, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // lighten
	{ gpu::FUNC_DST_COLOR, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // darker
	{ gpu::FUNC_DST_COLOR, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // darken
	{ gpu::FUNC_ZERO, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // destination-out
	{ gpu::FUNC_ONE_MINUS_DST_ALPHA, gpu::FUNC_ONE }, // destination-over
	{ gpu::FUNC_DST_ALPHA, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // source-atop
	{ gpu::FUNC_ONE_MINUS_DST_ALPHA, gpu::FUNC_ONE_MINUS_SRC_ALPHA }, // xor
	{ gpu::FUNC_ONE, gpu::FUNC_ZERO }, // copy
	{ gpu::FUNC_DST_ALPHA, gpu::FUNC_ZERO }, // source-in
	{ gpu::FUNC_ZERO, gpu::FUNC_SRC_ALPHA }, // destination-in
	{ gpu::FUNC_ONE_MINUS_DST_ALPHA, gpu::FUNC_ZERO }, // source-out
	{ gpu::FUNC_ONE_MINUS_DST_ALPHA, gpu::FUNC_SRC_ALPHA }, // destination-atop
};

struct Context2DState {
	AffineTransform transform;
	CompositeOperation globalCompositeOperation;
	SDL_Color fillColor;
	FillObject* fillObject;
	SDL_Color strokeColor;
	FillObject* strokeObject;
	float globalAlpha;
	float lineWidth;
	LineCap lineCap;
	LineJoin lineJoin;
	float miterLimit;
	Font font;
	TextBaseline textBaseline;
	TextAlign textAlign;	
	GPU_Rect clipRect;
	GPU_Rect lastPathRect;
	Context2DState() : globalCompositeOperation(kCompositeOperationSourceOver), globalAlpha(1.0f),		
		lineWidth(1.0f), lineCap(kLineCapButt), lineJoin(kLineJoinMiter), miterLimit(10),
		fillObject(nullptr), strokeObject(nullptr),
		textBaseline(kTextBaselineAlphabetic), textAlign(kTextAlignStart)
	{
		transform = gpu::AffineTransformMakeIdentity();
		fillColor = { 0, 0, 0, 0xff };
		strokeColor = { 0, 0, 0, 0xff };
		font = { 0/* sans-serif */, 10/* 10px */, 0/* no italic */, 0/* no bold */ };
		clipRect = { 0, 0, 0, 0 };
		lastPathRect = { 0, 0, 0, 0 };		
	}
};

class CanvasContext2D : public CanvasContext
{
public:
	CanvasContext2D(Canvas* canvas);
	~CanvasContext2D();
public:
	// State
	JS_PROPGET_DECL(globalCompositeOperation)
	JS_PROPSET_DECL(globalCompositeOperation)
	JS_PROPGET_DECL(globalAlpha)
	JS_PROPSET_DECL(globalAlpha)
	JS_PROPGET_DECL(fillStyle)
	JS_PROPSET_DECL(fillStyle)
	JS_PROPGET_DECL(strokeStyle)
	JS_PROPSET_DECL(strokeStyle)
	JS_PROPGET_DECL(lineWidth)
	JS_PROPSET_DECL(lineWidth)	
	JS_FUNC_DECL(save)
	JS_FUNC_DECL(restore)
	// Transform
	JS_FUNC_DECL(setTransform)
	JS_FUNC_DECL(rotate)
	JS_FUNC_DECL(scale)
	JS_FUNC_DECL(translate)
	// Rendering
	JS_FUNC_DECL(drawImage)
	JS_FUNC_DECL(clearRect)
	JS_FUNC_DECL(fillRect)
	JS_FUNC_DECL(strokeRect)
	// Pixel Manipulate
	JS_FUNC_DECL(createImageData)
	JS_FUNC_DECL(getImageData)
	JS_FUNC_DECL(putImageData)
	// Path
	JS_FUNC_DECL(beginPath)
	JS_FUNC_DECL(closePath)
	JS_FUNC_DECL(fill)
	JS_FUNC_DECL(stroke)
	JS_FUNC_DECL(moveTo)
	JS_FUNC_DECL(lineTo)
	JS_FUNC_DECL(rect)
	JS_FUNC_DECL(bezierCurveTo)
	JS_FUNC_DECL(quadraticCurveTo)
	JS_FUNC_DECL(arc)
	JS_FUNC_DECL(arcTo)
	JS_FUNC_DECL(clip)
	// Font
	JS_PROPGET_DECL(font)
	JS_PROPSET_DECL(font)
	JS_PROPGET_DECL(textAlign)
	JS_PROPSET_DECL(textAlign)
	JS_PROPGET_DECL(textBaseline)
	JS_PROPSET_DECL(textBaseline)
	JS_FUNC_DECL(fillText)
	JS_FUNC_DECL(strokeText)
	JS_FUNC_DECL(measureText)
	// Pattern and Gradient
	JS_FUNC_DECL(createPattern)
	JS_FUNC_DECL(createLinearGradient)
	JS_FUNC_DECL(createRadialGradient)
	// Rekka Extra
	JS_FUNC_DECL(tintImage)	
public:	
	JSObject* createObject(JSContext *ctx);
	int getType() { return kContextType2D; }
private:
	inline void checkTarget();
	void fillRect(float x, float y, float w, float h, const SDL_Color& color);
	void clearRect(float x, float y, float w, float h);
	void restoreState();
private:
	Context2DState* _state;
	Path _path;
	std::vector<Context2DState> _stateStack;
};

NS_REK_END
