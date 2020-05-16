#include "path.h"
#include "render/extra.h"

NS_REK_BEGIN

#define PATH_RECURSION_LIMIT 8
#define PATH_DISTANCE_EPSILON 1.0f
#define PATH_COLLINEARITY_EPSILON 1.192092896e-07F // FLT_EPSILON
#define PATH_STEPS_FOR_CIRCLE 48.0f

void Path::beginPath()
{
	_hasStartPoint = false;
	_paths.clear();
	_currentPath.clear();
}

void Path::closePath()
{	
	// 以closePath方式结束时, 新subpath的起始点为前subpath的起始点
	if (_hasStartPoint) {
		_currentPoint = _startPoint;
		_currentPath.isClosed = true;		
	}
	done();
}

void Path::done()
{
	// 只有0或1个点则放弃
	if (_currentPath.points.size() > 1) {
		_paths.push_back(_currentPath);
	}
	_currentPath.clear();	
}

void Path::setStartPoint(const GPU_Point & point)
{
	_startPoint = point;
	_hasStartPoint = true;
}

void Path::moveTo(float x, float y, const AffineTransform * transform)
{
	done();
	_currentPoint = gpu::PointApplyAffineTransform(x, y, transform);
	setStartPoint(_currentPoint);
	push(_currentPoint);
}

void Path::lineTo(float x, float y, const AffineTransform * transform)
{
	_currentPoint = gpu::PointApplyAffineTransform(x, y, transform);	
	if (_currentPath.points.empty()) setStartPoint(_currentPoint);
	push(_currentPoint);	
}

void Path::bezierCurveTo(float cpx1, float cpy1, float cpx2, float cpy2, float x, float y, const AffineTransform * transform)
{
	const auto& p = gpu::PointApplyAffineTransform(x, y, transform);
	if (_currentPath.points.empty()) {
		setStartPoint(p);
		push(p);
		_currentPoint = p;
	}
	else {
		float scale = getAffineTransformScale(transform);
		float distanceTolerance = PATH_DISTANCE_EPSILON / scale;
		distanceTolerance *= distanceTolerance;
		const auto& cp1 = gpu::PointApplyAffineTransform(cpx1, cpy1, transform);
		const auto& cp2 = gpu::PointApplyAffineTransform(cpx2, cpy2, transform);
		recursiveBezier(_currentPoint.x, _currentPoint.y, cp1.x, cp1.y, cp2.x, cp2.y, p.x, p.y, 0, distanceTolerance);
		push(p);
		_currentPoint = p;
	}
}

void Path::recursiveBezier(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int level, float distanceTolerance)
{
	// Based on http://www.antigrain.com/research/adaptive_bezier/index.html
	// Calculate all the mid-points of the line segments
	float x12 = (x1 + x2) / 2;
	float y12 = (y1 + y2) / 2;
	float x23 = (x2 + x3) / 2;
	float y23 = (y2 + y3) / 2;
	float x34 = (x3 + x4) / 2;
	float y34 = (y3 + y4) / 2;
	float x123 = (x12 + x23) / 2;
	float y123 = (y12 + y23) / 2;
	float x234 = (x23 + x34) / 2;
	float y234 = (y23 + y34) / 2;
	float x1234 = (x123 + x234) / 2;
	float y1234 = (y123 + y234) / 2;
	if (level > 0) {
		// Enforce subdivision first time
		// Try to approximate the full cubic curve by a single straight line
		float dx = x4 - x1;
		float dy = y4 - y1;
		float d2 = fabsf(((x2 - x4) * dy - (y2 - y4) * dx));
		float d3 = fabsf(((x3 - x4) * dy - (y3 - y4) * dx));
		if (d2 > PATH_COLLINEARITY_EPSILON && d3 > PATH_COLLINEARITY_EPSILON) {
			// Regular care
			if ((d2 + d3)*(d2 + d3) <= distanceTolerance * (dx*dx + dy*dy)) {
				// If the curvature doesn't exceed the distance_tolerance value
				// we tend to finish subdivisions.
				push(x1234, y1234);
				return;
			}
		}
		else {
			if (d2 > PATH_COLLINEARITY_EPSILON) {
				// p1,p3,p4 are collinear, p2 is considerable
				if (d2 * d2 <= distanceTolerance * (dx*dx + dy*dy)) {
					push(x1234, y1234);
					return;
				}
			}
			else if (d3 > PATH_COLLINEARITY_EPSILON) {
				// p1,p2,p4 are collinear, p3 is considerable
				if (d3 * d3 <= distanceTolerance * (dx*dx + dy*dy)) {
					push(x1234, y1234);					
					return;
				}
			}
			else {
				// Collinear case
				dx = x1234 - (x1 + x4) / 2;
				dy = y1234 - (y1 + y4) / 2;
				if (dx*dx + dy*dy <= distanceTolerance) {
					push(x1234, y1234);					
					return;
				}
			}
		}
	}
	if (level <= PATH_RECURSION_LIMIT) {
		// Continue subdivision
		recursiveBezier(x1, y1, x12, y12, x123, y123, x1234, y1234, level + 1, distanceTolerance);
		recursiveBezier(x1234, y1234, x234, y234, x34, y34, x4, y4, level + 1, distanceTolerance);
	}
}

void Path::quadraticCurveTo(float cpx, float cpy, float x, float y, const AffineTransform * transform)
{
	const auto& p = gpu::PointApplyAffineTransform(x, y, transform);
	if (_currentPath.points.empty()) {
		setStartPoint(p);
		push(p);		
		_currentPoint = p;
	}
	else {
		float scale = getAffineTransformScale(transform);
		float distanceTolerance = PATH_DISTANCE_EPSILON / scale;
		distanceTolerance *= distanceTolerance;
		const auto& cp = gpu::PointApplyAffineTransform(cpx, cpy, transform);
		recursiveQuadratic(_currentPoint.x, _currentPoint.y, cp.x, cp.y, p.x, p.y, 0, distanceTolerance);
		push(p);
		_currentPoint = p;
	}
}

void Path::recursiveQuadratic(float x1, float y1, float x2, float y2, float x3, float y3, int level, float distanceTolerance)
{
	// Based on http://www.antigrain.com/research/adaptive_bezier/index.html
	// Calculate all the mid-points of the line segments
	float x12 = (x1 + x2) / 2;
	float y12 = (y1 + y2) / 2;
	float x23 = (x2 + x3) / 2;
	float y23 = (y2 + y3) / 2;
	float x123 = (x12 + x23) / 2;
	float y123 = (y12 + y23) / 2;
	float dx = x3 - x1;
	float dy = y3 - y1;
	float d = fabsf(((x2 - x3) * dy - (y2 - y3) * dx));
	if (d > PATH_COLLINEARITY_EPSILON) {
		// Regular care
		if (d * d <= distanceTolerance * (dx*dx + dy*dy)) {
			push(x123, y123);
			return;
		}
	}
	else {
		// Collinear case
		dx = x123 - (x1 + x3) / 2;
		dy = y123 - (y1 + y3) / 2;
		if (dx*dx + dy*dy <= distanceTolerance) {
			push(x123, y123);
			return;
		}
	}
	if (level <= PATH_RECURSION_LIMIT) {
		// Continue subdivision
		recursiveQuadratic(x1, y1, x12, y12, x123, y123, level + 1, distanceTolerance);
		recursiveQuadratic(x123, y123, x23, y23, x3, y3, level + 1, distanceTolerance);
	}
}

void Path::arcTo(float x1, float y1, float x2, float y2, float radius, const AffineTransform * transform)
{
	if (_currentPath.points.empty()) {
		const auto& p = gpu::PointApplyAffineTransform(x2, y2, transform);
		setStartPoint(p);
		push(p);
		_currentPoint = p;
		return;
	}
	// Lifted from http://code.google.com/p/fxcanvas/
	// get untransformed current point	
	const auto& it = invertAffineTransform(transform);
	const auto& cp = gpu::PointApplyAffineTransform(_currentPoint.x, _currentPoint.y, &it);
	float a1 = cp.y - y1;
	float b1 = cp.x - x1;
	float a2 = y2 - y1;
	float b2 = x2 - x1;
	float mm = fabsf(a1 * b2 - b1 * a2);
	if (mm < 1.0e-8 || radius == 0) {
		lineTo(x1, y1, transform);
	}
	else {
		float dd = a1 * a1 + b1 * b1;
		float cc = a2 * a2 + b2 * b2;
		float tt = a1 * a2 + b1 * b2;
		float k1 = radius * sqrtf(dd) / mm;
		float k2 = radius * sqrtf(cc) / mm;
		float j1 = k1 * tt / dd;
		float j2 = k2 * tt / cc;
		float ctx = k1 * b2 + k2 * b1;
		float cy = k1 * a2 + k2 * a1;
		float px = b1 * (k2 + j1);
		float py = a1 * (k2 + j1);
		float qx = b2 * (k1 + j2);
		float qy = a2 * (k1 + j2);
		float startAngle = atan2f(py - cy, px - ctx);
		float endAngle = atan2f(qy - cy, qx - ctx);
		arc(ctx + x1, cy + y1, radius, startAngle, endAngle, (b1 * a2 > b2 * a1), transform);
	}
}

void Path::arc(float x, float y, float radius, float startAngle, float endAngle, bool antiClockwise, const AffineTransform * transform)
{
	startAngle = fmodf(startAngle, 2 * M_PI);
	endAngle = fmodf(endAngle, 2 * M_PI);
	if (!antiClockwise && endAngle <= startAngle) {
		endAngle += 2 * M_PI;
	}
	else if (antiClockwise && startAngle <= endAngle) {
		startAngle += 2 * M_PI;
	}
	float span = endAngle - startAngle;
	int steps = (int)ceil(fabsf(span) * (PATH_STEPS_FOR_CIRCLE / (2 * M_PI)));
	float stepSize = span / steps;
	float angle = startAngle;	 
	for (int i = 0; i <= steps; i++, angle += stepSize) {
		const GPU_Point& p = gpu::PointApplyAffineTransform(x + cosf(angle) * radius, y + sinf(angle) * radius, transform);
		if (i == 0 && _currentPath.points.empty()) setStartPoint(p);
		push(p);
	}
	_currentPoint = _currentPath.points.back();
}

void Path::subfill(gpu::Target * target, const subpath_t & path, const SDL_Color & color)
{
	if (path.points.size() < 2) return;
	gpu::PolygonFilled(target, path.points.size(), (float*)&path.points.front(), color);
}

void Path::subfill_pattern(gpu::Target * target, const subpath_t & path, Pattern* pattern, float texture_x, float texture_y)
{
	if (path.points.size() < 2) return;
	if (CanvasExtra::supportNPOTRepeat || pattern->_powerof2) {			
		gpu::PolygonTextureFilled(target, path.points.size(), (float*)&path.points.front(), pattern->_texture, texture_x, texture_y);
	}
	else {
		CanvasExtra::beginNPOTRepeat(target, pattern->_texture, texture_x, texture_y);
		gpu::PolygonTextureFilledNPOT(target, path.points.size(), (float*)&path.points.front(), pattern->_texture);
		CanvasExtra::endNPOTRepeat();
	}
}

void Path::subfill_gradient(gpu::Target * target, const subpath_t & path, void * gradient, int gradientType)
{
	if (path.points.size() < 2) return;
	gpu::PolygonColorFilled(target, path.points.size(), (float*)&path.points.front(),
		gradientType == 0 ? LinearGradient::colorFunc : RadialGradient::colorFunc, gradient);
}

void Path::substroke(gpu::Target * target, const subpath_t & path, const SDL_Color & color)
{
	auto& points = path.points;
	if (points.size() < 1) return;
	int num_lines = path.isClosed ? points.size() : points.size() - 1;	
	for (int i = 0; i < num_lines; ++i) {
		if (i == points.size() - 1) {
			gpu::Line(target, points[i].x, points[i].y, points[0].x, points[0].y, color);
		}
		else {
			gpu::Line(target, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, color);
		}
	}	
}

void Path::substroke_pattern(gpu::Target * target, const subpath_t & path, Pattern* pattern, float texture_x, float texture_y)
{
	auto& points = path.points;
	if (points.size() < 1) return;
	int num_lines = path.isClosed ? points.size() : points.size() - 1;
	if (CanvasExtra::supportNPOTRepeat || pattern->_powerof2) {
		for (int i = 0; i < num_lines; ++i) {
			if (i == points.size() - 1)
				gpu::TextureLine(target, points[i].x, points[i].y, points[0].x, points[0].y, pattern->_texture, texture_x, texture_y);			
			else 
				gpu::TextureLine(target, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, pattern->_texture, texture_x, texture_y);			
		}
	}
	else {
		CanvasExtra::beginNPOTRepeat(target, pattern->_texture, texture_x, texture_y);
		for (int i = 0; i < num_lines; ++i) {
			if (i == points.size() - 1)
				gpu::TextureLineNPOT(target, points[i].x, points[i].y, points[0].x, points[0].y, pattern->_texture);			
			else 
				gpu::TextureLineNPOT(target, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, pattern->_texture);			
		}		
		CanvasExtra::endNPOTRepeat();
	}
}

void Path::substroke_gradient(gpu::Target * target, const subpath_t & path, void * gradient, int gradientType)
{
	auto& points = path.points;
	if (points.size() < 1) return;
	int num_lines = path.isClosed ? points.size() : points.size() - 1;
	for (int i = 0; i < num_lines; ++i) {
		if (i == points.size() - 1) {
			gpu::ColorLine(target, points[i].x, points[i].y, points[0].x, points[0].y,
				gradientType == 0 ? LinearGradient::colorFunc : RadialGradient::colorFunc, gradient);
		}
		else {
			gpu::ColorLine(target, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y,
				gradientType == 0 ? LinearGradient::colorFunc : RadialGradient::colorFunc, gradient);
		}
	}
}

void Path::fill(gpu::Target * target, const SDL_Color & color)
{
	for (auto& path : _paths) {	
		subfill(target, path, color);
	}	
	subfill(target, _currentPath, color);
}

void Path::fill(gpu::Target * target, FillObject* filler, const AffineTransform* transform)
{
	Pattern* pattern = dynamic_cast<Pattern*>(filler);
	if (pattern) {		
		const auto& first_point = gpu::PointApplyAffineTransform(0, 0, transform);		
		for (auto& path : _paths) {
			subfill_pattern(target, path, pattern, first_point.x, first_point.y);
		}
		subfill_pattern(target, _currentPath, pattern, first_point.x, first_point.y);
		return;
	}
	LinearGradient* gradient = dynamic_cast<LinearGradient*>(filler);
	if (gradient) {
		for (auto& path : _paths) {
			subfill_gradient(target, path, gradient, 0);
		}
		subfill_gradient(target, _currentPath, gradient, 0);
		return;
	}
	RadialGradient* gradient1 = dynamic_cast<RadialGradient*>(filler);
	if (gradient1) {
		for (auto& path : _paths) {
			subfill_gradient(target, path, gradient1, 1);
		}
		subfill_gradient(target, _currentPath, gradient1, 1);
		return;
	}
	SDL_LogError(0, "Invalid fill object");
}

void Path::stroke(gpu::Target * target, const SDL_Color & color)
{
	for (auto& path : _paths) {
		substroke(target, path, color);
	}
	substroke(target, _currentPath, color);
}

void Path::stroke(gpu::Target * target, FillObject * filler, const AffineTransform* transform)
{
	Pattern* pattern = dynamic_cast<Pattern*>(filler);
	if (pattern) {		
		const auto& first_point = gpu::PointApplyAffineTransform(0, 0, transform);
		for (auto& path : _paths) {
			substroke_pattern(target, path, pattern, first_point.x, first_point.y);
		}
		substroke_pattern(target, _currentPath, pattern, first_point.x, first_point.y);
		return;
	}
	LinearGradient* gradient = dynamic_cast<LinearGradient*>(filler);
	if (gradient) {
		for (auto& path : _paths) {
			substroke_gradient(target, path, gradient, 0);
		}
		substroke_gradient(target, _currentPath, gradient, 0);
		return;
	}
	RadialGradient* gradient1 = dynamic_cast<RadialGradient*>(filler);
	if (gradient1) {
		for (auto& path : _paths) {
			substroke_gradient(target, path, gradient1, 1);
		}
		substroke_gradient(target, _currentPath, gradient1, 1);
		return;
	}
	SDL_LogError(0, "Invalid stroke object");
}

NS_REK_END