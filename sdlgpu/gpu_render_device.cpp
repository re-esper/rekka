#include <string.h>
#include "gpu_render_device.h"
#include "gpu_renderer.h"

NS_GPU_BEGIN

#if defined(XGPU_USE_GLES)
RenderDevice* CreateRenderer(DeviceID request)
{
	RenderDevice* renderer = new RenderDevice();

	renderer->id = request;
	renderer->id.renderer = RENDERER_GLES_2;
	renderer->shader_language = LANGUAGE_GLSLES;
	renderer->min_shader_version = 100;
	renderer->max_shader_version = XGPU_GLSL_VERSION;

	renderer->default_image_anchor_x = 0.5f;
	renderer->default_image_anchor_y = 0.5f;

	renderer->current_context_target = NULL;

	return renderer;
}
#endif

#if defined(XGPU_USE_OPENGL)
RenderDevice* CreateRenderer(DeviceID request)
{
	RenderDevice* renderer = new RenderDevice();

    renderer->id = request;
    renderer->id.renderer = RENDERER_OPENGL_2;
    renderer->shader_language = LANGUAGE_GLSL;
    renderer->min_shader_version = 110;
    renderer->max_shader_version = XGPU_GLSL_VERSION;
    
    renderer->default_image_anchor_x = 0.5f;
    renderer->default_image_anchor_y = 0.5f;
    
    renderer->current_context_target = NULL;
    
    return renderer;
}

#endif

void FreeRenderer(RenderDevice* renderer)
{
	if (renderer) delete renderer;
}

NS_GPU_END