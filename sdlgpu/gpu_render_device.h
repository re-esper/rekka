#ifndef _XGPU_DEVICE_H
#define _XGPU_DEVICE_H

#include "xgpu.h"
#include "SDL_platform.h"

#if !defined(XGPU_DISABLE_GLES)
#ifdef __IPHONEOS__
	#include <OpenGLES/ES2/gl.h>
	#include <OpenGLES/ES2/glext.h>
	#define XGPU_USE_BUFFER_RESET
#else
	#include "GLES2/gl2.h"
	#include "GLES2/gl2ext.h"
#endif
	#define glVertexAttribI1i glVertexAttrib1f
	#define glVertexAttribI2i glVertexAttrib2f
	#define glVertexAttribI3i glVertexAttrib3f
	#define glVertexAttribI4i glVertexAttrib4f
	#define glVertexAttribI1ui glVertexAttrib1f
	#define glVertexAttribI2ui glVertexAttrib2f
	#define glVertexAttribI3ui glVertexAttrib3f
	#define glVertexAttribI4ui glVertexAttrib4f
	#define glMapBuffer glMapBufferOES
	#define glUnmapBuffer glUnmapBufferOES
	#define GL_WRITE_ONLY GL_WRITE_ONLY_OES

	#define XGPU_USE_GLES
	#define XGPU_GLES_MAJOR_VERSION 2
	#define XGPU_GLSL_VERSION 100
	#define XGPU_SKIP_ENABLE_TEXTURE_2D
	#define XGPU_DISABLE_TEXTURE_GETS

	#include "gpu_shader_gles2.inc"
#endif

#if !defined(XGPU_DISABLE_OPENGL)
    // Hacks to fix compile errors due to polluted namespace
#ifdef _WIN32
    #define _WINUSER_H
    #define _WINGDI_H
#endif
    #include "glew.h"	
	#if defined(GL_EXT_bgr) && !defined(GL_BGR)
		#define GL_BGR GL_BGR_EXT
	#endif
	#if defined(GL_EXT_bgra) && !defined(GL_BGRA)
		#define GL_BGRA GL_BGRA_EXT
	#endif
	#if defined(GL_EXT_abgr) && !defined(GL_ABGR)
		#define GL_ABGR GL_ABGR_EXT
	#endif		
	#define XGPU_USE_OPENGL
	#define XGPU_GLSL_VERSION 120
	#define XGPU_GL_MAJOR_VERSION 2

	#include "gpu_shader_opengl2.inc"
#endif

NS_GPU_BEGIN
typedef struct ContextData
{
	SDL_Color last_color;
	bool last_use_texturing;
	unsigned int last_shape;
	bool last_use_blending;
	BlendMode last_blend_mode;
	GPU_Rect last_viewport;
	Camera last_camera;
	bool last_camera_inverted;
	
	Image* last_image;
	Target* last_target;
	float* blit_buffer;  // Holds sets of 4 vertices and 4 tex coords interleaved (e.g. [x0, y0, z0, s0, t0, ...]).
	unsigned short blit_buffer_num_vertices;
	unsigned short blit_buffer_max_num_vertices;
	unsigned short* index_buffer;  // Indexes into the blit buffer so we can use 4 vertices for every 2 triangles (1 quad)
	unsigned int index_buffer_num_vertices;
	unsigned int index_buffer_max_num_vertices;
	    
    unsigned int blit_VBO[2];  // For double-buffering
    unsigned int blit_IBO;
    bool blit_VBO_flop;
    
	AttributeSource shader_attributes[16];
	unsigned int attribute_VBO[16];
} ContextData;

typedef struct ImageData
{
    int refcount;
    bool owns_handle;
	Uint32 handle;
	Uint32 format;
} ImageData;

typedef struct TargetData
{
    int refcount;
	Uint32 handle;
	Uint32 format;
} TargetData;

/* Renderer object which specializes the API to a particular backend. */
class RenderDevice {
public:
	/* Struct identifier of the renderer. */
	DeviceID id;
	DeviceID requested_id;
	WindowFlagEnum SDL_init_flags;
	InitFlagEnum init_flags;

	ShaderLanguageEnum shader_language;
	int min_shader_version;
	int max_shader_version;
	FeatureEnum enabled_features;

	/* Current display target */
	Target* current_context_target;

	/* Default is (0.5, 0.5) - images draw centered. */
	float default_image_anchor_x;
	float default_image_anchor_y;
};


RenderDevice* CreateRenderer(DeviceID request);
void FreeRenderer(RenderDevice* renderer);

NS_GPU_END

#endif
