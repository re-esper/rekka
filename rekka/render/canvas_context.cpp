#include "canvas_context.h"
#include "canvas.h"
#include "core.h"

NS_REK_BEGIN

CanvasContext::CanvasContext(Canvas* canvas)
: _owner(canvas), _isAntiAlias(false)
{
	_target = canvas->_target;
	_isOffscreen = _target != Core::getInstance()->_screen;
}

NS_REK_END