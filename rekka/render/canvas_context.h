#pragma once

#include "rekka.h"

NS_REK_BEGIN

typedef enum {
	kContextTypeNone = 0,
	kContextType2D,
	kContextTypeWebGL
} CanvasContextType;

class Canvas;
class CanvasContext {	
public:
	friend class Canvas;	
	CanvasContext(Canvas* canvas);
	virtual ~CanvasContext() {};
	virtual int getType() = 0;
	virtual JSObject* createObject(JSContext *ctx) = 0;
protected:
	bool _isOffscreen;
	bool _isAntiAlias;
	Canvas* _owner;
	gpu::Target* _target;
};

NS_REK_END