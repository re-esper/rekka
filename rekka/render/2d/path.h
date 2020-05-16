#pragma once

#include "rekka.h"
#include "fill_object.h"

NS_REK_BEGIN

class Path {
	struct subpath_t {
		std::vector<GPU_Point> points;
		bool isClosed;
		subpath_t() : isClosed(false) {}
		void clear() { points.clear(); isClosed = false; }		
	};
public:
	Path() : _hasStartPoint(false) {}

	void beginPath(); // 路径清零
	void closePath(); // 导致subpath结束	
	void moveTo(float x, float y, const AffineTransform* transform); // 导致subpath结束
	void lineTo(float x, float y, const AffineTransform* transform);
	void bezierCurveTo(float cpx1, float cpy1, float cpx2, float cpy2,
		float x, float y, const AffineTransform* transform);	
	void quadraticCurveTo(float cpx, float cpy, float x, float y, const AffineTransform* transform);
	void arcTo(float x1, float y1, float x2, float y2, float radius, const AffineTransform* transform);
	void arc(float x, float y, float radius, float startAngle, float endAngle,
		bool antiClockwise, const AffineTransform* transform);
	
	void fill(gpu::Target* target, const SDL_Color& color);
	void fill(gpu::Target* target, FillObject* filler, const AffineTransform* transform);
	void stroke(gpu::Target* target, const SDL_Color& color);
	void stroke(gpu::Target* target, FillObject* filler, const AffineTransform* transform);
private:	
	void recursiveBezier(float x1, float y1, float x2, float y2, float x3,
		float y3, float x4, float y4, int level, float distanceTolerance);
	void recursiveQuadratic(float x1, float y1, float x2, float y2, float x3,
		float y3, int level, float distanceTolerance);	
	void done();
	void setStartPoint(const GPU_Point& point);
	void push(float x, float y) {
		GPU_Point point = { x, y };
		_currentPath.points.push_back(point);
	}
	void push(const GPU_Point& point) {
		_currentPath.points.push_back(point);
	}
	float getAffineTransformScale(const AffineTransform* t) { return sqrtf(t->a * t->a + t->c * t->c); }
	AffineTransform invertAffineTransform(const AffineTransform* t) {
		float determinant = 1 / (t->a * t->d - t->b * t->c);
		AffineTransform it = { determinant * t->d, -determinant * t->b, -determinant * t->c, determinant * t->a,
			determinant * (t->c * t->ty - t->d * t->tx), determinant * (t->b * t->tx - t->a * t->ty)};
		return it;
	}
	void subfill(gpu::Target* target, const subpath_t& path, const SDL_Color& color);
	void subfill_pattern(gpu::Target* target, const subpath_t& path, Pattern* pattern, float texture_x, float texture_y);
	void subfill_gradient(gpu::Target* target, const subpath_t& path, void* gradient, int gradientType);
	void substroke(gpu::Target* target, const subpath_t& path, const SDL_Color& color);
	void substroke_pattern(gpu::Target* target, const subpath_t& path, Pattern* pattern, float texture_x, float texture_y);
	void substroke_gradient(gpu::Target* target, const subpath_t& path, void* gradient, int gradientType);
private:	
	std::vector<subpath_t> _paths;
	subpath_t _currentPath;
	GPU_Point _currentPoint;
	GPU_Point _startPoint;
	bool _hasStartPoint;
};

NS_REK_END