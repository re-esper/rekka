#include <stdlib.h>
#include "SDL_platform.h"
#include <math.h>
#include <string.h>
#include "gpu_renderer.h"
#include "gpu_render_device.h"

//#define DEFAULT_BLEND_MODE BLEND_NORMAL
#define DEFAULT_BLEND_MODE BLEND_PREMULTIPLIED_ALPHA // specific for html5 canvas
// perform premultipy alpha
#if DEFAULT_BLEND_MODE == BLEND_PREMULTIPLIED_ALPHA
	#define PREMULTIPLIED_ALPHA
#endif

// Check for C99 support
// We'll use it for intptr_t which is used to suppress warnings about converting an int to a ptr for GL calls.
#if __STDC_VERSION__ >= 199901L
    #include <stdint.h>
#else
    #define intptr_t long
#endif

#include "stb_image.h"
#include "stb_image_write.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Visual C does not support static inline
#ifndef static_inline
    #ifdef _MSC_VER
		#define static_inline static
    #else
        #define static_inline static inline
    #endif
#endif

#if defined ( WIN32 )
#define __func__ __FUNCTION__
#endif


NS_GPU_BEGIN

// Forces a flush when vertex limit is reached (roughly 1000 sprites)
#define BLIT_BUFFER_VERTICES_PER_SPRITE 4
#define BLIT_BUFFER_INIT_MAX_NUM_VERTICES (BLIT_BUFFER_VERTICES_PER_SPRITE*1000)


// Near the unsigned short limit (65535)
#define BLIT_BUFFER_ABSOLUTE_MAX_VERTICES 60000
// Near the unsigned int limit (4294967295)
#define INDEX_BUFFER_ABSOLUTE_MAX_VERTICES 4000000000u


// x, y, s, t, r, g, b, a
#define BLIT_BUFFER_FLOATS_PER_VERTEX 8

// bytes per vertex
#define BLIT_BUFFER_STRIDE (sizeof(float)*BLIT_BUFFER_FLOATS_PER_VERTEX)
#define BLIT_BUFFER_VERTEX_OFFSET 0
#define BLIT_BUFFER_TEX_COORD_OFFSET 2
#define BLIT_BUFFER_COLOR_OFFSET 4


static_inline SDL_Window* get_window(Uint32 windowID)
{
    return SDL_GetWindowFromID(windowID);
}

static_inline Uint32 get_window_id(SDL_Window* window)
{
    return SDL_GetWindowID(window);
}

static_inline void get_window_dimensions(SDL_Window* window, int* w, int* h)
{
    SDL_GetWindowSize(window, w, h);
}

static_inline void get_drawable_dimensions(SDL_Window* window, int* w, int* h)
{
    SDL_GL_GetDrawableSize(window, w, h);
}

static_inline void resize_window(Target* target, int w, int h)
{
    SDL_SetWindowSize(SDL_GetWindowFromID(target->context->windowID), w, h);
}

static_inline bool get_fullscreen_state(SDL_Window* window)
{
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
}

static_inline bool has_colorkey(SDL_Surface* surface)
{
    return (SDL_GetColorKey(surface, NULL) == 0);
}

static_inline void get_target_window_dimensions(Target* target, int* w, int* h)
{
    SDL_Window* window;
    if(target == NULL || target->context == NULL)
        return;
    window = get_window(target->context->windowID);
    get_window_dimensions(window, w, h);
}

static_inline void get_target_drawable_dimensions(Target* target, int* w, int* h)
{
    SDL_Window* window;
    if(target == NULL || target->context == NULL)
        return;
    window = get_window(target->context->windowID);
    get_drawable_dimensions(window, w, h);
}


// Workaround for Intel HD glVertexAttrib() bug.
#ifdef XGPU_USE_OPENGL
// FIXME: This should probably exist in context storage, as I expect it to be a problem across contexts.
static bool apply_Intel_attrib_workaround = false;
static bool vendor_is_Intel = false;
#endif


static char shader_message[256];

bool Renderer::IsExtensionSupported(const char* extension_str)
{
#ifdef XGPU_USE_OPENGL
    return glewIsExtensionSupported(extension_str);
#else
    // As suggested by Mesa3D.org
    char* p = (char*)glGetString(GL_EXTENSIONS);
    char* end;
    unsigned long extNameLen;
    
    if(p == NULL)
        return false;
    
    extNameLen = strlen(extension_str);
    end = p + strlen(p);

    while(p < end)
    {
        unsigned long n = strcspn(p, " ");
        if((extNameLen == n) && (strncmp(extension_str, p, n) == 0))
            return true;

        p += (n + 1);
    }
    return false;
#endif
}

void Renderer::init_features()
{
    // Reset supported features
    _device->enabled_features = 0;

    // NPOT textures
#ifdef XGPU_USE_OPENGL
    #if XGPU_GL_MAJOR_VERSION >= 2
        // Core in GL 2+
        _device->enabled_features |= FEATURE_NON_POWER_OF_TWO;
    #else
        if(IsExtensionSupported("GL_ARB_texture_non_power_of_two"))
            _device->enabled_features |= FEATURE_NON_POWER_OF_TWO;
        else
            _device->enabled_features &= ~FEATURE_NON_POWER_OF_TWO;
    #endif
#elif defined(XGPU_USE_GLES)
    #if XGPU_GLES_MAJOR_VERSION >= 3
        // Core in GLES 3+
        _device->enabled_features |= FEATURE_NON_POWER_OF_TWO;
    #else
        if(IsExtensionSupported("GL_OES_texture_npot") || IsExtensionSupported("GL_IMG_texture_npot")
           || IsExtensionSupported("GL_APPLE_texture_2D_limited_npot") || IsExtensionSupported("GL_ARB_texture_non_power_of_two"))
            _device->enabled_features |= FEATURE_NON_POWER_OF_TWO;
        else
            _device->enabled_features &= ~FEATURE_NON_POWER_OF_TWO;
            
        #if XGPU_GLES_MAJOR_VERSION >= 2
        // Assume limited NPOT support for GLES 2+
            _device->enabled_features |= FEATURE_NON_POWER_OF_TWO;
        #endif
    #endif
#endif

    // FBO
#ifdef XGPU_USE_OPENGL
    #if XGPU_GL_MAJOR_VERSION >= 3
        // Core in GL 3+
        _device->enabled_features |= FEATURE_RENDER_TARGETS;
    #else
        if (IsExtensionSupported("GL_EXT_framebuffer_object"))
            _device->enabled_features |= FEATURE_RENDER_TARGETS;
        else
            _device->enabled_features &= ~FEATURE_RENDER_TARGETS;
    #endif
#elif defined(XGPU_USE_GLES)
    #if XGPU_GLES_MAJOR_VERSION >= 2
        // Core in GLES 2+
        _device->enabled_features |= FEATURE_RENDER_TARGETS;
    #else
        if(IsExtensionSupported("GL_OES_framebuffer_object"))
            _device->enabled_features |= FEATURE_RENDER_TARGETS;
        else
            _device->enabled_features &= ~FEATURE_RENDER_TARGETS;
    #endif
#endif

    // Blending
#ifdef XGPU_USE_OPENGL
    _device->enabled_features |= FEATURE_BLEND_EQUATIONS;
    _device->enabled_features |= FEATURE_BLEND_FUNC_SEPARATE;

    #if XGPU_GL_MAJOR_VERSION >= 2
        // Core in GL 2+
        _device->enabled_features |= FEATURE_BLEND_EQUATIONS_SEPARATE;
    #else
        if(IsExtensionSupported("GL_EXT_blend_equation_separate"))
            _device->enabled_features |= FEATURE_BLEND_EQUATIONS_SEPARATE;
        else
            _device->enabled_features &= ~FEATURE_BLEND_EQUATIONS_SEPARATE;
    #endif

#elif defined(XGPU_USE_GLES)

    #if XGPU_GLES_MAJOR_VERSION >= 2
        // Core in GLES 2+
        _device->enabled_features |= FEATURE_BLEND_EQUATIONS;
        _device->enabled_features |= FEATURE_BLEND_FUNC_SEPARATE;
        _device->enabled_features |= FEATURE_BLEND_EQUATIONS_SEPARATE;
    #else
        if(IsExtensionSupported("GL_OES_blend_subtract"))
            _device->enabled_features |= FEATURE_BLEND_EQUATIONS;
        else
            _device->enabled_features &= ~FEATURE_BLEND_EQUATIONS;

        if(IsExtensionSupported("GL_OES_blend_func_separate"))
            _device->enabled_features |= FEATURE_BLEND_FUNC_SEPARATE;
        else
            _device->enabled_features &= ~FEATURE_BLEND_FUNC_SEPARATE;

        if(IsExtensionSupported("GL_OES_blend_equation_separate"))
            _device->enabled_features |= FEATURE_BLEND_EQUATIONS_SEPARATE;
        else
            _device->enabled_features &= ~FEATURE_BLEND_EQUATIONS_SEPARATE;
    #endif
#endif

    // wrap modes
#ifdef XGPU_USE_OPENGL
    #if XGPU_GL_MAJOR_VERSION >= 2
        _device->enabled_features |= FEATURE_WRAP_REPEAT_MIRRORED;
    #else
        if(IsExtensionSupported("GL_ARB_texture_mirrored_repeat"))
            _device->enabled_features |= FEATURE_WRAP_REPEAT_MIRRORED;
        else
            _device->enabled_features &= ~FEATURE_WRAP_REPEAT_MIRRORED;
    #endif
#elif defined(XGPU_USE_GLES)
    #if XGPU_GLES_MAJOR_VERSION >= 2
        _device->enabled_features |= FEATURE_WRAP_REPEAT_MIRRORED;
    #else
        if(IsExtensionSupported("GL_OES_texture_mirrored_repeat"))
            _device->enabled_features |= FEATURE_WRAP_REPEAT_MIRRORED;
        else
            _device->enabled_features &= ~FEATURE_WRAP_REPEAT_MIRRORED;
    #endif
#endif

    // GL texture formats
    if(IsExtensionSupported("GL_EXT_bgr"))
        _device->enabled_features |= FEATURE_GL_BGR;
    if(IsExtensionSupported("GL_EXT_bgra"))
        _device->enabled_features |= FEATURE_GL_BGRA;
    if(IsExtensionSupported("GL_EXT_abgr"))
        _device->enabled_features |= FEATURE_GL_ABGR;

	// Disable other texture formats for GLES.
	// TODO: Add better (static) checking for format support.  Some GL versions do not report previously non-core features as extensions.
	#ifdef XGPU_USE_GLES
		_device->enabled_features &= ~FEATURE_GL_BGR;
		_device->enabled_features &= ~FEATURE_GL_BGRA;
		_device->enabled_features &= ~FEATURE_GL_ABGR;
	#endif

    // Shader support    
    if(IsExtensionSupported("GL_ARB_fragment_shader"))
        _device->enabled_features |= FEATURE_FRAGMENT_SHADER;
    if(IsExtensionSupported("GL_ARB_vertex_shader"))
        _device->enabled_features |= FEATURE_VERTEX_SHADER;
    if(IsExtensionSupported("GL_ARB_geometry_shader4"))
        _device->enabled_features |= FEATURE_GEOMETRY_SHADER;    
    
    _device->enabled_features |= FEATURE_BASIC_SHADERS;    
}

void Renderer::extBindFramebuffer(GLuint handle)
{
    if(_device->enabled_features & FEATURE_RENDER_TARGETS)
        glBindFramebuffer(GL_FRAMEBUFFER, handle);
}


static_inline bool isPowerOfTwo(unsigned int x)
{
    return ((x != 0) && !(x & (x - 1)));
}

static_inline unsigned int getNearestPowerOf2(unsigned int n)
{
    unsigned int x = 1;
    while(x < n)
    {
        x <<= 1;
    }
    return x;
}

void Renderer::bindTexture(Image* image)
{
    // Bind the texture to which subsequent calls refer
    if(image != ((ContextData*)_device->current_context_target->context->data)->last_image)
    {
        GLuint handle = ((ImageData*)image->data)->handle;
        FlushBlitBuffer();

        glBindTexture( GL_TEXTURE_2D, handle );
        ((ContextData*)_device->current_context_target->context->data)->last_image = image;
    }
}

inline void Renderer::flushAndBindTexture(GLuint handle)
{
    // Bind the texture to which subsequent calls refer
    FlushBlitBuffer();

    glBindTexture( GL_TEXTURE_2D, handle );
    ((ContextData*)_device->current_context_target->context->data)->last_image = NULL;
}

// Returns false if it can't be bound
bool Renderer::bindFramebuffer(Target* target)
{
    if(_device->enabled_features & FEATURE_RENDER_TARGETS)
    {
        // Bind the FBO
        if(target != ((ContextData*)_device->current_context_target->context->data)->last_target)
        {
            GLuint handle = 0;
            if(target != NULL)
                handle = ((TargetData*)target->data)->handle;
            FlushBlitBuffer();

            extBindFramebuffer(handle);
            ((ContextData*)_device->current_context_target->context->data)->last_target = target;
        }
        return true;
    }
    else
    {
        // There's only one possible render target, the default framebuffer.
        // Note: Could check against the default framebuffer value (((TargetData*)target->data)->handle versus result of GL_FRAMEBUFFER_BINDING)...
        if(target != NULL)
        {
            ((ContextData*)_device->current_context_target->context->data)->last_target = target;
            return true;
        }
        return false;
    }
}

inline void Renderer::flushAndBindFramebuffer(GLuint handle)
{
    // Bind the FBO
    FlushBlitBuffer();

    extBindFramebuffer(handle);
    ((ContextData*)_device->current_context_target->context->data)->last_target = NULL;
}

inline void Renderer::flushBlitBufferIfCurrentTexture(Image* image)
{
    if(image == ((ContextData*)_device->current_context_target->context->data)->last_image)
    {
        FlushBlitBuffer();
    }
}

inline void Renderer::flushAndClearBlitBufferIfCurrentTexture(Image* image)
{
    if(image == ((ContextData*)_device->current_context_target->context->data)->last_image)
    {
        FlushBlitBuffer();
        ((ContextData*)_device->current_context_target->context->data)->last_image = NULL;
    }
}

inline bool Renderer::isCurrentTarget(Target* target)
{
    return (target == ((ContextData*)_device->current_context_target->context->data)->last_target
            || ((ContextData*)_device->current_context_target->context->data)->last_target == NULL);
}

inline void Renderer::flushAndClearBlitBufferIfCurrentFramebuffer(Target* target)
{
    if(target == ((ContextData*)_device->current_context_target->context->data)->last_target
            || ((ContextData*)_device->current_context_target->context->data)->last_target == NULL)
    {
        FlushBlitBuffer();
        ((ContextData*)_device->current_context_target->context->data)->last_target = NULL;
    }
}

static bool growBlitBuffer(ContextData* cdata, unsigned int minimum_vertices_needed)
{
	unsigned int new_max_num_vertices;
	float* new_buffer;

    if(minimum_vertices_needed <= cdata->blit_buffer_max_num_vertices)
        return true;
    if(cdata->blit_buffer_max_num_vertices == BLIT_BUFFER_ABSOLUTE_MAX_VERTICES)
        return false;

    // Calculate new size (in vertices)
    new_max_num_vertices = ((unsigned int)cdata->blit_buffer_max_num_vertices) * 2;
    while(new_max_num_vertices <= minimum_vertices_needed)
        new_max_num_vertices *= 2;

    if(new_max_num_vertices > BLIT_BUFFER_ABSOLUTE_MAX_VERTICES)
        new_max_num_vertices = BLIT_BUFFER_ABSOLUTE_MAX_VERTICES;

    //LogError("Growing to %d vertices\n", new_max_num_vertices);
    // Resize the blit buffer
    new_buffer = (float*)SDL_malloc(new_max_num_vertices * BLIT_BUFFER_STRIDE);
    memcpy(new_buffer, cdata->blit_buffer, cdata->blit_buffer_num_vertices * BLIT_BUFFER_STRIDE);
    SDL_free(cdata->blit_buffer);
    cdata->blit_buffer = new_buffer;
    cdata->blit_buffer_max_num_vertices = new_max_num_vertices;

	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, BLIT_BUFFER_STRIDE * cdata->blit_buffer_max_num_vertices, NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, BLIT_BUFFER_STRIDE * cdata->blit_buffer_max_num_vertices, NULL, GL_STREAM_DRAW);

    return true;
}

static bool growIndexBuffer(ContextData* cdata, unsigned int minimum_vertices_needed)
{
	unsigned int new_max_num_vertices;
	unsigned short* new_indices;

    if(minimum_vertices_needed <= cdata->index_buffer_max_num_vertices)
        return true;
    if(cdata->index_buffer_max_num_vertices == INDEX_BUFFER_ABSOLUTE_MAX_VERTICES)
        return false;

    // Calculate new size (in vertices)
    new_max_num_vertices = cdata->index_buffer_max_num_vertices * 2;
    while(new_max_num_vertices <= minimum_vertices_needed)
        new_max_num_vertices *= 2;

    if(new_max_num_vertices > INDEX_BUFFER_ABSOLUTE_MAX_VERTICES)
        new_max_num_vertices = INDEX_BUFFER_ABSOLUTE_MAX_VERTICES;

    //LogError("Growing to %d indices\n", new_max_num_vertices);
    // Resize the index buffer
    new_indices = (unsigned short*)SDL_malloc(new_max_num_vertices * sizeof(unsigned short));
    memcpy(new_indices, cdata->index_buffer, cdata->index_buffer_num_vertices * sizeof(unsigned short));
    SDL_free(cdata->index_buffer);
    cdata->index_buffer = new_indices;
    cdata->index_buffer_max_num_vertices = new_max_num_vertices;


	// Resize the IBO
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cdata->blit_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * cdata->index_buffer_max_num_vertices, NULL, GL_DYNAMIC_DRAW);

    return true;
}


// Only for window targets, which have their own contexts.
void Renderer::makeContextCurrent(Target* target)
{
    if(target == NULL || target->context == NULL || _device->current_context_target == target)
        return;

    FlushBlitBuffer();
    
    SDL_GL_MakeCurrent(SDL_GetWindowFromID(target->context->windowID), target->context->context);
    _device->current_context_target = target;
}

void Renderer::setClipRect(Target* target)
{
    if(target->use_clip_rect)
    {
        Target* context_target = _device->current_context_target;
        glEnable(GL_SCISSOR_TEST);
        if(target->context != NULL)
        {
            int y = context_target->h - (target->clip_rect.y + target->clip_rect.h);            
            float xFactor = ((float)context_target->context->drawable_w)/context_target->w;
            float yFactor = ((float)context_target->context->drawable_h)/context_target->h;
            glScissor(target->clip_rect.x * xFactor, y * yFactor, target->clip_rect.w * xFactor, target->clip_rect.h * yFactor);
        }
        else
            glScissor(target->clip_rect.x, target->clip_rect.y, target->clip_rect.w, target->clip_rect.h);
    }
}

static void unsetClipRect(Target* target)
{
    if(target->use_clip_rect)
        glDisable(GL_SCISSOR_TEST);
}

void Renderer::prepareToRenderToTarget(Target* target)
{
    // Set up the camera
	SetCamera(target, &target->camera);
}

void Renderer::changeBlending(bool enable)
{
    ContextData* cdata = (ContextData*)_device->current_context_target->context->data;
    if(cdata->last_use_blending == enable)
        return;

    FlushBlitBuffer();

    if(enable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    cdata->last_use_blending = enable;
}

void Renderer::forceChangeBlendMode(BlendMode mode)
{
    ContextData* cdata = (ContextData*)_device->current_context_target->context->data;

    FlushBlitBuffer();

    cdata->last_blend_mode = mode;

    if(mode.source_color == mode.source_alpha && mode.dest_color == mode.dest_alpha)
    {
        glBlendFunc(mode.source_color, mode.dest_color);
    }
    else if(_device->enabled_features & FEATURE_BLEND_FUNC_SEPARATE)
    {
        glBlendFuncSeparate(mode.source_color, mode.dest_color, mode.source_alpha, mode.dest_alpha);
    }
    else
    {
        PushErrorCode("(SDL_gpu internal)", ERROR_BACKEND_ERROR, "Could not set blend function because FEATURE_BLEND_FUNC_SEPARATE is not supported.");
    }

    if(_device->enabled_features & FEATURE_BLEND_EQUATIONS)
    {
        if(mode.color_equation == mode.alpha_equation)
            glBlendEquation(mode.color_equation);
        else if(_device->enabled_features & FEATURE_BLEND_EQUATIONS_SEPARATE)
            glBlendEquationSeparate(mode.color_equation, mode.alpha_equation);
        else
        {
            PushErrorCode("(SDL_gpu internal)", ERROR_BACKEND_ERROR, "Could not set blend equation because FEATURE_BLEND_EQUATIONS_SEPARATE is not supported.");
        }
    }
    else
    {
        PushErrorCode("(SDL_gpu internal)", ERROR_BACKEND_ERROR, "Could not set blend equation because FEATURE_BLEND_EQUATIONS is not supported.");
    }
}

void Renderer::changeBlendMode(BlendMode mode)
{
    ContextData* cdata = (ContextData*)_device->current_context_target->context->data;
    if(cdata->last_blend_mode.source_color == mode.source_color
       && cdata->last_blend_mode.dest_color == mode.dest_color
       && cdata->last_blend_mode.source_alpha == mode.source_alpha
       && cdata->last_blend_mode.dest_alpha == mode.dest_alpha
       && cdata->last_blend_mode.color_equation == mode.color_equation
       && cdata->last_blend_mode.alpha_equation == mode.alpha_equation)
        return;

    forceChangeBlendMode(mode);
}


// If 0 is returned, there is no valid shader.
Uint32 Renderer::get_proper_program_id(Uint32 program_object)
{
    Context* context = _device->current_context_target->context;
    if(context->default_textured_shader_program == 0)  // No shaders loaded!
        return 0;

    if(program_object == 0)
        return context->default_textured_shader_program;

    return program_object;
}


static void applyTexturing(RenderDevice* device)
{
    Context* context = device->current_context_target->context;
    if(context->use_texturing != ((ContextData*)context->data)->last_use_texturing)
    {
        ((ContextData*)context->data)->last_use_texturing = context->use_texturing;
        #ifndef XGPU_SKIP_ENABLE_TEXTURE_2D
        if(context->use_texturing)
            glEnable(GL_TEXTURE_2D);
        else
            glDisable(GL_TEXTURE_2D);
        #endif
    }
}

void Renderer::changeTexturing(bool enable)
{
    Context* context = _device->current_context_target->context;
    if(enable != ((ContextData*)context->data)->last_use_texturing)
    {
        FlushBlitBuffer();

        ((ContextData*)context->data)->last_use_texturing = enable;
        #ifndef XGPU_SKIP_ENABLE_TEXTURE_2D
        if(enable)
            glEnable(GL_TEXTURE_2D);
        else
            glDisable(GL_TEXTURE_2D);
        #endif
    }
}

void Renderer::enableTexturing()
{
    if(!_device->current_context_target->context->use_texturing)
    {
        FlushBlitBuffer();
        _device->current_context_target->context->use_texturing = 1;
    }
}

void Renderer::disableTexturing()
{
    if(_device->current_context_target->context->use_texturing)
    {
        FlushBlitBuffer();
        _device->current_context_target->context->use_texturing = 0;
    }
}

static void upload_texture(const void* pixels, GPU_Rect update_rect, Uint32 format, int alignment, int row_length, unsigned int pitch)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
#if defined(XGPU_USE_OPENGL) || XGPU_GLES_MAJOR_VERSION > 2
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

	glTexSubImage2D(GL_TEXTURE_2D, 0,
		update_rect.x, update_rect.y, update_rect.w, update_rect.h,
		format, GL_UNSIGNED_BYTE, pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#else
	unsigned int i;
	unsigned int h = update_rect.h;
	if (h > 0 && update_rect.w > 0.0f)
	{
		// Must upload row by row to account for row length

		for (i = 0; i < h; ++i)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0,
				update_rect.x, update_rect.y + i, update_rect.w, 1,
				format, GL_UNSIGNED_BYTE, pixels);
			pixels += pitch;
		}
	}
#endif
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

static void upload_new_texture(void* pixels, GPU_Rect update_rect, Uint32 format, int alignment, int row_length, int bytes_per_pixel)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    #if defined(XGPU_USE_OPENGL) || XGPU_GLES_MAJOR_VERSION > 2
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, update_rect.w, update_rect.h, 0,
                    format, GL_UNSIGNED_BYTE, pixels);
                    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    #else
    glTexImage2D(GL_TEXTURE_2D, 0, format, update_rect.w, update_rect.h, 0,
                 format, GL_UNSIGNED_BYTE, NULL);
    
    // Alignment is reset in upload_texture()
    upload_texture(pixels, update_rect, format, alignment, row_length, row_length*bytes_per_pixel);
    #endif
}

#define MIX_COLOR_COMPONENT_NORMALIZED_RESULT(a, b) ((a)/255.0f * (b)/255.0f)
#define MIX_COLOR_COMPONENT(a, b) (((a)/255.0f * (b)/255.0f)*255)

static SDL_Color get_complete_mod_color(Target* target, Image* image)
{
    if(target->use_color)
    {
		SDL_Color color;
		color.r = MIX_COLOR_COMPONENT(target->color.r, image->color.r);
		color.g = MIX_COLOR_COMPONENT(target->color.g, image->color.g);
		color.b = MIX_COLOR_COMPONENT(target->color.b, image->color.b);
		color.a = MIX_COLOR_COMPONENT(target->color.a, image->color.a);

        return color;
    }
    else
        return image->color;
}

void Renderer::prepareToRenderImage(Target* target, Image* image)
{
    Context* context = _device->current_context_target->context;

    enableTexturing();
    if(GL_TRIANGLES != ((ContextData*)context->data)->last_shape)
    {
        FlushBlitBuffer();
        ((ContextData*)context->data)->last_shape = GL_TRIANGLES;
    }

    // Blitting    
    changeBlending(image->use_blending);
    changeBlendMode(image->blend_mode);

    // If we're using the untextured shader, switch it.
    if(context->current_shader_program == context->default_untextured_shader_program)
		ActivateShaderProgram(context->default_textured_shader_program, NULL);
}

void Renderer::prepareToRenderShapes(unsigned int shape)
{
    Context* context = _device->current_context_target->context;

    disableTexturing();
    if(shape != ((ContextData*)context->data)->last_shape)
    {
        FlushBlitBuffer();
        ((ContextData*)context->data)->last_shape = shape;
    }

    // Shape rendering
    // GPU_Color is set elsewhere for shapes
    changeBlending(context->shapes_use_blending);
    changeBlendMode(context->shapes_blend_mode);

    // If we're using the textured shader, switch it.
    if(context->current_shader_program == context->default_textured_shader_program)
		ActivateShaderProgram(context->default_untextured_shader_program, NULL);
}


static void forceChangeViewport(Target* target, GPU_Rect viewport)
{
	float y;
    ContextData* cdata = (ContextData*)(GetContextTarget()->context->data);

    cdata->last_viewport = viewport;

    y = viewport.y;
    // Need the real height to flip the y-coord (from OpenGL coord system)
    if(target->image != NULL)
        y = target->image->h - viewport.h - viewport.y;
    else if(target->context != NULL)
        y = target->context->drawable_h - viewport.h - viewport.y;
    

    glViewport(viewport.x, y, viewport.w, viewport.h);
}

static void changeViewport(Target* target)
{
    ContextData* cdata = (ContextData*)(GetContextTarget()->context->data);

    if(cdata->last_viewport.x == target->viewport.x && cdata->last_viewport.y == target->viewport.y && cdata->last_viewport.w == target->viewport.w && cdata->last_viewport.h == target->viewport.h)
        return;

    forceChangeViewport(target, target->viewport);
}

static void applyTargetCamera(Target* target)
{
    ContextData* cdata = (ContextData*)GetContextTarget()->context->data;

    cdata->last_camera = target->camera;
    cdata->last_camera_inverted = (target->image != NULL);
}

static bool equal_cameras(Camera a, Camera b)
{
    return (a.x == b.x && a.y == b.y && a.z == b.z && a.angle == b.angle && a.zoom == b.zoom);
}

static void changeCamera(Target* target)
{
    //ContextData* cdata = (ContextData*)GetContextTarget()->context->data;

    //if(cdata->last_camera_target != target || !equal_cameras(cdata->last_camera, target->camera))
    {
        applyTargetCamera(target);
    }
}

static void get_camera_matrix(float* result, Camera camera)
{
    ContextData* cdata = (ContextData*)GetContextTarget()->context->data;
    Target* target = cdata->last_target;
    bool invert = cdata->last_camera_inverted;
	float offsetX, offsetY;

    MatrixIdentity(result);

    // Now multiply in the projection part
    if(!invert)
        MatrixOrtho(result, target->camera.x, target->w + target->camera.x, target->h + target->camera.y, target->camera.y, -1.0f, 1.0f);
    else
        MatrixOrtho(result, target->camera.x, target->w + target->camera.x, target->camera.y, target->h + target->camera.y, -1.0f, 1.0f);  // Special inverted orthographic projection because tex coords are inverted already for render-to-texture

    // First the modelview part
    offsetX = target->w/2.0f;
    offsetY = target->h/2.0f;
    MatrixTranslate(result, offsetX, offsetY, 0);
    MatrixRotate(result, target->camera.angle, 0, 0, 1);
    MatrixTranslate(result, -offsetX, -offsetY, 0);

    MatrixTranslate(result, target->camera.x + offsetX, target->camera.y + offsetY, 0);
    MatrixScale(result, target->camera.zoom, target->camera.zoom, 1.0f);
    MatrixTranslate(result, -target->camera.x - offsetX, -target->camera.y - offsetY, 0);
}

Renderer::Renderer(RenderDevice * device) : _device(device)
{
}

Target* Renderer::Init(DeviceID renderer_request, Uint16 w, Uint16 h, WindowFlagEnum SDL_flags)
{
	InitFlagEnum flags;
	SDL_Window* window;

#ifdef XGPU_USE_OPENGL
	const char* vendor_string;
#endif

    if(renderer_request.major_version < 1)
    {
        renderer_request.major_version = 1;
        renderer_request.minor_version = 1;
    }

    // Tell SDL what we require for the GL context.
    flags = GetPreInitFlags();

    _device->init_flags = flags;
    if(flags & INIT_DISABLE_DOUBLE_BUFFER)
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    else
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // GL version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, renderer_request.major_version);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, renderer_request.minor_version);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	_device->requested_id = renderer_request;

	window = NULL;
    // Is there a window already set up that we are supposed to use?
    if(_device->current_context_target != NULL)
        window = SDL_GetWindowFromID(_device->current_context_target->context->windowID);
    else
        window = SDL_GetWindowFromID(GetInitWindow());

    if(window == NULL)
    {
        int win_w, win_h;
        #ifdef __ANDROID__
        win_w = win_h = 0;  // Force Android to create full screen window
        #else
        win_w = w;
        win_h = h;
        #endif

        // Set up window flags
        SDL_flags |= SDL_WINDOW_OPENGL;
        if(!(SDL_flags & SDL_WINDOW_HIDDEN))
            SDL_flags |= SDL_WINDOW_SHOWN;

        _device->SDL_init_flags = SDL_flags;
        window = SDL_CreateWindow("",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  win_w, win_h,
                                  SDL_flags);

        if(window == NULL)
        {
            PushErrorCode("Init", ERROR_BACKEND_ERROR, "Window creation failed.");
            return NULL;
        }

        SetInitWindow(get_window_id(window));
    }
    else
        _device->SDL_init_flags = SDL_flags;

    _device->enabled_features = 0xFFFFFFFF;  // Pretend to support them all if using incompatible headers

    // Create or re-init the current target.  This also creates the GL context and initializes enabled_features.
    if(CreateTargetFromWindow(get_window_id(window), _device->current_context_target) == NULL)
        return NULL;

    // If the dimensions of the window don't match what we asked for, then set up a virtual resolution to pretend like they are.
    if(!(flags & INIT_DISABLE_AUTO_VIRTUAL_RESOLUTION) && w != 0 && h != 0 && (w != _device->current_context_target->w || h != _device->current_context_target->h))
        SetVirtualResolution(_device->current_context_target, w, h);

    // Init glVertexAttrib workaround
    #ifdef XGPU_USE_OPENGL
    vendor_string = (const char*)glGetString(GL_VENDOR);
    if(strstr(vendor_string, "Intel") != NULL)
    {
        vendor_is_Intel = 1;
        apply_Intel_attrib_workaround = 1;
    }
    #endif

    return _device->current_context_target;
}


bool Renderer::IsFeatureEnabled(FeatureEnum feature)
{
    return ((_device->enabled_features & feature) == feature);
}

static bool get_GL_version(int* major, int* minor)
{
    const char* version_string;
    #ifdef XGPU_USE_OPENGL
        // OpenGL < 3.0 doesn't have GL_MAJOR_VERSION.  Check via version string instead.
        version_string = (const char*)glGetString(GL_VERSION);
        if(version_string == NULL || sscanf(version_string, "%d.%d", major, minor) <= 0)
        {
            // Failure
            *major = XGPU_GL_MAJOR_VERSION;
            #if XGPU_GL_MAJOR_VERSION != 3
                *minor = 1;
            #else
                *minor = 0;
            #endif

            PushErrorCode(__func__, ERROR_BACKEND_ERROR, "Failed to parse OpenGL version string: \"%s\"", version_string);
            return false;
        }
        return true;
    #else
        // GLES doesn't have GL_MAJOR_VERSION.  Check via version string instead.
        version_string = (const char*)glGetString(GL_VERSION);
        // OpenGL ES 2.0?
        if(version_string == NULL || sscanf(version_string, "OpenGL ES %d.%d", major, minor) <= 0)
        {
            // OpenGL ES-CM 1.1?  OpenGL ES-CL 1.1?
            if(version_string == NULL || sscanf(version_string, "OpenGL ES-C%*c %d.%d", major, minor) <= 0)
            {
                // Failure
                *major = XGPU_GLES_MAJOR_VERSION;
                #if XGPU_GLES_MAJOR_VERSION == 1
                    *minor = 1;
                #else
                    *minor = 0;
                #endif

                PushErrorCode(__func__, ERROR_BACKEND_ERROR, "Failed to parse OpenGL ES version string: \"%s\"", version_string);
                return false;
            }
        }
        return true;
    #endif
}

static bool get_GLSL_version(int* version)
{
	const char* version_string;
	int major, minor;
#ifdef XGPU_USE_OPENGL
	{
		version_string = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		if (version_string == NULL || sscanf(version_string, "%d.%d", &major, &minor) <= 0)
		{
			PushErrorCode(__func__, ERROR_BACKEND_ERROR, "Failed to parse GLSL version string: \"%s\"", version_string);
			*version = XGPU_GLSL_VERSION;
			return false;
		}
		else
			*version = major * 100 + minor;
	}
#else
	{
		version_string = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		if (version_string == NULL || sscanf(version_string, "OpenGL ES GLSL ES %d.%d", &major, &minor) <= 0)
		{
			PushErrorCode(__func__, ERROR_BACKEND_ERROR, "Failed to parse GLSL ES version string: \"%s\"", version_string);
			*version = XGPU_GLSL_VERSION;
			return false;
		}
		else
			*version = major * 100 + minor;
	}
#endif
    
    return true;
}

static bool get_API_versions(RenderDevice* _device)
{
    return (get_GL_version(&_device->id.major_version, &_device->id.minor_version)
           && get_GLSL_version(&_device->max_shader_version));
}

static void update_stored_dimensions(Target* target)
{
    bool is_fullscreen;
    SDL_Window* window;
    
    if(target->context == NULL)
        return;
    
    window = get_window(target->context->windowID);
    get_window_dimensions(window, &target->context->window_w, &target->context->window_h);
    is_fullscreen = get_fullscreen_state(window);
    
    if(!is_fullscreen)
    {
        target->context->stored_window_w = target->context->window_w;
        target->context->stored_window_h = target->context->window_h;
    }
}

Target* Renderer::CreateTargetFromWindow(Uint32 windowID, Target* target)
{
    bool created = false;  // Make a new one or repurpose an existing target?
	ContextData* cdata;
	SDL_Window* window;
	
	int framebuffer_handle;
	SDL_Color white = { 255, 255, 255, 255 };
#ifdef XGPU_USE_OPENGL
	GLenum err;
#endif
	FeatureEnum required_features = GetRequiredFeatures();

    if(target == NULL)
	{
		int blit_buffer_storage_size;
		int index_buffer_storage_size;

        created = true;
        target = (Target*)SDL_malloc(sizeof(Target));
        memset(target, 0, sizeof(Target));
        target->refcount = 1;
        target->is_alias = false;
        target->data = (TargetData*)SDL_malloc(sizeof(TargetData));
        memset(target->data, 0, sizeof(TargetData));
        ((TargetData*)target->data)->refcount = 1;
        target->image = NULL;
        target->context = (Context*)SDL_malloc(sizeof(Context));
        memset(target->context, 0, sizeof(Context));
        cdata = (ContextData*)SDL_malloc(sizeof(ContextData));
        memset(cdata, 0, sizeof(ContextData));
        target->context->data = cdata;
        target->context->context = NULL;

        cdata->last_image = NULL;
        cdata->last_target = NULL;
        // Initialize the blit buffer
        cdata->blit_buffer_max_num_vertices = BLIT_BUFFER_INIT_MAX_NUM_VERTICES;
        cdata->blit_buffer_num_vertices = 0;
        blit_buffer_storage_size = BLIT_BUFFER_INIT_MAX_NUM_VERTICES*BLIT_BUFFER_STRIDE;
        cdata->blit_buffer = (float*)SDL_malloc(blit_buffer_storage_size);
        cdata->index_buffer_max_num_vertices = BLIT_BUFFER_INIT_MAX_NUM_VERTICES;
        cdata->index_buffer_num_vertices = 0;
        index_buffer_storage_size = BLIT_BUFFER_INIT_MAX_NUM_VERTICES*sizeof(unsigned short);
        cdata->index_buffer = (unsigned short*)SDL_malloc(index_buffer_storage_size);
    }
    else
    {
        RemoveWindowMapping(target->context->windowID);
        cdata = (ContextData*)target->context->data;
    }


    window = get_window(windowID);
    if(window == NULL)
    {
        PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to acquire the window from the given ID.");
        if(created)
        {
            SDL_free(cdata->blit_buffer);
            SDL_free(cdata->index_buffer);
            SDL_free(target->context->data);
            SDL_free(target->context);
            SDL_free(target->data);
            SDL_free(target);
        }
        return NULL;
    }

    // Store the window info
    target->context->windowID = get_window_id(window);

    // Make a new context if needed and make it current
    if(created || target->context->context == NULL)
    {
        target->context->context = SDL_GL_CreateContext(window);
        if(target->context->context == NULL)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to create GL context.");
            SDL_free(cdata->blit_buffer);
            SDL_free(cdata->index_buffer);
            SDL_free(target->context->data);
            SDL_free(target->context);
            SDL_free(target->data);
            SDL_free(target);
            return NULL;
        }
        AddWindowMapping(target);
    }
    
    // We need a GL context before we can get the drawable size.
    SDL_GL_GetDrawableSize(window, &target->context->drawable_w, &target->context->drawable_h);
    
    update_stored_dimensions(target);

    ((TargetData*)target->data)->handle = 0;
    ((TargetData*)target->data)->format = GL_RGBA;

    target->renderer = _device;
    target->context_target = _device->current_context_target;
    target->w = target->context->drawable_w;
    target->h = target->context->drawable_h;
    target->base_w = target->context->drawable_w;
    target->base_h = target->context->drawable_h;

    target->use_clip_rect = false;
    target->clip_rect.x = 0;
    target->clip_rect.y = 0;
    target->clip_rect.w = target->w;
    target->clip_rect.h = target->h;
    target->use_color = false;

    target->viewport = MakeRect(0, 0, target->context->drawable_w, target->context->drawable_h);
    target->camera = GetDefaultCamera();
    target->use_camera = true;

    target->context->line_thickness = 1.0f;
    target->context->use_texturing = true;
    target->context->shapes_use_blending = true;
    target->context->shapes_blend_mode = GetBlendModeFromPreset(DEFAULT_BLEND_MODE);

    cdata->last_color = white;

    cdata->last_use_texturing = true;
    cdata->last_shape = GL_TRIANGLES;

    cdata->last_use_blending = false;
    cdata->last_blend_mode = GetBlendModeFromPreset(DEFAULT_BLEND_MODE);

    cdata->last_viewport = target->viewport;
    cdata->last_camera = target->camera;  // Redundant due to applyTargetCamera(), below
    cdata->last_camera_inverted = false;

#ifdef XGPU_USE_OPENGL
	glewExperimental = GL_TRUE;  // Force GLEW to get exported functions instead of checking via extension string
	err = glewInit();
	if (GLEW_OK != err)
	{
		// Probably don't have the right GL version for this _device
		PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to initialize extensions for _device %s.", _device->id.name);
		target->context->failed = true;
		return NULL;
	}
#endif

    MakeCurrent(target, target->context->windowID);

    framebuffer_handle = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_handle);
    ((TargetData*)target->data)->handle = framebuffer_handle;


    // Update our _device info from the current GL context.
    if(!get_API_versions(_device))
        PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to get backend API versions.");

    // Did the wrong runtime library try to use a later versioned _device?
    if(_device->id.major_version < _device->requested_id.major_version)
    {
        PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Renderer major version (%d) is incompatible with the available OpenGL runtime library version (%d).", _device->requested_id.major_version, _device->id.major_version);
        target->context->failed = true;
        return NULL;
    }
    

    init_features();

    if(!IsFeatureEnabled(required_features))
    {
        PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Renderer does not support required features.");
        target->context->failed = true;
        return NULL;
    }

    // No preference for vsync?
    if(!(_device->init_flags & (INIT_DISABLE_VSYNC | INIT_ENABLE_VSYNC)))
    {
        // Default to late swap vsync if available
        if(SDL_GL_SetSwapInterval(-1) < 0)
            SDL_GL_SetSwapInterval(1);  // Or go for vsync
    }
    else if(_device->init_flags & INIT_ENABLE_VSYNC)
        SDL_GL_SetSwapInterval(1);
    else if(_device->init_flags & INIT_DISABLE_VSYNC)
        SDL_GL_SetSwapInterval(0);    

    // Set up GL state

    target->context->projection_matrix.size = 1;
    MatrixIdentity(target->context->projection_matrix.matrix[0]);

    target->context->modelview_matrix.size = 1;
    MatrixIdentity(target->context->modelview_matrix.matrix[0]);

    target->context->matrix_mode = MODELVIEW;

    // Modes
#ifndef XGPU_SKIP_ENABLE_TEXTURE_2D
	glEnable(GL_TEXTURE_2D);
#endif
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	BlendMode default_blend_mode = GetBlendModeFromPreset(DEFAULT_BLEND_MODE);
	glBlendFunc(default_blend_mode.source_color, default_blend_mode.dest_color);

    glDisable(GL_BLEND);
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    // Viewport and Framebuffer
    glViewport(0.0f, 0.0f, target->viewport.w, target->viewport.h);

    glClear( GL_COLOR_BUFFER_BIT );

    // Set up camera
    applyTargetCamera(target);

    SetLineThickness(1.0f);

	// Create vertex array container and buffer
    target->context->default_textured_shader_program = 0;
    target->context->default_untextured_shader_program = 0;
    target->context->current_shader_program = 0;

    // Load default shaders
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
    {
        Uint32 v, f, p;
        const char* textured_vertex_shader_source = DEFAULT_TEXTURED_VERTEX_SHADER_SOURCE;
        const char* textured_fragment_shader_source = DEFAULT_TEXTURED_FRAGMENT_SHADER_SOURCE;
        const char* untextured_vertex_shader_source = DEFAULT_UNTEXTURED_VERTEX_SHADER_SOURCE;
        const char* untextured_fragment_shader_source = DEFAULT_UNTEXTURED_FRAGMENT_SHADER_SOURCE;

        #ifdef XGPU_ENABLE_CORE_SHADERS
        // Use core shaders only when supported by the actual context we got
        if(_device->id.major_version > 3 || (_device->id.major_version == 3 && _device->id.minor_version >= 2))
        {
            textured_vertex_shader_source = DEFAULT_TEXTURED_VERTEX_SHADER_SOURCE_CORE;
            textured_fragment_shader_source = DEFAULT_TEXTURED_FRAGMENT_SHADER_SOURCE_CORE;
            untextured_vertex_shader_source = DEFAULT_UNTEXTURED_VERTEX_SHADER_SOURCE_CORE;
            untextured_fragment_shader_source = DEFAULT_UNTEXTURED_FRAGMENT_SHADER_SOURCE_CORE;
        }
        #endif

        // Textured shader
        v = CompileShader(VERTEX_SHADER, textured_vertex_shader_source);

        if(!v)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to load default textured vertex shader: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        f = CompileShader(FRAGMENT_SHADER, textured_fragment_shader_source);

        if(!f)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to load default textured fragment shader: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        p = CreateShaderProgram();
        AttachShader(p, v);
        AttachShader(p, f);
        LinkShaderProgram(p);

        if(!p)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to link default textured shader program: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        target->context->default_textured_shader_program = p;

        // Get locations of the attributes in the shader
        target->context->default_textured_shader_block = LoadShaderBlock(p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");


        // Untextured shader
        v = CompileShader(VERTEX_SHADER, untextured_vertex_shader_source);

        if(!v)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to load default untextured vertex shader: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        f = CompileShader(FRAGMENT_SHADER, untextured_fragment_shader_source);

        if(!f)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to load default untextured fragment shader: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        p = CreateShaderProgram();
        AttachShader(p, v);
        AttachShader(p, f);
        LinkShaderProgram(p);

        if(!p)
        {
            PushErrorCode("CreateTargetFromWindow", ERROR_BACKEND_ERROR, "Failed to link default untextured shader program: %s.", GetShaderMessage());
            target->context->failed = true;
            return NULL;
        }

        glUseProgram(p);

        target->context->default_untextured_shader_program = target->context->current_shader_program = p;

        // Get locations of the attributes in the shader
        target->context->default_untextured_shader_block = LoadShaderBlock(p, "gpu_Vertex", NULL, "gpu_Color", "gpu_ModelViewProjectionMatrix");
        SetShaderBlock(target->context->default_untextured_shader_block);
    }
    else
    {
        snprintf(shader_message, 256, "Shaders not supported by this hardware.  Default shaders are disabled.\n");
        target->context->default_untextured_shader_program = target->context->current_shader_program = 0;
    }


	// Create vertex array container and buffer

	glGenBuffers(2, cdata->blit_VBO);
	// Create space on the GPU
	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, BLIT_BUFFER_STRIDE * cdata->blit_buffer_max_num_vertices, NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, BLIT_BUFFER_STRIDE * cdata->blit_buffer_max_num_vertices, NULL, GL_STREAM_DRAW);
	cdata->blit_VBO_flop = false;

	glGenBuffers(1, &cdata->blit_IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cdata->blit_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * cdata->blit_buffer_max_num_vertices, NULL, GL_DYNAMIC_DRAW);

	glGenBuffers(16, cdata->attribute_VBO);

	// Init 16 attributes to 0 / NULL.
	memset(cdata->shader_attributes, 0, 16 * sizeof(AttributeSource));

    return target;
}


Target* Renderer::CreateAliasTarget(Target* target)
{
	Target* result;
	(void)_device;

    if(target == NULL)
        return NULL;

    result = (Target*)SDL_malloc(sizeof(Target));

    // Copy the members
    *result = *target;

    // Alias info
    if(target->image != NULL)
        target->image->refcount++;
    ((TargetData*)target->data)->refcount++;
    result->refcount = 1;
    result->is_alias = true;

    return result;
}

void Renderer::MakeCurrent(Target* target, Uint32 windowID)
{
	SDL_Window* window;

    if(target == NULL || target->context == NULL)
        return;
    
    if(target->image != NULL)
        return;
    
    if(target->context->context != NULL)
    {
        _device->current_context_target = target;        
        SDL_GL_MakeCurrent(SDL_GetWindowFromID(windowID), target->context->context);        
        
        // Reset window mapping, base size, and camera if the target's window was changed
        if(target->context->windowID != windowID)
        {
            FlushBlitBuffer();

            // Update the window mappings
            RemoveWindowMapping(windowID);
            // Don't remove the target's current mapping.  That lets other windows refer to it.
            target->context->windowID = windowID;
            AddWindowMapping(target);

            // Update target's window size
            window = get_window(windowID);
            if(window != NULL)
            {
                get_window_dimensions(window, &target->context->window_w, &target->context->window_h);
                get_drawable_dimensions(window, &target->context->drawable_w, &target->context->drawable_h);
                target->base_w = target->context->drawable_w;
                target->base_h = target->context->drawable_h;
            }

            // Reset the camera for this window
            applyTargetCamera(((ContextData*)_device->current_context_target->context->data)->last_target);
        }
    }
}

void Renderer::SetAsCurrent(RenderDevice* _device)
{
    if(_device->current_context_target == NULL)
        return;

    MakeCurrent(_device->current_context_target, _device->current_context_target->context->windowID);
}

void Renderer::ResetRendererState(RenderDevice* _device)
{
    Target* target;
    ContextData* cdata;

    if(_device->current_context_target == NULL)
        return;

    target = _device->current_context_target;
    cdata = (ContextData*)target->context->data;


    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        glUseProgram(target->context->current_shader_program);

    SDL_GL_MakeCurrent(SDL_GetWindowFromID(target->context->windowID), target->context->context);

    #ifndef XGPU_SKIP_ENABLE_TEXTURE_2D
    if(cdata->last_use_texturing)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);
    #endif

    if(cdata->last_use_blending)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    forceChangeBlendMode(cdata->last_blend_mode);

    forceChangeViewport(target, target->viewport);

    if(cdata->last_image != NULL)
        glBindTexture(GL_TEXTURE_2D, ((ImageData*)(cdata->last_image)->data)->handle);

    if(cdata->last_target != NULL)
        extBindFramebuffer(((TargetData*)cdata->last_target->data)->handle);
    else
        extBindFramebuffer(((TargetData*)target->data)->handle);
}

bool Renderer::SetWindowResolution(Uint16 w, Uint16 h)
{
    Target* target = _device->current_context_target;

    bool isCurrent = isCurrentTarget(target);
    if(isCurrent)
        FlushBlitBuffer();

    // Don't need to resize (only update internals) when resolution isn't changing.
    get_target_window_dimensions(target, &target->context->window_w, &target->context->window_h);
    get_target_drawable_dimensions(target, &target->context->drawable_w, &target->context->drawable_h);
    if(target->context->window_w != w || target->context->window_h != h)
    {
        resize_window(target, w, h);
        get_target_window_dimensions(target, &target->context->window_w, &target->context->window_h);
        get_target_drawable_dimensions(target, &target->context->drawable_w, &target->context->drawable_h);
    }

    // Store the resolution for fullscreen_desktop changes
    update_stored_dimensions(target);

    // Update base dimensions
    target->base_w = target->context->drawable_w;
    target->base_h = target->context->drawable_h;

    // Resets virtual resolution
    target->w = target->base_w;
    target->h = target->base_h;
    target->using_virtual_resolution = false;

    // Resets viewport
    target->viewport = MakeRect(0, 0, target->w, target->h);
    changeViewport(target);

    UnsetClip(target);

    if(isCurrent)
        applyTargetCamera(target);

    return 1;
}

void Renderer::SetVirtualResolution(Target* target, Uint16 w, Uint16 h)
{
	bool isCurrent;

    if(target == NULL)
        return;

    isCurrent = isCurrentTarget(target);
    if(isCurrent)
        FlushBlitBuffer();

    target->w = w;
    target->h = h;
    target->using_virtual_resolution = true;

    if(isCurrent)
        applyTargetCamera(target);
}

void Renderer::UnsetVirtualResolution(Target* target)
{
	bool isCurrent;

    if(target == NULL)
        return;

    isCurrent = isCurrentTarget(target);
    if(isCurrent)
        FlushBlitBuffer();

    target->w = target->base_w;
    target->h = target->base_h;

    target->using_virtual_resolution = false;

    if(isCurrent)
        applyTargetCamera(target);
}

void Renderer::Quit(RenderDevice* _device)
{
    FreeTarget(_device->current_context_target);
    _device->current_context_target = NULL;
}



bool Renderer::SetFullscreen(bool enable_fullscreen, bool use_desktop_resolution)
{
    Target* target = _device->current_context_target;


    SDL_Window* window = SDL_GetWindowFromID(target->context->windowID);
    Uint32 old_flags = SDL_GetWindowFlags(window);
    bool was_fullscreen = (old_flags & SDL_WINDOW_FULLSCREEN);
    bool is_fullscreen = was_fullscreen;

    Uint32 flags = 0;

    if(enable_fullscreen)
    {
        if(use_desktop_resolution)
            flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        else
            flags = SDL_WINDOW_FULLSCREEN;
    }

    if(SDL_SetWindowFullscreen(window, flags) >= 0)
    {
        flags = SDL_GetWindowFlags(window);
        is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN);

        // If we just went fullscreen, save the original resolution
        // We do this because you can't depend on the resolution to be preserved by SDL
        // SDL_WINDOW_FULLSCREEN_DESKTOP changes the resolution and SDL_WINDOW_FULLSCREEN can change it when a given mode is not available
        if(!was_fullscreen && is_fullscreen)
        {
            target->context->stored_window_w = target->context->window_w;
            target->context->stored_window_h = target->context->window_h;
        }

        // If we're in windowed mode now and a resolution was stored, restore the original window resolution
        if(was_fullscreen && !is_fullscreen && (target->context->stored_window_w != 0 && target->context->stored_window_h != 0))
            SDL_SetWindowSize(window, target->context->stored_window_w, target->context->stored_window_h);
    }

    if(is_fullscreen != was_fullscreen)
    {
        // Update window dims
        get_target_window_dimensions(target, &target->context->window_w, &target->context->window_h);
        get_target_drawable_dimensions(target, &target->context->drawable_w, &target->context->drawable_h);
        
        // If virtual res is not set, we need to update the target dims and reset stuff that no longer is right
        if(!target->using_virtual_resolution)
        {
            // Update dims
            target->w = target->context->drawable_w;
            target->h = target->context->drawable_h;
        }

        // Reset viewport
        target->viewport = MakeRect(0, 0, target->context->drawable_w, target->context->drawable_h);
        changeViewport(target);

        // Reset clip
        UnsetClip(target);

        // Update camera
        if(isCurrentTarget(target))
            applyTargetCamera(target);
    }

    target->base_w = target->context->drawable_w;
    target->base_h = target->context->drawable_h;

    return is_fullscreen;
}


Camera Renderer::SetCamera(Target* target, Camera* cam)
{
    Camera new_camera;
	Camera old_camera;

    if(target == NULL)
    {
        PushErrorCode("SetCamera", ERROR_NULL_ARGUMENT, "target");
        return GetDefaultCamera();
    }

    if(cam == NULL)
        new_camera = GetDefaultCamera();
    else
        new_camera = *cam;

    old_camera = target->camera;

    if(!equal_cameras(new_camera, old_camera))
    {
        if(isCurrentTarget(target))
            FlushBlitBuffer();

        target->camera = new_camera;
    }

    return old_camera;
}

GLuint Renderer::CreateUninitializedTexture()
{
    GLuint handle;

    glGenTextures(1, &handle);
    if(handle == 0)
        return 0;

    flushAndBindTexture(handle);

    // Set the texture's stretching properties
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return handle;
}

Image* Renderer::CreateUninitializedImage(Uint16 w, Uint16 h, FormatEnum format)
{
    GLuint handle, num_layers, bytes_per_pixel;
    GLenum gl_format;
	Image* result;
	ImageData* data;
	SDL_Color white = { 255, 255, 255, 255 };

    switch(format)
    {
        case FORMAT_LUMINANCE:
            gl_format = GL_LUMINANCE;
            num_layers = 1;
            bytes_per_pixel = 1;
            break;
        case FORMAT_LUMINANCE_ALPHA:
            gl_format = GL_LUMINANCE_ALPHA;
            num_layers = 1;
            bytes_per_pixel = 2;
            break;
        case FORMAT_RGB:
            gl_format = GL_RGB;
            num_layers = 1;
            bytes_per_pixel = 3;
            break;
        case FORMAT_RGBA:
            gl_format = GL_RGBA;
            num_layers = 1;
            bytes_per_pixel = 4;
            break;
        case FORMAT_ALPHA:
            gl_format = GL_ALPHA;
            num_layers = 1;
            bytes_per_pixel = 1;
            break;
        #ifndef XGPU_USE_GLES
        case FORMAT_RG:
            gl_format = GL_RG;
            num_layers = 1;
            bytes_per_pixel = 2;
            break;
        #endif
        case FORMAT_YCbCr420P:
            gl_format = GL_LUMINANCE;
            num_layers = 3;
            bytes_per_pixel = 1;
            break;
        case FORMAT_YCbCr422:
            gl_format = GL_LUMINANCE;
            num_layers = 3;
            bytes_per_pixel = 1;
            break;
        default:
            PushErrorCode("CreateUninitializedImage", ERROR_DATA_ERROR, "Unsupported image format (0x%x)", format);
            return NULL;
    }

    if(bytes_per_pixel < 1 || bytes_per_pixel > 4)
    {
        PushErrorCode("CreateUninitializedImage", ERROR_DATA_ERROR, "Unsupported number of bytes per pixel (%d)", bytes_per_pixel);
        return NULL;
    }

    // Create the underlying texture
    handle = CreateUninitializedTexture();
    if(handle == 0)
    {
        PushErrorCode("CreateUninitializedImage", ERROR_BACKEND_ERROR, "Failed to generate a texture handle.");
        return NULL;
    }

    // Create the Image
    result = (Image*)SDL_malloc(sizeof(Image));
    result->refcount = 1;
    data = (ImageData*)SDL_malloc(sizeof(ImageData));
    data->refcount = 1;
    result->target = NULL;
    result->renderer = _device;
    result->context_target = _device->current_context_target;
    result->format = format;
    result->num_layers = num_layers;
    result->bytes_per_pixel = bytes_per_pixel;
    result->has_mipmaps = false;
    
    result->anchor_x = _device->default_image_anchor_x;
    result->anchor_y = _device->default_image_anchor_y;
	result->anchor_fixed = false;
    
    result->color = white;
    result->use_blending = true;
    result->blend_mode = GetBlendModeFromPreset(DEFAULT_BLEND_MODE);
    result->filter_mode = FILTER_LINEAR;
    //result->snap_mode = SNAP_POSITION_AND_DIMENSIONS;
	result->snap_mode = SNAP_NONE;
    result->wrap_mode_x = WRAP_NONE;
    result->wrap_mode_y = WRAP_NONE;

    result->data = data;
    result->is_alias = false;
    data->handle = handle;
    data->owns_handle = true;
    data->format = gl_format;

    result->using_virtual_resolution = false;
    result->w = w;
    result->h = h;
    result->base_w = w;
    result->base_h = h;
    // POT textures will change this later
    result->texture_w = w;
    result->texture_h = h;

    return result;
}


Image* Renderer::CreateImage(Uint16 w, Uint16 h, FormatEnum format)
{
	Image* result;
	GLenum internal_format;
	static unsigned char* zero_buffer = NULL;
	static unsigned int zero_buffer_size = 0;

    if(format < 1)
    {
        PushErrorCode("CreateImage", ERROR_DATA_ERROR, "Unsupported image format (0x%x)", format);
        return NULL;
    }

    result = CreateUninitializedImage(w, h, format);

    if(result == NULL)
    {
        PushErrorCode("CreateImage", ERROR_BACKEND_ERROR, "Could not create image as requested.");
        return NULL;
    }

    changeTexturing(true);
    bindTexture(result);

    internal_format = ((ImageData*)(result->data))->format;
    w = result->w;
    h = result->h;
    if(!(_device->enabled_features & FEATURE_NON_POWER_OF_TWO))
    {
        if(!isPowerOfTwo(w))
            w = getNearestPowerOf2(w);
        if(!isPowerOfTwo(h))
            h = getNearestPowerOf2(h);
    }

    // Initialize texture using a blank buffer
    if(zero_buffer_size < (unsigned int)(w*h*result->bytes_per_pixel))
    {
        SDL_free(zero_buffer);
        zero_buffer_size = w*h*result->bytes_per_pixel;
        zero_buffer = (unsigned char*)SDL_malloc(zero_buffer_size);
        memset(zero_buffer, 0, zero_buffer_size);
    }
        
    upload_new_texture(zero_buffer, MakeRect(0, 0, w, h), internal_format, 1, w, result->bytes_per_pixel);
        
    // Tell SDL_gpu what we got (power-of-two requirements have made this change)
    result->texture_w = w;
    result->texture_h = h;

    return result;
}

Image* Renderer::CreateAliasImage(Image* image)
{
	Image* result;
	(void)_device;

    if(image == NULL)
        return NULL;

    result = (Image*)SDL_malloc(sizeof(Image));
    // Copy the members
    *result = *image;
	result->target = NULL;

    // Alias info
    ((ImageData*)image->data)->refcount++;
	
    result->refcount = 1;
    result->is_alias = true;

    return result;
}

bool Renderer::readTargetPixels(Target* source, GLint format, GLubyte* pixels)
{
    if(source == NULL)
        return false;

    if(isCurrentTarget(source))
        FlushBlitBuffer();

    if(bindFramebuffer(source))
    {
        glReadPixels(0, 0, source->base_w, source->base_h, format, GL_UNSIGNED_BYTE, pixels);
        return true;
    }
    return false;
}

bool Renderer::readImagePixels(Image* source, GLint format, GLubyte* pixels)
{
#ifdef XGPU_USE_GLES
	bool created_target;
	bool result;
#endif

    if(source == NULL)
        return false;

	// No glGetTexImage() in OpenGLES
#ifdef XGPU_USE_GLES
	// Load up the target
	created_target = false;
	if (source->target == NULL)
	{
		LoadTarget(source);
		created_target = true;
	}
	// Get the data
	// FIXME: This may use different dimensions than the OpenGL code... (base_w vs texture_w)
	// FIXME: I should force it to use the texture dims.
	result = readTargetPixels(source->target, format, pixels);
	// Free the target
	if (created_target)
		FreeTarget(source->target);
	return result;
#else
	// Bind the texture temporarily
	glBindTexture(GL_TEXTURE_2D, ((ImageData*)source->data)->handle);
	// Get the data
	glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, pixels);
	// Rebind the last texture
	if (((ContextData*)_device->current_context_target->context->data)->last_image != NULL)
		glBindTexture(GL_TEXTURE_2D, ((ImageData*)(((ContextData*)_device->current_context_target->context->data)->last_image)->data)->handle);
	return true;
#endif
}

unsigned char* Renderer::getRawTargetData(Target* target)
{
	int bytes_per_pixel;
	unsigned char* data;
	int pitch;
	unsigned char* copy;
	int y;

    if(isCurrentTarget(target))
        FlushBlitBuffer();

    bytes_per_pixel = 4;
    if(target->image != NULL)
        bytes_per_pixel = target->image->bytes_per_pixel;
    data = (unsigned char*)SDL_malloc(target->base_w * target->base_h * bytes_per_pixel);

    // This can take regions of pixels, so using base_w and base_h with an image target should be fine.
    if(!readTargetPixels(target, ((TargetData*)target->data)->format, data))
    {
        SDL_free(data);
        return NULL;
    }

    // Flip the data vertically (OpenGL framebuffer is read upside down)
    pitch = target->base_w * bytes_per_pixel;
    copy = (unsigned char*)SDL_malloc(pitch);

    for(y = 0; y < target->base_h/2; y++)
    {
        unsigned char* top = &data[target->base_w * y * bytes_per_pixel];
        unsigned char* bottom = &data[target->base_w * (target->base_h - y - 1) * bytes_per_pixel];
        memcpy(copy, top, pitch);
        memcpy(top, bottom, pitch);
        memcpy(bottom, copy, pitch);
    }
    SDL_free(copy);

    return data;
}

unsigned char* Renderer::getRawImageData(Image* image)
{
	unsigned char* data;

    if(image->target != NULL && isCurrentTarget(image->target))
        FlushBlitBuffer();

    data = (unsigned char*)SDL_malloc(image->texture_w * image->texture_h * image->bytes_per_pixel);

    // FIXME: Sometimes the texture is stored and read in RGBA even when I specify RGB.  getRawImageData() might need to return the stored format or Bpp.
    if(!readImagePixels(image, ((ImageData*)image->data)->format, data))
    {
        SDL_free(data);
        return NULL;
    }

    return data;
}

bool Renderer::SaveImage(Image* image, const char* filename, FileFormatEnum format)
{
    bool result;
    SDL_Surface* surface;

    if(image == NULL || filename == NULL ||
            image->texture_w < 1 || image->texture_h < 1 || image->bytes_per_pixel < 1 || image->bytes_per_pixel > 4)
    {
        return false;
    }

    surface = CopySurfaceFromImage(image);

    if(surface == NULL)
        return false;
    
    result = SaveSurface(surface, filename, format);

    SDL_FreeSurface(surface);
    return result;
}

SDL_Surface* Renderer::CopySurfaceFromTarget(Target* target)
{
    unsigned char* data;
    SDL_Surface* result;
	SDL_PixelFormat* format;

    if(target == NULL)
    {
        PushErrorCode("CopySurfaceFromTarget", ERROR_NULL_ARGUMENT, "target");
        return NULL;
    }
    if(target->base_w < 1 || target->base_h < 1)
    {
        PushErrorCode("CopySurfaceFromTarget", ERROR_DATA_ERROR, "Invalid target dimensions (%dx%d)", target->base_w, target->base_h);
        return NULL;
    }

    data = getRawTargetData(target);

    if(data == NULL)
    {
        PushErrorCode("CopySurfaceFromTarget", ERROR_BACKEND_ERROR, "Could not retrieve target data.");
        return NULL;
    }

    format = AllocFormat(((TargetData*)target->data)->format);

    result = SDL_CreateRGBSurface(SDL_SWSURFACE, target->base_w, target->base_h, format->BitsPerPixel, format->Rmask, format->Gmask, format->Bmask, format->Amask);

	if(result == NULL)
	{
	    PushErrorCode("CopySurfaceFromTarget", ERROR_DATA_ERROR, "Failed to create new %dx%d surface", target->base_w, target->base_h);
	    SDL_free(data);
		return NULL;
	}

    // Copy row-by-row in case the pitch doesn't match
    {
        int i;
        int source_pitch = target->base_w*format->BytesPerPixel;
        for(i = 0; i < target->base_h; ++i)
        {
            memcpy((Uint8*)result->pixels + i*result->pitch, data + source_pitch*i, source_pitch);
        }
    }

    SDL_free(data);

	SDL_free(format);
    return result;
}

SDL_Surface* Renderer::CopySurfaceFromImage(Image* image)
{
    unsigned char* data;
    SDL_Surface* result;
	SDL_PixelFormat* format;
	int w, h;

    if(image == NULL)
    {
        PushErrorCode("CopySurfaceFromImage", ERROR_NULL_ARGUMENT, "image");
        return NULL;
    }
    if(image->w < 1 || image->h < 1)
    {
        PushErrorCode("CopySurfaceFromImage", ERROR_DATA_ERROR, "Invalid image dimensions (%dx%d)", image->base_w, image->base_h);
        return NULL;
    }

    // FIXME: Virtual resolutions overwrite the NPOT dimensions when NPOT textures are not supported!
    if(image->using_virtual_resolution)
    {
        w = image->texture_w;
        h = image->texture_h;
    }
    else
    {
        w = image->w;
        h = image->h;
    }
    data = getRawImageData(image);

    if(data == NULL)
    {
        PushErrorCode("CopySurfaceFromImage", ERROR_BACKEND_ERROR, "Could not retrieve target data.");
        return NULL;
    }

    format = AllocFormat(((ImageData*)image->data)->format);

    result = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, format->BitsPerPixel, format->Rmask, format->Gmask, format->Bmask, format->Amask);

	if(result == NULL)
	{
	    PushErrorCode("CopySurfaceFromImage", ERROR_DATA_ERROR, "Failed to create new %dx%d surface", w, h);
	    SDL_free(data);
		return NULL;
	}

    // Copy row-by-row in case the pitch doesn't match
    {
        int i;
        int source_pitch = image->texture_w*format->BytesPerPixel;  // Use the actual texture width to pull from the data
        for(i = 0; i < h; ++i)
        {
            memcpy((Uint8*)result->pixels + i*result->pitch, data + source_pitch*i, result->pitch);
        }
    }

    SDL_free(data);

	SDL_free(format);
    return result;
}



// Returns 0 if a direct conversion (asking OpenGL to do it) is safe.  Returns 1 if a copy is needed.  Returns -1 on error.
// The surfaceFormatResult is used to specify what direct conversion format the surface pixels are in (source format).
#ifdef XGPU_USE_GLES
// OpenGLES does not do direct conversion.  Internal format (glFormat) and original format (surfaceFormatResult) must be the same.
int Renderer::compareFormats(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult)
{
    SDL_PixelFormat* format = surface->format;
    switch(glFormat)
    {
        // 3-channel formats
    case GL_RGB:
        if(format->BytesPerPixel != 3)
            return 1;

        if(format->Rmask == 0x0000FF && format->Gmask == 0x00FF00 && format->Bmask ==  0xFF0000)
        {
            if(surfaceFormatResult != NULL)
                *surfaceFormatResult = GL_RGB;
            return 0;
        }
#ifdef GL_BGR
        if(format->Rmask == 0xFF0000 && format->Gmask == 0x00FF00 && format->Bmask == 0x0000FF)
        {
            if(_device->enabled_features & FEATURE_GL_BGR)
            {
                if(surfaceFormatResult != NULL)
                    *surfaceFormatResult = GL_BGR;
				return 0;
            }
        }
#endif
        return 1;
        // 4-channel formats
    case GL_RGBA:
        if(format->BytesPerPixel != 4)
            return 1;

        if (format->Rmask == 0x000000FF && format->Gmask == 0x0000FF00 && format->Bmask ==  0x00FF0000)
        {
            if(surfaceFormatResult != NULL)
                *surfaceFormatResult = GL_RGBA;
            return 0;
        }
#ifdef GL_BGRA
        if (format->Rmask == 0x00FF0000 && format->Gmask == 0x0000FF00 && format->Bmask == 0x000000FF)
        {
            if(_device->enabled_features & FEATURE_GL_BGRA)
            {
				if(surfaceFormatResult != NULL)
					*surfaceFormatResult = GL_BGRA;
				return 0;
			}
        }
#endif
#ifdef GL_ABGR
        if (format->Rmask == 0xFF000000 && format->Gmask == 0x00FF0000 && format->Bmask == 0x0000FF00)
        {
            if(_device->enabled_features & FEATURE_GL_ABGR)
            {
				if(surfaceFormatResult != NULL)
					*surfaceFormatResult = GL_ABGR;
				return 0;
			}
        }
#endif
        return 1;
    default:
        PushErrorCode("CompareFormats", ERROR_DATA_ERROR, "Invalid texture format (0x%x)", glFormat);
        return -1;
    }
}
#else
//GL_RGB/GL_RGBA and Surface format
int Renderer::compareFormats(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult)
{
    SDL_PixelFormat* format = surface->format;
    switch(glFormat)
    {
        // 3-channel formats
    case GL_RGB:
        if(format->BytesPerPixel != 3)
            return 1;

        // Looks like RGB?  Easy!
        if(format->Rmask == 0x0000FF && format->Gmask == 0x00FF00 && format->Bmask == 0xFF0000)
        {
            if(surfaceFormatResult != NULL)
                *surfaceFormatResult = GL_RGB;
            return 0;
        }
        // Looks like BGR?
        if(format->Rmask == 0xFF0000 && format->Gmask == 0x00FF00 && format->Bmask == 0x0000FF)
        {
#ifdef GL_BGR
            if(_device->enabled_features & FEATURE_GL_BGR)
            {
                if(surfaceFormatResult != NULL)
                    *surfaceFormatResult = GL_BGR;
                return 0;
            }
#endif
        }
        return 1;

        // 4-channel formats
    case GL_RGBA:

        if(format->BytesPerPixel != 4)
            return 1;

        // Looks like RGBA?  Easy!
        if(format->Rmask == 0x000000FF && format->Gmask == 0x0000FF00 && format->Bmask == 0x00FF0000)
        {
            if(surfaceFormatResult != NULL)
                *surfaceFormatResult = GL_RGBA;
            return 0;
        }
        // Looks like ABGR?
        if(format->Rmask == 0xFF000000 && format->Gmask == 0x00FF0000 && format->Bmask == 0x0000FF00)
        {
#ifdef GL_ABGR
            if(_device->enabled_features & FEATURE_GL_ABGR)
            {
                if(surfaceFormatResult != NULL)
                    *surfaceFormatResult = GL_ABGR;
                return 0;
            }
#endif
        }
        // Looks like BGRA?
        else if(format->Rmask == 0x00FF0000 && format->Gmask == 0x0000FF00 && format->Bmask == 0x000000FF)
        {
#ifdef GL_BGRA
            if(_device->enabled_features & FEATURE_GL_BGRA)
            {
                //ARGB, for OpenGL BGRA
                if(surfaceFormatResult != NULL)
                    *surfaceFormatResult = GL_BGRA;
                return 0;
            }
#endif
        }
        return 1;
    default:
        PushErrorCode("CompareFormats", ERROR_DATA_ERROR, "Invalid texture format (0x%x)", glFormat);
        return -1;
    }
}
#endif


// Adapted from SDL_AllocFormat()
SDL_PixelFormat* Renderer::AllocFormat(GLenum glFormat)
{
    // Yes, I need to do the whole thing myself... :(
    int channels;
    Uint32 Rmask, Gmask, Bmask, Amask = 0, mask;
	SDL_PixelFormat* result;

    switch(glFormat)
    {
    case GL_RGB:
        channels = 3;
        Rmask = 0x0000FF;
        Gmask = 0x00FF00;
        Bmask = 0xFF0000;
        break;
#ifdef GL_BGR
    case GL_BGR:
        channels = 3;
        Rmask = 0xFF0000;
        Gmask = 0x00FF00;
        Bmask = 0x0000FF;
        break;
#endif
    case GL_RGBA:
        channels = 4;
        Rmask = 0x000000FF;
        Gmask = 0x0000FF00;
        Bmask = 0x00FF0000;
        Amask = 0xFF000000;
        break;
#ifdef GL_BGRA
    case GL_BGRA:
        channels = 4;
        Rmask = 0x00FF0000;
        Gmask = 0x0000FF00;
        Bmask = 0x000000FF;
        Amask = 0xFF000000;
        break;
#endif
#ifdef GL_ABGR
    case GL_ABGR:
        channels = 4;
        Rmask = 0xFF000000;
        Gmask = 0x00FF0000;
        Bmask = 0x0000FF00;
        Amask = 0x000000FF;
        break;
#endif
    default:
        return NULL;
    }

    //LogError("AllocFormat(): %d, Masks: %X %X %X %X\n", glFormat, Rmask, Gmask, Bmask, Amask);

    result = (SDL_PixelFormat*)SDL_malloc(sizeof(SDL_PixelFormat));
    memset(result, 0, sizeof(SDL_PixelFormat));

    result->BitsPerPixel = 8*channels;
    result->BytesPerPixel = channels;

    result->Rmask = Rmask;
    result->Rshift = 0;
    result->Rloss = 8;
    if (Rmask) {
        for (mask = Rmask; !(mask & 0x01); mask >>= 1)
            ++result->Rshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Rloss;
    }

    result->Gmask = Gmask;
    result->Gshift = 0;
    result->Gloss = 8;
    if (Gmask) {
        for (mask = Gmask; !(mask & 0x01); mask >>= 1)
            ++result->Gshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Gloss;
    }

    result->Bmask = Bmask;
    result->Bshift = 0;
    result->Bloss = 8;
    if (Bmask) {
        for (mask = Bmask; !(mask & 0x01); mask >>= 1)
            ++result->Bshift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Bloss;
    }

    result->Amask = Amask;
    result->Ashift = 0;
    result->Aloss = 8;
    if (Amask) {
        for (mask = Amask; !(mask & 0x01); mask >>= 1)
            ++result->Ashift;
        for (; (mask & 0x01); mask >>= 1)
            --result->Aloss;
    }

    return result;
}

// Returns NULL on failure.  Returns the original surface if no copy is needed.  Returns a new surface converted to the right format otherwise.
SDL_Surface* Renderer::copySurfaceIfNeeded(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult)
{
    #ifdef XGPU_USE_GLES
    SDL_Surface* original = surface;
    #endif

    // If format doesn't match, we need to do a copy
    int format_compare = compareFormats(glFormat, surface, surfaceFormatResult);

    // There's a problem, logged in compareFormats()
    if(format_compare < 0)
        return NULL;

    // Copy it to a different format
    if(format_compare > 0)
    {
        // Convert to the right format
        SDL_PixelFormat* dst_fmt = AllocFormat(glFormat);
        surface = SDL_ConvertSurface(surface, dst_fmt, 0);
		SDL_free(dst_fmt);
        if(surfaceFormatResult != NULL && surface != NULL)
            *surfaceFormatResult = glFormat;
    }

    // No copy needed
    return surface;
}

Image* Renderer::gpu_copy_image_pixels_only(Image* image)
{
    Image* result = NULL;

    if(image == NULL)
        return NULL;

    switch(image->format)
    {
        case FORMAT_RGB:
        case FORMAT_RGBA:
        // Copy via framebuffer blitting (fast)
		{
			Target* target;

            result = Renderer::CreateImage(image->texture_w, image->texture_h, image->format);
            if(result == NULL)
            {
                PushErrorCode("CopyImage", ERROR_BACKEND_ERROR, "Failed to create new image.");
                return NULL;
            }

            target = LoadTarget(result);
            if(target == NULL)
            {
                FreeImage(result);
                PushErrorCode("CopyImage", ERROR_BACKEND_ERROR, "Failed to load target.");
                return NULL;
            }

            // For some reason, I wasn't able to get glCopyTexImage2D() or glCopyTexSubImage2D() working without getting GL_INVALID_ENUM (0x500).
            // It seemed to only work for the default framebuffer...
			{
				// Clear the color, blending, and filter mode
				SDL_Color color = image->color;
				bool use_blending = image->use_blending;
				FilterEnum filter_mode = image->filter_mode;
				bool use_virtual = image->using_virtual_resolution;
				Uint16 w = 0, h = 0;
				UnsetColor(image);
				SetBlending(image, 0);
				SetImageFilter(image, FILTER_NEAREST);
				if(use_virtual)
                {
                    w = image->w;
                    h = image->h;
                    UnsetImageVirtualResolution(image);
                }

				Blit(image, NULL, target, image->w / 2, image->h / 2);

				// Restore the saved settings
				SetColor(image, color);
				SetBlending(image, use_blending);
				SetImageFilter(image, filter_mode);
				if(use_virtual)
                {
                    SetImageVirtualResolution(image, w, h);
                }
			}

            // Don't free the target yet (a waste of perf), but let it be freed next time...
            target->refcount--;
        }
        break;
        case FORMAT_LUMINANCE:
        case FORMAT_LUMINANCE_ALPHA:
        case FORMAT_ALPHA:
        case FORMAT_RG:
        // Copy via texture download and upload (slow)
		{
			GLenum internal_format;
			int w;
			int h;
            unsigned char* texture_data = getRawImageData(image);
            if(texture_data == NULL)
            {
                PushErrorCode("CopyImage", ERROR_BACKEND_ERROR, "Failed to get raw texture data.");
                return NULL;
            }

            result = CreateUninitializedImage(image->texture_w, image->texture_h, image->format);
            if(result == NULL)
            {
                SDL_free(texture_data);
                PushErrorCode("CopyImage", ERROR_BACKEND_ERROR, "Failed to create new image.");
                return NULL;
            }

            changeTexturing(1);
            bindTexture(result);

            internal_format = ((ImageData*)(result->data))->format;
            w = result->w;
            h = result->h;
            if(!(_device->enabled_features & FEATURE_NON_POWER_OF_TWO))
            {
                if(!isPowerOfTwo(w))
                    w = getNearestPowerOf2(w);
                if(!isPowerOfTwo(h))
                    h = getNearestPowerOf2(h);
            }

            upload_new_texture(texture_data, MakeRect(0, 0, w, h), internal_format, 1, w, result->bytes_per_pixel);
            
            
            // Tell SDL_gpu what we got.
            result->texture_w = w;
            result->texture_h = h;

            SDL_free(texture_data);
        }
        break;
        default:
            PushErrorCode("CopyImage", ERROR_BACKEND_ERROR, "Could not copy the given image format.");
        break;
    }

    return result;
}

Image* Renderer::CopyImage(Image* image)
{
    Image* result = NULL;

    if(image == NULL)
        return NULL;

    result = gpu_copy_image_pixels_only(image);

    if(result != NULL)
    {
        // Copy the image settings
        SetColor(result, image->color);
        SetBlending(result, image->use_blending);
        result->blend_mode = image->blend_mode;
        SetImageFilter(result, image->filter_mode);
        SetSnapMode(result, image->snap_mode);
        SetWrapMode(result, image->wrap_mode_x, image->wrap_mode_y);
        if(image->has_mipmaps)
            GenerateMipmaps(result);
        if(image->using_virtual_resolution)
            SetImageVirtualResolution(result, image->w, image->h);
    }

    return result;
}

void Renderer::UpdateImage(Image* image, const GPU_Rect* image_rect, SDL_Surface* surface, const GPU_Rect* surface_rect)
{
	ImageData* data;
	GLenum original_format;

	SDL_Surface* newSurface;
	GPU_Rect updateRect;
	GPU_Rect sourceRect;
	int alignment;
	Uint8* pixels;

    if(image == NULL || surface == NULL)
        return;

    data = (ImageData*)image->data;
    original_format = data->format;

    newSurface = copySurfaceIfNeeded(data->format, surface, &original_format);
    if(newSurface == NULL)
    {
        PushErrorCode("UpdateImage", ERROR_BACKEND_ERROR, "Failed to convert surface to proper pixel format.");
        return;
    }

    if(image_rect != NULL)
    {
        updateRect = *image_rect;
        if(updateRect.x < 0)
        {
            updateRect.w += updateRect.x;
            updateRect.x = 0;
        }
        if(updateRect.y < 0)
        {
            updateRect.h += updateRect.y;
            updateRect.y = 0;
        }
        if(updateRect.x + updateRect.w > image->base_w)
            updateRect.w += image->base_w - (updateRect.x + updateRect.w);
        if(updateRect.y + updateRect.h > image->base_h)
            updateRect.h += image->base_h - (updateRect.y + updateRect.h);

        if(updateRect.w <= 0)
            updateRect.w = 0;
        if(updateRect.h <= 0)
            updateRect.h = 0;
    }
    else
    {
        updateRect.x = 0;
        updateRect.y = 0;
        updateRect.w = image->base_w;
        updateRect.h = image->base_h;
        if(updateRect.w < 0.0f || updateRect.h < 0.0f)
        {
            PushErrorCode("UpdateImage", ERROR_USER_ERROR, "Given negative image rectangle.");
            return;
        }
    }

    if(surface_rect != NULL)
    {
        sourceRect = *surface_rect;
        if(sourceRect.x < 0)
        {
            sourceRect.w += sourceRect.x;
            sourceRect.x = 0;
        }
        if(sourceRect.y < 0)
        {
            sourceRect.h += sourceRect.y;
            sourceRect.y = 0;
        }
        if(sourceRect.x + sourceRect.w > newSurface->w)
            sourceRect.w += newSurface->w - (sourceRect.x + sourceRect.w);
        if(sourceRect.y + sourceRect.h > newSurface->h)
            sourceRect.h += newSurface->h - (sourceRect.y + sourceRect.h);

        if(sourceRect.w <= 0)
            sourceRect.w = 0;
        if(sourceRect.h <= 0)
            sourceRect.h = 0;
    }
    else
    {
        sourceRect.x = 0;
        sourceRect.y = 0;
        sourceRect.w = newSurface->w;
        sourceRect.h = newSurface->h;
    }

    changeTexturing(1);
    if(image->target != NULL && isCurrentTarget(image->target))
        FlushBlitBuffer();
    bindTexture(image);
    alignment = 8;
    while(newSurface->pitch % alignment)
        alignment >>= 1;

    // Use the smaller of the image and surface rect dimensions
    if(sourceRect.w < updateRect.w)
        updateRect.w = sourceRect.w;
    if(sourceRect.h < updateRect.h)
        updateRect.h = sourceRect.h;

    pixels = (Uint8*)newSurface->pixels;
    // Shift the pixels pointer to the proper source position
    pixels += (int)(newSurface->pitch * sourceRect.y + (newSurface->format->BytesPerPixel)*sourceRect.x);
    
    upload_texture(pixels, updateRect, original_format, alignment, (newSurface->pitch / newSurface->format->BytesPerPixel), newSurface->pitch);
    
    // Delete temporary surface
    if(surface != newSurface)
        SDL_FreeSurface(newSurface);
}

void Renderer::UpdateImageBytes(Image* image, const GPU_Rect* image_rect, const unsigned char* bytes, int bytes_per_row)
{
	ImageData* data;
	GLenum original_format;

	GPU_Rect updateRect;
	int alignment;

    if(image == NULL || bytes == NULL)
        return;

    data = (ImageData*)image->data;
    original_format = data->format;

    if(image_rect != NULL)
    {
        updateRect = *image_rect;
        if(updateRect.x < 0)
        {
            updateRect.w += updateRect.x;
            updateRect.x = 0;
        }
        if(updateRect.y < 0)
        {
            updateRect.h += updateRect.y;
            updateRect.y = 0;
        }
        if(updateRect.x + updateRect.w > image->base_w)
            updateRect.w += image->base_w - (updateRect.x + updateRect.w);
        if(updateRect.y + updateRect.h > image->base_h)
            updateRect.h += image->base_h - (updateRect.y + updateRect.h);

        if(updateRect.w <= 0)
            updateRect.w = 0;
        if(updateRect.h <= 0)
            updateRect.h = 0;
    }
    else
    {
        updateRect.x = 0;
        updateRect.y = 0;
        updateRect.w = image->base_w;
        updateRect.h = image->base_h;
        if(updateRect.w < 0.0f || updateRect.h < 0.0f)
        {
            PushErrorCode("UpdateImage", ERROR_USER_ERROR, "Given negative image rectangle.");
            return;
        }
    }


    changeTexturing(1);
    if(image->target != NULL && isCurrentTarget(image->target))
        FlushBlitBuffer();
    bindTexture(image);
    alignment = 8;
    while(bytes_per_row % alignment)
        alignment >>= 1;
    
    upload_texture(bytes, updateRect, original_format, alignment, (bytes_per_row / image->bytes_per_pixel), bytes_per_row);    
}

bool Renderer::ReplaceImage(Image* image, SDL_Surface* surface, const GPU_Rect* surface_rect)
{
	ImageData* data;
	GPU_Rect sourceRect;
	SDL_Surface* newSurface;
	GLenum internal_format;
	Uint8* pixels;
	int w, h;
	int alignment;

    if(image == NULL)
    {
        PushErrorCode("ReplaceImage", ERROR_NULL_ARGUMENT, "image");
        return false;
    }

    if(surface == NULL)
    {
        PushErrorCode("ReplaceImage", ERROR_NULL_ARGUMENT, "surface");
        return false;
    }

    data = (ImageData*)image->data;
    internal_format = data->format;

    newSurface = copySurfaceIfNeeded(internal_format, surface, &internal_format);
    if(newSurface == NULL)
    {
        PushErrorCode("ReplaceImage", ERROR_BACKEND_ERROR, "Failed to convert surface to proper pixel format.");
        return false;
    }

    // Free the attached framebuffer
    if((_device->enabled_features & FEATURE_RENDER_TARGETS) && image->target != NULL)
    {
        TargetData* tdata = (TargetData*)image->target->data;
        if(_device->current_context_target != NULL)
            flushAndClearBlitBufferIfCurrentFramebuffer(image->target);
        if(tdata->handle != 0)
            glDeleteFramebuffers(1, &tdata->handle);
        tdata->handle = 0;
    }

    // Free the old texture
    if(data->owns_handle)
        glDeleteTextures( 1, &data->handle);
    data->handle = 0;

    // Get the area of the surface we'll use
    if(surface_rect == NULL)
    {
        sourceRect.x = 0;
        sourceRect.y = 0;
        sourceRect.w = surface->w;
        sourceRect.h = surface->h;
    }
    else
        sourceRect = *surface_rect;

    // Clip the source rect to the surface
    if(sourceRect.x < 0)
    {
        sourceRect.w += sourceRect.x;
        sourceRect.x = 0;
    }
    if(sourceRect.y < 0)
    {
        sourceRect.h += sourceRect.y;
        sourceRect.y = 0;
    }
    if(sourceRect.x >= surface->w)
        sourceRect.x = surface->w - 1;
    if(sourceRect.y >= surface->h)
        sourceRect.y = surface->h - 1;

    if(sourceRect.x + sourceRect.w > surface->w)
        sourceRect.w = surface->w - sourceRect.x;
    if(sourceRect.y + sourceRect.h > surface->h)
        sourceRect.h = surface->h - sourceRect.y;

    if(sourceRect.w <= 0 || sourceRect.h <= 0)
    {
        PushErrorCode("ReplaceImage", ERROR_DATA_ERROR, "Clipped source rect has zero size.");
        return false;
    }

    // Allocate new texture
    data->handle = CreateUninitializedTexture();
    data->owns_handle = 1;
    if(data->handle == 0)
    {
        PushErrorCode("ReplaceImage", ERROR_BACKEND_ERROR, "Failed to create a new texture handle.");
        return false;
    }

    // Update image members
    w = sourceRect.w;
    h = sourceRect.h;

    if(!image->using_virtual_resolution)
    {
        image->w = w;
        image->h = h;
    }
    image->base_w = w;
    image->base_h = h;

    if(!(_device->enabled_features & FEATURE_NON_POWER_OF_TWO))
    {
        if(!isPowerOfTwo(w))
            w = getNearestPowerOf2(w);
        if(!isPowerOfTwo(h))
            h = getNearestPowerOf2(h);
    }
    image->texture_w = w;
    image->texture_h = h;

    image->has_mipmaps = false;

    // Upload surface pixel data
    alignment = 8;
    while(newSurface->pitch % alignment)
        alignment >>= 1;

    pixels = (Uint8*)newSurface->pixels;
    // Shift the pixels pointer to the proper source position
    pixels += (int)(newSurface->pitch * sourceRect.y + (newSurface->format->BytesPerPixel)*sourceRect.x);

    upload_new_texture(pixels, MakeRect(0, 0, w, h), internal_format, alignment, (newSurface->pitch / newSurface->format->BytesPerPixel), newSurface->format->BytesPerPixel);
    
    // Delete temporary surface
    if(surface != newSurface)
        SDL_FreeSurface(newSurface);

    // Update target members
    if((_device->enabled_features & FEATURE_RENDER_TARGETS) && image->target != NULL)
    {
        GLenum status;
        Target* target = image->target;
        TargetData* tdata = (TargetData*)target->data;

        // Create framebuffer object
        glGenFramebuffers(1, &tdata->handle);
        if(tdata->handle == 0)
        {
            PushErrorCode("ReplaceImage", ERROR_BACKEND_ERROR, "Failed to create new framebuffer target.");
            return false;
        }

        flushAndBindFramebuffer(tdata->handle);

        // Attach the texture to it
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, data->handle, 0);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            PushErrorCode("ReplaceImage", ERROR_BACKEND_ERROR, "Failed to recreate framebuffer target.");
            return false;
        }

        if(!target->using_virtual_resolution)
        {
            target->w = image->base_w;
            target->h = image->base_h;
        }
        target->base_w = image->texture_w;
        target->base_h = image->texture_h;

        // Reset viewport?
        target->viewport = MakeRect(0, 0, target->w, target->h);
    }

    return true;
}

static_inline Uint32 getPixel(SDL_Surface *Surface, int x, int y)
{
    Uint8* bits;
    Uint32 bpp;

    if(x < 0 || x >= Surface->w)
        return 0;  // Best I could do for errors

    bpp = Surface->format->BytesPerPixel;
    bits = ((Uint8*)Surface->pixels) + y*Surface->pitch + x*bpp;

    switch (bpp)
    {
    case 1:
        return *((Uint8*)Surface->pixels + y * Surface->pitch + x);
        break;
    case 2:
        return *((Uint16*)Surface->pixels + y * Surface->pitch/2 + x);
        break;
    case 3:
        // Endian-correct, but slower
    {
        Uint8 r, g, b;
        r = *((bits)+Surface->format->Rshift/8);
        g = *((bits)+Surface->format->Gshift/8);
        b = *((bits)+Surface->format->Bshift/8);
        return SDL_MapRGB(Surface->format, r, g, b);
    }
    break;
    case 4:
        return *((Uint32*)Surface->pixels + y * Surface->pitch/4 + x);
        break;
    }

    return 0;  // FIXME: Handle errors better
}

Image* Renderer::CopyImageFromSurface(SDL_Surface* surface)
{
    FormatEnum format;
	Image* image;

    if(surface == NULL)
    {
        PushErrorCode("CopyImageFromSurface", ERROR_NULL_ARGUMENT, "surface");
        return NULL;
    }

    if(surface->w == 0 || surface->h == 0)
    {
        PushErrorCode("CopyImageFromSurface", ERROR_DATA_ERROR, "Surface has a zero dimension.");
        return NULL;
    }

    // See what the best image format is.
    if(surface->format->Amask == 0)
    {
        if(has_colorkey(surface))
            format = FORMAT_RGBA;
        else
            format = FORMAT_RGB;
    }
    else
        format = FORMAT_RGBA;

    image = CreateImage(surface->w, surface->h, format);
    if(image == NULL)
        return NULL;

    UpdateImage(image, NULL, surface, NULL);

    return image;
}

Image* Renderer::CopyImageFromTarget(Target* target)
{
	Image* result;

    if(target == NULL)
        return NULL;
    
    if(target->image != NULL)
    {
        result = gpu_copy_image_pixels_only(target->image);
    }
    else
    {
        SDL_Surface* surface = CopySurfaceFromTarget(target);
        result = CopyImageFromSurface(surface);
        SDL_FreeSurface(surface);
    }

    return result;
}

void Renderer::FreeImage(Image* image)
{
	ImageData* data;

    if(image == NULL)
        return;

    if(image->refcount > 1)
    {
        image->refcount--;
        return;
    }

    // Delete the attached target first
    if(image->target != NULL)
    {
        Target* target = image->target;
        image->target = NULL;
        FreeTarget(target);
    }

    flushAndClearBlitBufferIfCurrentTexture(image);

    // Does the _device data need to be freed too?
    data = (ImageData*)image->data;
    if(data->refcount > 1)
    {
        data->refcount--;
    }
    else
    {
        if(data->owns_handle)
        {
            MakeCurrent(image->context_target, image->context_target->context->windowID);
            glDeleteTextures( 1, &data->handle);
        }
        SDL_free(data);
    }

    SDL_free(image);
}

Target* Renderer::LoadTarget(Image* image)
{
    GLuint handle;
	GLenum status;
	Target* result;
	TargetData* data;

    if(image == NULL)
        return NULL;

    if(image->target != NULL)
    {
        image->target->refcount++;
        ((TargetData*)image->target->data)->refcount++;
        return image->target;
    }

    if(!(_device->enabled_features & FEATURE_RENDER_TARGETS))
        return NULL;

    // Create framebuffer object
    glGenFramebuffers(1, &handle);
    flushAndBindFramebuffer(handle);

    // Attach the texture to it
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ((ImageData*)image->data)->handle, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
        return NULL;

    result = (Target*)SDL_malloc(sizeof(Target));
    memset(result, 0, sizeof(Target));
    result->refcount = 1;
    data = (TargetData*)SDL_malloc(sizeof(TargetData));
    data->refcount = 1;
    result->data = data;
    data->handle = handle;
    data->format = ((ImageData*)image->data)->format;

    result->renderer = _device;
    result->context_target = _device->current_context_target;
    result->context = NULL;
    result->image = image;
    result->w = image->w;
    result->h = image->h;
    result->base_w = image->texture_w;
    result->base_h = image->texture_h;
    result->using_virtual_resolution = image->using_virtual_resolution;

    result->viewport = MakeRect(0, 0, result->w, result->h);

    result->camera = GetDefaultCamera();
    result->use_camera = true;

    result->use_clip_rect = false;
    result->clip_rect.x = 0;
    result->clip_rect.y = 0;
    result->clip_rect.w = result->w;
    result->clip_rect.h = result->h;
    result->use_color = false;

    image->target = result;
    return result;
}

void Renderer::FreeTarget(Target* target)
{
	TargetData* data;

    if(target == NULL)
        return;

    if(target->refcount > 1)
    {
        target->refcount--;
        return;
    }

    if(target->context != NULL && target->context->failed)
    {
        ContextData* cdata = (ContextData*)target->context->data;

        if(target == _device->current_context_target)
            _device->current_context_target = NULL;

        SDL_free(cdata->blit_buffer);
        SDL_free(cdata->index_buffer);

        if(target->context->context != 0)
            SDL_GL_DeleteContext(target->context->context);

        // Remove all of the window mappings that refer to this target
        RemoveWindowMappingByTarget(target);

        SDL_free(target->context->data);
        SDL_free(target->context);

        // Does the _device data need to be freed too?
        data = ((TargetData*)target->data);

        SDL_free(data);
        SDL_free(target);

        return;
    }

    if(target == _device->current_context_target)
    {
        FlushBlitBuffer();
        _device->current_context_target = NULL;
    }
    else
        MakeCurrent(target->context_target, target->context_target->context->windowID);

    if(!target->is_alias && target->image != NULL)
        target->image->target = NULL;  // Remove reference to this object


    // Does the _device data need to be freed too?
    data = ((TargetData*)target->data);
    if(data->refcount > 1)
    {
        data->refcount--;
        SDL_free(target);
        return;
    }

    if(_device->enabled_features & FEATURE_RENDER_TARGETS)
    {
        if(_device->current_context_target != NULL)
            flushAndClearBlitBufferIfCurrentFramebuffer(target);
        if(data->handle != 0)
            glDeleteFramebuffers(1, &data->handle);
    }

    if(target->context != NULL)
    {
        ContextData* cdata = (ContextData*)target->context->data;

        SDL_free(cdata->blit_buffer);
        SDL_free(cdata->index_buffer);


		glDeleteBuffers(2, cdata->blit_VBO);
		glDeleteBuffers(1, &cdata->blit_IBO);
		glDeleteBuffers(16, cdata->attribute_VBO);

        if(target->context->context != 0)
            SDL_GL_DeleteContext(target->context->context);

        // Remove all of the window mappings that refer to this target
        RemoveWindowMappingByTarget(target);

        SDL_free(target->context->data);
        SDL_free(target->context);
        target->context = NULL;
    }

    SDL_free(data);
    SDL_free(target);
}

#define SET_TEXTURED_VERTEX(x, y, s, t, r, g, b, a) \
    blit_buffer[vert_index] = x; \
    blit_buffer[vert_index+1] = y; \
    blit_buffer[tex_index] = s; \
    blit_buffer[tex_index+1] = t; \
    blit_buffer[color_index] = r; \
    blit_buffer[color_index+1] = g; \
    blit_buffer[color_index+2] = b; \
    blit_buffer[color_index+3] = a; \
    index_buffer[cdata->index_buffer_num_vertices++] = cdata->blit_buffer_num_vertices++; \
    vert_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    tex_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index += BLIT_BUFFER_FLOATS_PER_VERTEX;

#define SET_TEXTURED_VERTEX_UNINDEXED(x, y, s, t, r, g, b, a) \
    blit_buffer[vert_index] = x; \
    blit_buffer[vert_index+1] = y; \
    blit_buffer[tex_index] = s; \
    blit_buffer[tex_index+1] = t; \
    blit_buffer[color_index] = r; \
    blit_buffer[color_index+1] = g; \
    blit_buffer[color_index+2] = b; \
    blit_buffer[color_index+3] = a; \
    vert_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    tex_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index += BLIT_BUFFER_FLOATS_PER_VERTEX;

#define SET_UNTEXTURED_VERTEX(x, y, r, g, b, a) \
    blit_buffer[vert_index] = x; \
    blit_buffer[vert_index+1] = y; \
    blit_buffer[color_index] = r; \
    blit_buffer[color_index+1] = g; \
    blit_buffer[color_index+2] = b; \
    blit_buffer[color_index+3] = a; \
    index_buffer[cdata->index_buffer_num_vertices++] = cdata->blit_buffer_num_vertices++; \
    vert_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index += BLIT_BUFFER_FLOATS_PER_VERTEX;

#define SET_UNTEXTURED_VERTEX_UNINDEXED(x, y, r, g, b, a) \
    blit_buffer[vert_index] = x; \
    blit_buffer[vert_index+1] = y; \
    blit_buffer[color_index] = r; \
    blit_buffer[color_index+1] = g; \
    blit_buffer[color_index+2] = b; \
    blit_buffer[color_index+3] = a; \
    vert_index += BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index += BLIT_BUFFER_FLOATS_PER_VERTEX;

#define SET_INDEXED_VERTEX(offset) \
    index_buffer[cdata->index_buffer_num_vertices++] = blit_buffer_starting_index + (offset);

#define SET_RELATIVE_INDEXED_VERTEX(offset) \
    index_buffer[cdata->index_buffer_num_vertices++] = cdata->blit_buffer_num_vertices + (offset);


#define BEGIN_UNTEXTURED_SEGMENTS(x1, y1, x2, y2, r, g, b, a) \
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a); \
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);

// Finish previous triangles and start the next one
#define SET_UNTEXTURED_SEGMENTS(x1, y1, x2, y2, r, g, b, a) \
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a); \
    SET_RELATIVE_INDEXED_VERTEX(-2); \
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a); \
    SET_RELATIVE_INDEXED_VERTEX(-2); \
    SET_RELATIVE_INDEXED_VERTEX(-2); \
    SET_RELATIVE_INDEXED_VERTEX(-1);

// Finish previous triangles
#define LOOP_UNTEXTURED_SEGMENTS() \
    SET_INDEXED_VERTEX(0); \
    SET_RELATIVE_INDEXED_VERTEX(-1); \
    SET_INDEXED_VERTEX(1); \
    SET_INDEXED_VERTEX(0);

#define END_UNTEXTURED_SEGMENTS(x1, y1, x2, y2, r, g, b, a) \
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a); \
    SET_RELATIVE_INDEXED_VERTEX(-2); \
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a); \
    SET_RELATIVE_INDEXED_VERTEX(-2);



void Renderer::Blit(Image* image, GPU_Rect* src_rect, Target* target, float x, float y)
{
	Uint32 tex_w, tex_h;
	float w;
	float h;
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index;
	int tex_index;
	int color_index;
	float r, g, b, a;

    if(image == NULL)
    {
        PushErrorCode("Blit", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(target == NULL)
    {
        PushErrorCode("Blit", ERROR_NULL_ARGUMENT, "target");
        return;
    }
    if(_device != image->renderer || _device != target->renderer)
    {
        PushErrorCode("Blit", ERROR_USER_ERROR, "Mismatched _device");
        return;
    }

    makeContextCurrent(target);
    if(_device->current_context_target == NULL)
    {
        PushErrorCode("Blit", ERROR_USER_ERROR, "NULL context");
        return;
    }

    prepareToRenderToTarget(target);
    prepareToRenderImage(target, image);

    // Bind the texture to which subsequent calls refer
    bindTexture(image);

    // Bind the FBO
    if(!bindFramebuffer(target))
    {
        PushErrorCode("Blit", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
        return;
    }

    tex_w = image->texture_w;
    tex_h = image->texture_h;

    if(image->snap_mode == SNAP_POSITION || image->snap_mode == SNAP_POSITION_AND_DIMENSIONS)
    {
        // Avoid rounding errors in texture sampling by insisting on integral pixel positions
        x = floorf(x);
        y = floorf(y);
    }

    if(src_rect == NULL)
    {
        // Scale tex coords according to actual texture dims
        x1 = 0.0f;
        y1 = 0.0f;
        x2 = ((float)image->w)/tex_w;
        y2 = ((float)image->h)/tex_h;
        w = image->w;
        h = image->h;
    }
    else
    {
        // Scale src_rect tex coords according to actual texture dims
        x1 = src_rect->x/(float)tex_w;
        y1 = src_rect->y/(float)tex_h;
        x2 = (src_rect->x + src_rect->w)/(float)tex_w;
        y2 = (src_rect->y + src_rect->h)/(float)tex_h;
        w = src_rect->w;
        h = src_rect->h;
    }

    if(image->using_virtual_resolution)
    {
        // Scale texture coords to fit the original dims
        x1 *= image->base_w/(float)image->w;
        y1 *= image->base_h/(float)image->h;
        x2 *= image->base_w/(float)image->w;
        y2 *= image->base_h/(float)image->h;
    }

    // Center the image on the given coords
    dx1 = x - w * image->anchor_x;
    dy1 = y - h * image->anchor_y;
    dx2 = x + w * (1.0f - image->anchor_x);
    dy2 = y + h * (1.0f - image->anchor_y);

    if(image->snap_mode == SNAP_DIMENSIONS || image->snap_mode == SNAP_POSITION_AND_DIMENSIONS)
    {
        float fractional;
        fractional = w/2.0f - floorf(w/2.0f);
        dx1 += fractional;
        dx2 += fractional;
        fractional = h/2.0f - floorf(h/2.0f);
        dy1 += fractional;
        dy2 += fractional;
    }

    cdata = (ContextData*)_device->current_context_target->context->data;

    if(cdata->blit_buffer_num_vertices + 4 >= cdata->blit_buffer_max_num_vertices)
    {
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + 4))
            FlushBlitBuffer();
    }
    if(cdata->index_buffer_num_vertices + 6 >= cdata->index_buffer_max_num_vertices)
    {
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + 6))
            FlushBlitBuffer();
    }

    blit_buffer = cdata->blit_buffer;
    index_buffer = cdata->index_buffer;

    blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

    vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
    tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
    color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
    if(target->use_color)
    {
        r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
        g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
        b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
        a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
    }
    else
    {
        r = image->color.r/255.0f;
        g = image->color.g/255.0f;
        b = image->color.b/255.0f;
        a = image->color.a/255.0f;
    }
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

    // 4 Quad vertices
    SET_TEXTURED_VERTEX_UNINDEXED(dx1, dy1, x1, y1, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx2, dy1, x2, y1, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx2, dy2, x2, y2, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx1, dy2, x1, y2, r, g, b, a);

    // 6 Triangle indices
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(1);
    SET_INDEXED_VERTEX(2);

    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(2);
    SET_INDEXED_VERTEX(3);

    cdata->blit_buffer_num_vertices += BLIT_BUFFER_VERTICES_PER_SPRITE;
}

void Renderer::BlitRotate(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees)
{
	float w, h;
    if(image == NULL)
    {
        PushErrorCode("BlitRotate", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(target == NULL)
    {
        PushErrorCode("BlitRotate", ERROR_NULL_ARGUMENT, "target");
        return;
    }

    w = (src_rect == NULL? image->w : src_rect->w);
    h = (src_rect == NULL? image->h : src_rect->h);
    BlitTransformX(image, src_rect, target, x, y, w*image->anchor_x, h*image->anchor_y, degrees, 1.0f, 1.0f);
}

void Renderer::BlitScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float scaleX, float scaleY)
{
	float w, h;
    if(image == NULL)
    {
        PushErrorCode("BlitScale", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(target == NULL)
    {
        PushErrorCode("BlitScale", ERROR_NULL_ARGUMENT, "target");
        return;
    }

    w = (src_rect == NULL? image->w : src_rect->w);
    h = (src_rect == NULL? image->h : src_rect->h);
    BlitTransformX(image, src_rect, target, x, y, w*image->anchor_x, h*image->anchor_y, 0.0f, scaleX, scaleY);
}

void Renderer::BlitTransform(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees, float scaleX, float scaleY)
{
	float w, h;
    if(image == NULL)
    {
        PushErrorCode("BlitTransform", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(target == NULL)
    {
        PushErrorCode("BlitTransform", ERROR_NULL_ARGUMENT, "target");
        return;
    }

    w = (src_rect == NULL? image->w : src_rect->w);
    h = (src_rect == NULL? image->h : src_rect->h);
    BlitTransformX(image, src_rect, target, x, y, w*image->anchor_x, h*image->anchor_y, degrees, scaleX, scaleY);
}

void Renderer::BlitTransformX(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float pivot_x, float pivot_y, float degrees, float scaleX, float scaleY)
{
	Uint32 tex_w, tex_h;
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;
	float w, h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index;
	int tex_index;
	int color_index;
	float r, g, b, a;

    if(image == NULL)
    {
        PushErrorCode("BlitTransformX", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(target == NULL)
    {
        PushErrorCode("BlitTransformX", ERROR_NULL_ARGUMENT, "target");
        return;
    }
    if(_device != image->renderer || _device != target->renderer)
    {
        PushErrorCode("BlitTransformX", ERROR_USER_ERROR, "Mismatched _device");
        return;
    }

    makeContextCurrent(target);

    prepareToRenderToTarget(target);
    prepareToRenderImage(target, image);

    // Bind the texture to which subsequent calls refer
    bindTexture(image);

    // Bind the FBO
    if(!bindFramebuffer(target))
    {
        PushErrorCode("BlitTransformX", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
        return;
    }

    tex_w = image->texture_w;
    tex_h = image->texture_h;

    if(image->snap_mode == SNAP_POSITION || image->snap_mode == SNAP_POSITION_AND_DIMENSIONS)
    {
        // Avoid rounding errors in texture sampling by insisting on integral pixel positions
        x = floorf(x);
        y = floorf(y);
    }

    /*
        1,1 --- 3,3
         |       |
         |       |
        4,4 --- 2,2
    */
    if(src_rect == NULL)
    {
        // Scale tex coords according to actual texture dims
        x1 = 0.0f;
        y1 = 0.0f;
        x2 = ((float)image->w)/tex_w;
        y2 = ((float)image->h)/tex_h;
        w = image->w;
        h = image->h;
    }
    else
    {
        // Scale src_rect tex coords according to actual texture dims
        x1 = src_rect->x/(float)tex_w;
        y1 = src_rect->y/(float)tex_h;
        x2 = (src_rect->x + src_rect->w)/(float)tex_w;
        y2 = (src_rect->y + src_rect->h)/(float)tex_h;
        w = src_rect->w;
        h = src_rect->h;
    }

    if(image->using_virtual_resolution)
    {
        // Scale texture coords to fit the original dims
        x1 *= image->base_w/(float)image->w;
        y1 *= image->base_h/(float)image->h;
        x2 *= image->base_w/(float)image->w;
        y2 *= image->base_h/(float)image->h;
    }

    // Create vertices about the anchor
    dx1 = -pivot_x;
    dy1 = -pivot_y;
    dx2 = w - pivot_x;
    dy2 = h - pivot_y;

    if(image->snap_mode == SNAP_DIMENSIONS || image->snap_mode == SNAP_POSITION_AND_DIMENSIONS)
    {
        // This is a little weird for rotating sprites, but oh well.
        float fractional;
        fractional = w/2.0f - floorf(w/2.0f);
        dx1 += fractional;
        dx2 += fractional;
        fractional = h/2.0f - floorf(h/2.0f);
        dy1 += fractional;
        dy2 += fractional;
    }

    // Apply transforms

    // Scale about the anchor
    if(scaleX != 1.0f || scaleY != 1.0f)
    {
        dx1 *= scaleX;
        dy1 *= scaleY;
        dx2 *= scaleX;
        dy2 *= scaleY;
    }

    // Get extra vertices for rotation
    dx3 = dx2;
    dy3 = dy1;
    dx4 = dx1;
    dy4 = dy2;

    // Rotate about the anchor
    if (degrees != 0.0f)
    {
		float angle = degrees * M_PI / 180;
        float cosA = cos(angle);
        float sinA = sin(angle);
        float tempX = dx1;
        dx1 = dx1*cosA - dy1*sinA;
        dy1 = tempX*sinA + dy1*cosA;
        tempX = dx2;
        dx2 = dx2*cosA - dy2*sinA;
        dy2 = tempX*sinA + dy2*cosA;
        tempX = dx3;
        dx3 = dx3*cosA - dy3*sinA;
        dy3 = tempX*sinA + dy3*cosA;
        tempX = dx4;
        dx4 = dx4*cosA - dy4*sinA;
        dy4 = tempX*sinA + dy4*cosA;
    }

    // Translate to final position
    dx1 += x;
    dx2 += x;
    dx3 += x;
    dx4 += x;
    dy1 += y;
    dy2 += y;
    dy3 += y;
    dy4 += y;

    cdata = (ContextData*)_device->current_context_target->context->data;

    if(cdata->blit_buffer_num_vertices + 4 >= cdata->blit_buffer_max_num_vertices)
    {
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + 4))
            FlushBlitBuffer();
    }
    if(cdata->index_buffer_num_vertices + 6 >= cdata->index_buffer_max_num_vertices)
    {
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + 6))
            FlushBlitBuffer();
    }

    blit_buffer = cdata->blit_buffer;
    index_buffer = cdata->index_buffer;

    blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

    vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
    tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
    color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;

    if(target->use_color)
    {
        r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
        g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
        b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
        a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
    }
    else
    {
        r = image->color.r/255.0f;
        g = image->color.g/255.0f;
        b = image->color.b/255.0f;
        a = image->color.a/255.0f;
    }
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

    // 4 Quad vertices
    SET_TEXTURED_VERTEX_UNINDEXED(dx1, dy1, x1, y1, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx3, dy3, x2, y1, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx2, dy2, x2, y2, r, g, b, a);
    SET_TEXTURED_VERTEX_UNINDEXED(dx4, dy4, x1, y2, r, g, b, a);

    // 6 Triangle indices
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(1);
    SET_INDEXED_VERTEX(2);

    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(2);
    SET_INDEXED_VERTEX(3);

    cdata->blit_buffer_num_vertices += BLIT_BUFFER_VERTICES_PER_SPRITE;
}


// BlitTransformA do NOT care snap mode!!!
// AffineTransform is about (0, 0)
void Renderer::BlitTransformA(Image* image, GPU_Rect* src_rect, Target* target,
	float x, float y, AffineTransform* transform, float scaleX, float scaleY)
{
	Uint32 tex_w, tex_h;
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;
	float w, h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index;
	int tex_index;
	int color_index;
	float r, g, b, a;

	if (image == NULL) {
		PushErrorCode("BlitTransformA", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("BlitTransformA", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("BlitTransformA", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}

	makeContextCurrent(target);
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);

	// Bind the texture to which subsequent calls refer
	bindTexture(image);

	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("BlitTransformA", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}

	tex_w = image->texture_w;
	tex_h = image->texture_h;

	/*
	1,1 --- 3,3
	|       |
	|       |
	4,4 --- 2,2
	*/
	if (src_rect == NULL) {
		// Scale tex coords according to actual texture dims
		x1 = 0.0f;
		y1 = 0.0f;
		x2 = ((float)image->w) / tex_w;
		y2 = ((float)image->h) / tex_h;
		w = image->w;
		h = image->h;
	}
	else {
		// Scale src_rect tex coords according to actual texture dims
		x1 = src_rect->x / (float)tex_w;
		y1 = src_rect->y / (float)tex_h;
		x2 = (src_rect->x + src_rect->w) / (float)tex_w;
		y2 = (src_rect->y + src_rect->h) / (float)tex_h;
		w = src_rect->w;
		h = src_rect->h;
	}

	if (image->using_virtual_resolution) {
		// Scale texture coords to fit the original dims
		x1 *= image->base_w / (float)image->w;
		y1 *= image->base_h / (float)image->h;
		x2 *= image->base_w / (float)image->w;
		y2 *= image->base_h / (float)image->h;
	}

	// Create vertices about the anchor
	if (image->anchor_fixed) { // just hacks, NOT use it with virtual resolution
		dx1 = -image->anchor_x * scaleX + x;
		dy1 = -image->anchor_y * scaleY + y;
		dx2 = (w-image->anchor_x) * scaleX + x;
		dy2 = (h-image->anchor_y) * scaleY + y;
	}
	else {
		dx1 = -w * image->anchor_x * scaleX + x;
		dy1 = -h * image->anchor_y * scaleY + y;
		dx2 = (w - w * image->anchor_x) * scaleX + x;
		dy2 = (h - h * image->anchor_y) * scaleY + y;
	}

	dx3 = dx2;
	dy3 = dy1;
	dx4 = dx1;
	dy4 = dy2;

	// Apply transforms
	GPU_Point p1 = PointApplyAffineTransform(dx1, dy1, transform);
	GPU_Point p2 = PointApplyAffineTransform(dx2, dy2, transform);
	GPU_Point p3 = PointApplyAffineTransform(dx3, dy3, transform);
	GPU_Point p4 = PointApplyAffineTransform(dx4, dy4, transform);

	cdata = (ContextData*)_device->current_context_target->context->data;

	if (cdata->blit_buffer_num_vertices + 4 >= cdata->blit_buffer_max_num_vertices)
	{
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + 4))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + 6 >= cdata->index_buffer_max_num_vertices)
	{
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + 6))
			FlushBlitBuffer();
	}

	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;

	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;

	if (target->use_color)
	{
		r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
		g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
		b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
		a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
	}
	else
	{
		r = image->color.r / 255.0f;
		g = image->color.g / 255.0f;
		b = image->color.b / 255.0f;
		a = image->color.a / 255.0f;
	}
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	// 4 Quad vertices
	SET_TEXTURED_VERTEX_UNINDEXED(p1.x, p1.y, x1, y1, r, g, b, a);
	SET_TEXTURED_VERTEX_UNINDEXED(p3.x, p3.y, x2, y1, r, g, b, a);
	SET_TEXTURED_VERTEX_UNINDEXED(p2.x, p2.y, x2, y2, r, g, b, a);
	SET_TEXTURED_VERTEX_UNINDEXED(p4.x, p4.y, x1, y2, r, g, b, a);

	// 6 Triangle indices
	SET_INDEXED_VERTEX(0);
	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);

	SET_INDEXED_VERTEX(0);
	SET_INDEXED_VERTEX(2);
	SET_INDEXED_VERTEX(3);

	cdata->blit_buffer_num_vertices += BLIT_BUFFER_VERTICES_PER_SPRITE;
}

// AffineTransform is about (0, 0)
// Colors inputed with clockwise order
void Renderer::BlitTransformAColor(Image* image, Target* target,
	float x, float y, AffineTransform* transform, GPU_Color colors[4])
{
	Uint32 tex_w, tex_h;
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;
	float w, h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index, tex_index, color_index;
	int i;	

	if (image == NULL) {
		PushErrorCode("BlitTransformAColor", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("BlitTransformAColor", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("BlitTransformAColor", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}

	makeContextCurrent(target);
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);

	// Bind the texture to which subsequent calls refer
	bindTexture(image);

	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("BlitTransformAColor", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}

	tex_w = image->texture_w;
	tex_h = image->texture_h;
	/*
	1,1 --- 3,3
	|       |
	|       |
	4,4 --- 2,2
	*/
	// Scale tex coords according to actual texture dims
	x1 = 0.0f;
	y1 = 0.0f;
	x2 = ((float)image->w) / tex_w;
	y2 = ((float)image->h) / tex_h;
	w = image->w;
	h = image->h;

	if (image->using_virtual_resolution) {
		// Scale texture coords to fit the original dims
		x1 *= image->base_w / (float)image->w;
		y1 *= image->base_h / (float)image->h;
		x2 *= image->base_w / (float)image->w;
		y2 *= image->base_h / (float)image->h;
	}

	// Create vertices about the anchor
	if (image->anchor_fixed) { // just hacks, NOT use it with virtual resolution
		dx1 = -image->anchor_x + x;
		dy1 = -image->anchor_y + y;
		dx2 = (w - image->anchor_x) + x;
		dy2 = (h - image->anchor_y) + y;
	}
	else {
		dx1 = -w * image->anchor_x + x;
		dy1 = -h * image->anchor_y + y;
		dx2 = (w - w * image->anchor_x) + x;
		dy2 = (h - h * image->anchor_y) + y;
	}
	dx3 = dx2;
	dy3 = dy1;
	dx4 = dx1;
	dy4 = dy2;

	// Apply transforms
	GPU_Point p1 = PointApplyAffineTransform(dx1, dy1, transform);
	GPU_Point p2 = PointApplyAffineTransform(dx2, dy2, transform);
	GPU_Point p3 = PointApplyAffineTransform(dx3, dy3, transform);
	GPU_Point p4 = PointApplyAffineTransform(dx4, dy4, transform);

	cdata = (ContextData*)_device->current_context_target->context->data;

	if (cdata->blit_buffer_num_vertices + 4 >= cdata->blit_buffer_max_num_vertices)
	{
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + 4))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + 6 >= cdata->index_buffer_max_num_vertices)
	{
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + 6))
			FlushBlitBuffer();
	}

	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;

	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	
	GPU_Color vert_colors[4];
	for (i = 0; i < 4; ++i) {
		if (target->use_color) {
			vert_colors[i].r = target->color.r / 255.0f * colors[i].r;
			vert_colors[i].g = target->color.g / 255.0f * colors[i].g;
			vert_colors[i].b = target->color.b / 255.0f * colors[i].b;
			vert_colors[i].a = target->color.a / 255.0f * colors[i].a;
		}
		else {
			vert_colors[i] = colors[i];
		}
#ifdef PREMULTIPLIED_ALPHA
		vert_colors[i].r *= vert_colors[i].a; vert_colors[i].g *= vert_colors[i].a; vert_colors[i].b *= vert_colors[i].a;
#endif
	}	

	// 4 Quad vertices
	SET_TEXTURED_VERTEX_UNINDEXED(p1.x, p1.y, x1, y1, vert_colors[0].r, vert_colors[0].g, vert_colors[0].b, vert_colors[0].a);
	SET_TEXTURED_VERTEX_UNINDEXED(p3.x, p3.y, x2, y1, vert_colors[1].r, vert_colors[1].g, vert_colors[1].b, vert_colors[1].a);
	SET_TEXTURED_VERTEX_UNINDEXED(p2.x, p2.y, x2, y2, vert_colors[2].r, vert_colors[2].g, vert_colors[2].b, vert_colors[2].a);
	SET_TEXTURED_VERTEX_UNINDEXED(p4.x, p4.y, x1, y2, vert_colors[3].r, vert_colors[3].g, vert_colors[3].b, vert_colors[3].a);

	// 6 Triangle indices
	SET_INDEXED_VERTEX(0);
	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	SET_INDEXED_VERTEX(0);
	SET_INDEXED_VERTEX(2);
	SET_INDEXED_VERTEX(3);
	cdata->blit_buffer_num_vertices += BLIT_BUFFER_VERTICES_PER_SPRITE;
}


static_inline int sizeof_type(TypeEnum type)
{
    if(type == TYPE_DOUBLE) return sizeof(double);
    if(type == TYPE_FLOAT) return sizeof(float);
    if(type == TYPE_INT) return sizeof(int);
    if(type == TYPE_UNSIGNED_INT) return sizeof(unsigned int);
    if(type == TYPE_SHORT) return sizeof(short);
    if(type == TYPE_UNSIGNED_SHORT) return sizeof(unsigned short);
    if(type == TYPE_BYTE) return sizeof(char);
    if(type == TYPE_UNSIGNED_BYTE) return sizeof(unsigned char);
    return 0;
}

static void refresh_attribute_data(ContextData* cdata)
{
    int i;
    for(i = 0; i < 16; i++)
    {
        AttributeSource* a = &cdata->shader_attributes[i];
        if(a->attribute.values != NULL && a->attribute.location >= 0 && a->num_values > 0 && a->attribute.format.is_per_sprite)
        {
            // Expand the values to 4 vertices
            int n;
            void* storage_ptr = a->per_vertex_storage;
            void* values_ptr = (void*)((char*)a->attribute.values + a->attribute.format.offset_bytes);
            int value_size_bytes = a->attribute.format.num_elems_per_value * sizeof_type(a->attribute.format.type);
            for(n = 0; n < a->num_values; n+=4)
            {
                memcpy(storage_ptr, values_ptr, value_size_bytes);
                storage_ptr = (void*)((char*)storage_ptr + a->per_vertex_storage_stride_bytes);
                memcpy(storage_ptr, values_ptr, value_size_bytes);
                storage_ptr = (void*)((char*)storage_ptr + a->per_vertex_storage_stride_bytes);
                memcpy(storage_ptr, values_ptr, value_size_bytes);
                storage_ptr = (void*)((char*)storage_ptr + a->per_vertex_storage_stride_bytes);
                memcpy(storage_ptr, values_ptr, value_size_bytes);
                storage_ptr = (void*)((char*)storage_ptr + a->per_vertex_storage_stride_bytes);

                values_ptr = (void*)((char*)values_ptr + a->attribute.format.stride_bytes);
            }
        }
    }
}

static void upload_attribute_data(ContextData* cdata, int num_vertices)
{
    int i;
    for(i = 0; i < 16; i++)
    {
        AttributeSource* a = &cdata->shader_attributes[i];
        if(a->attribute.values != NULL && a->attribute.location >= 0 && a->num_values > 0)
        {
            int num_values_used = num_vertices;
			int bytes_used;

            if(a->num_values < num_values_used)
                num_values_used = a->num_values;

            glBindBuffer(GL_ARRAY_BUFFER, cdata->attribute_VBO[i]);

            bytes_used = a->per_vertex_storage_stride_bytes * num_values_used;
            glBufferData(GL_ARRAY_BUFFER, bytes_used, a->next_value, GL_STREAM_DRAW);

            glEnableVertexAttribArray(a->attribute.location);
            glVertexAttribPointer(a->attribute.location, a->attribute.format.num_elems_per_value, a->attribute.format.type, a->attribute.format.normalize, a->per_vertex_storage_stride_bytes, (void*)(intptr_t)a->per_vertex_storage_offset_bytes);

            a->enabled = true;
            // Move the data along so we use the next values for the next flush
            a->num_values -= num_values_used;
            if(a->num_values <= 0)
                a->next_value = a->per_vertex_storage;
            else
                a->next_value = (void*)(((char*)a->next_value) + bytes_used);
        }
    }
}

static void disable_attribute_data(ContextData* cdata)
{
    int i;
    for(i = 0; i < 16; i++)
    {
        AttributeSource* a = &cdata->shader_attributes[i];
        if(a->enabled)
        {
            glDisableVertexAttribArray(a->attribute.location);
            a->enabled = false;
        }
    }
}

static int get_lowest_attribute_num_values(ContextData* cdata, int cap)
{
    int lowest = cap;

    int i;
    for(i = 0; i < 16; i++)
    {
        AttributeSource* a = &cdata->shader_attributes[i];
        if(a->attribute.values != NULL && a->attribute.location >= 0)
        {
            if(a->num_values < lowest)
                lowest = a->num_values;
        }
    }

    return lowest;
}

static_inline void submit_buffer_data(int bytes, float* values, int bytes_indices, unsigned short* indices)
{
#ifdef XGPU_USE_BUFFER_MAPPING
	// NOTE: On the Raspberry Pi, you may have to use GL_DYNAMIC_DRAW instead of GL_STREAM_DRAW for buffers to work with glMapBuffer().
	float* data = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	unsigned short* data_i = (indices == NULL ? NULL : (unsigned short*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY));
	if (data != NULL)
	{
		memcpy(data, values, bytes);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	if (data_i != NULL)
	{
		memcpy(data_i, indices, bytes_indices);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
#elif defined(XGPU_USE_BUFFER_RESET)
	glBufferData(GL_ARRAY_BUFFER, bytes, values, GL_STREAM_DRAW);
	if (indices != NULL)
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes_indices, indices, GL_DYNAMIC_DRAW);
#else
	glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, values);
	if (indices != NULL)
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, bytes_indices, indices);
#endif  
}

// Assumes the right format
void Renderer::TriangleBatchX(Image* image, Target* target, unsigned short num_vertices, void* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags)
{
    Context* context;
	ContextData* cdata;
	int stride, offset_texcoords, offset_colors;
	int size_vertices, size_texcoords, size_colors;

	bool using_texture = (image != NULL);
	bool use_vertices = (flags & (BATCH_XY | BATCH_XYZ));
	bool use_texcoords = (flags & BATCH_ST);
	bool use_colors = (flags & (BATCH_RGB | BATCH_RGBA | BATCH_RGB8 | BATCH_RGBA8));
	bool use_byte_colors = (flags & (BATCH_RGB8 | BATCH_RGBA8));
	bool use_z = (flags & BATCH_XYZ);
	bool use_a = (flags & (BATCH_RGBA | BATCH_RGBA8));

    if(num_vertices == 0)
        return;

    if(target == NULL)
    {
        PushErrorCode("TriangleBatchX", ERROR_NULL_ARGUMENT, "target");
        return;
    }
    if((image != NULL && _device != image->renderer) || _device != target->renderer)
    {
        PushErrorCode("TriangleBatchX", ERROR_USER_ERROR, "Mismatched _device");
        return;
    }

    makeContextCurrent(target);

    // Bind the texture to which subsequent calls refer
    if(using_texture)
        bindTexture(image);

    // Bind the FBO
    if(!bindFramebuffer(target))
    {
        PushErrorCode("TriangleBatchX", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
        return;
    }

    prepareToRenderToTarget(target);
    if(using_texture)
        prepareToRenderImage(target, image);
    else
        prepareToRenderShapes(GL_TRIANGLES);
    changeViewport(target);
    changeCamera(target);

    if(using_texture)
        changeTexturing(true);

    setClipRect(target);

    
    context = _device->current_context_target->context;
    cdata = (ContextData*)context->data;

    FlushBlitBuffer();

    if(cdata->index_buffer_num_vertices + num_indices >= cdata->index_buffer_max_num_vertices)
    {
        growBlitBuffer(cdata, cdata->index_buffer_num_vertices + num_indices);
    }
    if(cdata->blit_buffer_num_vertices + num_vertices >= cdata->blit_buffer_max_num_vertices)
    {
        growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + num_vertices);
    }

    // Only need to check the blit buffer because of the VBO storage
    if(cdata->blit_buffer_num_vertices + num_vertices >= cdata->blit_buffer_max_num_vertices)
    {
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + num_vertices))
        {
            // Can't do all of these sprites!  Only do some of them...
            num_vertices = (cdata->blit_buffer_max_num_vertices - cdata->blit_buffer_num_vertices);
        }
    }
    if(cdata->index_buffer_num_vertices + num_indices >= cdata->index_buffer_max_num_vertices)
    {
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + num_indices))
        {
            // Can't do all of these sprites!  Only do some of them...
            num_indices = (cdata->index_buffer_max_num_vertices - cdata->index_buffer_num_vertices);
        }
    }
 
    refresh_attribute_data(cdata);

    if(indices == NULL)
        num_indices = num_vertices;

    (void)stride;
    (void)offset_texcoords;
    (void)offset_colors;
    (void)size_vertices;
    (void)size_texcoords;
    (void)size_colors;

    stride = 0;
    offset_texcoords = offset_colors = 0;
    size_vertices = size_texcoords = size_colors = 0;

    // Determine stride, size, and offsets
    if(use_vertices)
    {
        if(use_z)
            size_vertices = 3;
        else
            size_vertices = 2;

        stride += size_vertices;

        offset_texcoords = stride;
        offset_colors = stride;
    }

    if(use_texcoords)
    {
        size_texcoords = 2;

        stride += size_texcoords;

        offset_colors = stride;
    }

    if(use_colors)
    {
        if(use_a)
            size_colors = 4;
        else
            size_colors = 3;
    }
    
    // Floating point color components (either 3 or 4 floats)
    if(use_colors && !use_byte_colors)
    {
        stride += size_colors;
    }

    // Convert offsets to a number of bytes
    stride *= sizeof(float);
    offset_texcoords *= sizeof(float);
    offset_colors *= sizeof(float);

    // Unsigned byte color components (either 3 or 4 bytes)
    if(use_colors && use_byte_colors)
    {
        stride += size_colors;
    }


	// Skip uploads if we have no attribute location
	if (context->current_shader_block.position_loc < 0)
		use_vertices = false;
	if (context->current_shader_block.texcoord_loc < 0)
		use_texcoords = false;
	if (context->current_shader_block.color_loc < 0)
		use_colors = false;


	// Upload our modelviewprojection matrix
	if (context->current_shader_block.modelViewProjection_loc >= 0)
	{
		float mvp[16];
		float cam_matrix[16];
		GetModelViewProjection(mvp);
		get_camera_matrix(cam_matrix, cdata->last_camera);

		MultiplyAndAssign(mvp, cam_matrix);

		glUniformMatrix4fv(context->current_shader_block.modelViewProjection_loc, 1, 0, mvp);
	}

	if (values != NULL)
	{
		// Upload blit buffer to a single buffer object
		glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[cdata->blit_VBO_flop]);
		cdata->blit_VBO_flop = !cdata->blit_VBO_flop;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cdata->blit_IBO);

		// Copy the whole blit buffer to the GPU
		submit_buffer_data(stride * num_vertices, (float *)values, sizeof(unsigned short)*num_indices, indices);  // Fills GPU buffer with data.

																										 // Specify the formatting of the blit buffer
		if (use_vertices)
		{
			glEnableVertexAttribArray(context->current_shader_block.position_loc);  // Tell GL to use client-side attribute data
			glVertexAttribPointer(context->current_shader_block.position_loc, size_vertices, GL_FLOAT, GL_FALSE, stride, 0);  // Tell how the data is formatted
		}
		if (use_texcoords)
		{
			glEnableVertexAttribArray(context->current_shader_block.texcoord_loc);
			glVertexAttribPointer(context->current_shader_block.texcoord_loc, size_texcoords, GL_FLOAT, GL_FALSE, stride, (void*)(offset_texcoords));
		}
		if (use_colors)
		{
			glEnableVertexAttribArray(context->current_shader_block.color_loc);
			if (use_byte_colors)
			{
				glVertexAttribPointer(context->current_shader_block.color_loc, size_colors, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)(offset_colors));
			}
			else
			{
				glVertexAttribPointer(context->current_shader_block.color_loc, size_colors, GL_FLOAT, GL_FALSE, stride, (void*)(offset_colors));
			}
		}
		else
		{
			SDL_Color color = get_complete_mod_color(target, image);
			float default_color[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
			SetAttributefv(context->current_shader_block.color_loc, 4, default_color);
		}
	}

	upload_attribute_data(cdata, num_indices);

	if (indices == NULL)
		glDrawArrays(GL_TRIANGLES, 0, num_indices);
	else
		glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, (void*)0);

	// Disable the vertex arrays again
	if (use_vertices)
		glDisableVertexAttribArray(context->current_shader_block.position_loc);
	if (use_texcoords)
		glDisableVertexAttribArray(context->current_shader_block.texcoord_loc);
	if (use_colors)
		glDisableVertexAttribArray(context->current_shader_block.color_loc);

	disable_attribute_data(cdata);


    cdata->blit_buffer_num_vertices = 0;
    cdata->index_buffer_num_vertices = 0;

    unsetClipRect(target);
}

void Renderer::GenerateMipmaps(Image* image)
{
    #ifndef __IPHONEOS__
    GLint filter;
    if(image == NULL)
        return;

    if(image->target != NULL && isCurrentTarget(image->target))
        FlushBlitBuffer();
    bindTexture(image);
    glGenerateMipmap(GL_TEXTURE_2D);
    image->has_mipmaps = true;

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    if(filter == GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    #endif
}

GPU_Rect Renderer::SetClip(Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h)
{
	GPU_Rect r;
    if(target == NULL)
    {
        GPU_Rect r = {0,0,0,0};
        return r;
    }

    if(isCurrentTarget(target))
        FlushBlitBuffer();
    target->use_clip_rect = true;

    r = target->clip_rect;

    target->clip_rect.x = x;
    target->clip_rect.y = y;
    target->clip_rect.w = w;
    target->clip_rect.h = h;

    return r;
}

void Renderer::UnsetClip(Target* target)
{
    if(target == NULL)
        return;

    makeContextCurrent(target);

    if(isCurrentTarget(target))
        FlushBlitBuffer();
    // Leave the clip rect values intact so they can still be useful as storage
    target->use_clip_rect = false;
}


SDL_Color Renderer::GetPixel(Target* target, Sint16 x, Sint16 y)
{
    SDL_Color result = {0,0,0,0};
    if(target == NULL)
        return result;
    if(_device != target->renderer)
        return result;
    if(x < 0 || y < 0 || x >= target->w || y >= target->h)
        return result;

    if(isCurrentTarget(target))
        FlushBlitBuffer();
    if(bindFramebuffer(target))
    {
        unsigned char pixels[4];
		glReadPixels(x, target->image ? y : target->h - y - 1, 1, 1, ((TargetData*)target->data)->format, GL_UNSIGNED_BYTE, pixels);
        result.r = pixels[0];
        result.g = pixels[1];
        result.b = pixels[2];
        result.a = pixels[3];
    }
    return result;
}

bool Renderer::GetImageData(Target* target, unsigned char* data, Sint16 x, Sint16 y, Sint16 w, Sint16 h)
{	
	if (target == NULL) return false;
	if (_device != target->renderer) return false;
	if (x < 0 || y < 0 || x + w > target->w || y + h > target->h) return false;

	if (isCurrentTarget(target)) {
		FlushBlitBuffer();
	}
	if (bindFramebuffer(target)) {		
		glReadPixels(x, target->image ? y : target->h - y - 1, w, h, ((TargetData*)target->data)->format, GL_UNSIGNED_BYTE, data);
		return true;
	}
	return false;
}

void Renderer::SetImageFilter(Image* image, FilterEnum filter)
{
	GLenum minFilter, magFilter;

    if(image == NULL)
    {
        PushErrorCode("SetImageFilter", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(_device != image->renderer)
    {
        PushErrorCode("SetImageFilter", ERROR_USER_ERROR, "Mismatched _device");
        return;
    }

    switch(filter)
    {
        case FILTER_NEAREST:
            minFilter = GL_NEAREST;
            magFilter = GL_NEAREST;
            break;
        case FILTER_LINEAR:
            if(image->has_mipmaps)
                minFilter = GL_LINEAR_MIPMAP_NEAREST;
            else
                minFilter = GL_LINEAR;

            magFilter = GL_LINEAR;
            break;
        case FILTER_LINEAR_MIPMAP:
            if(image->has_mipmaps)
                minFilter = GL_LINEAR_MIPMAP_LINEAR;
            else
                minFilter = GL_LINEAR;

            magFilter = GL_LINEAR;
            break;
        default:
            PushErrorCode("SetImageFilter", ERROR_USER_ERROR, "Unsupported value for filter (0x%x)", filter);
            return;
    }

    flushBlitBufferIfCurrentTexture(image);
    bindTexture(image);

	image->filter_mode = filter;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
}

void Renderer::SetWrapMode(Image* image, WrapEnum wrap_mode_x, WrapEnum wrap_mode_y)
{
	GLenum wrap_x, wrap_y;

    if(image == NULL)
    {
        PushErrorCode("SetWrapMode", ERROR_NULL_ARGUMENT, "image");
        return;
    }
    if(_device != image->renderer)
    {
        PushErrorCode("SetWrapMode", ERROR_USER_ERROR, "Mismatched _device");
        return;
    }

	switch(wrap_mode_x)
	{
    case WRAP_NONE:
        wrap_x = GL_CLAMP_TO_EDGE;
        break;
    case WRAP_REPEAT:
        wrap_x = GL_REPEAT;
        break;
    case WRAP_MIRRORED:
        if(_device->enabled_features & FEATURE_WRAP_REPEAT_MIRRORED)
            wrap_x = GL_MIRRORED_REPEAT;
        else
        {
            PushErrorCode("SetWrapMode", ERROR_BACKEND_ERROR, "This _device does not support WRAP_MIRRORED.");
            return;
        }
        break;
    default:
        PushErrorCode("SetWrapMode", ERROR_USER_ERROR, "Unsupported value for wrap_mode_x (0x%x)", wrap_mode_x);
        return;
	}

	switch(wrap_mode_y)
	{
    case WRAP_NONE:
        wrap_y = GL_CLAMP_TO_EDGE;
        break;
    case WRAP_REPEAT:
        wrap_y = GL_REPEAT;
        break;
    case WRAP_MIRRORED:
        if(_device->enabled_features & FEATURE_WRAP_REPEAT_MIRRORED)
            wrap_y = GL_MIRRORED_REPEAT;
        else
        {
            PushErrorCode("SetWrapMode", ERROR_BACKEND_ERROR, "This _device does not support WRAP_MIRRORED.");
            return;
        }
        break;
    default:
        PushErrorCode("SetWrapMode", ERROR_USER_ERROR, "Unsupported value for wrap_mode_y (0x%x)", wrap_mode_y);
        return;
	}

    flushBlitBufferIfCurrentTexture(image);
    bindTexture(image);

	image->wrap_mode_x = wrap_mode_x;
	image->wrap_mode_y = wrap_mode_y;

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_x );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_y );
}


void Renderer::ClearRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if(target == NULL)
        return;
    if(_device != target->renderer)
        return;

    makeContextCurrent(target);

    if(isCurrentTarget(target))
        FlushBlitBuffer();
    if(bindFramebuffer(target))
    {
        setClipRect(target);

        glClearColor(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        unsetClipRect(target);
    }
}

static void DoPartialFlush(Target* dest, Context* context, unsigned short num_vertices, float* blit_buffer, unsigned int num_indices, unsigned short* index_buffer)
{
    ContextData* cdata = (ContextData*)context->data;

	// Upload our modelviewprojection matrix
	if (context->current_shader_block.modelViewProjection_loc >= 0)
	{
		float p[16];
		float mv[16];
		float mvp[16];

		MatrixCopy(p, GetProjection());
		MatrixCopy(mv, GetModelView());

		if (dest->use_camera)
		{
			float cam_matrix[16];
			get_camera_matrix(cam_matrix, cdata->last_camera);

			MultiplyAndAssign(cam_matrix, p);
			MatrixCopy(p, cam_matrix);
		}

		// MVP = P * MV
		Multiply4x4(mvp, p, mv);

		glUniformMatrix4fv(context->current_shader_block.modelViewProjection_loc, 1, 0, mvp);
	}

	// Upload blit buffer to a single buffer object
	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[cdata->blit_VBO_flop]);
	cdata->blit_VBO_flop = !cdata->blit_VBO_flop;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cdata->blit_IBO);

	// Copy the whole blit buffer to the GPU
	submit_buffer_data(BLIT_BUFFER_STRIDE * num_vertices, blit_buffer, sizeof(unsigned short)*num_indices, index_buffer);  // Fills GPU buffer with data.

																															   // Specify the formatting of the blit buffer
	if (context->current_shader_block.position_loc >= 0)
	{
		glEnableVertexAttribArray(context->current_shader_block.position_loc);  // Tell GL to use client-side attribute data
		glVertexAttribPointer(context->current_shader_block.position_loc, 2, GL_FLOAT, GL_FALSE, BLIT_BUFFER_STRIDE, 0);  // Tell how the data is formatted
	}
	if (context->current_shader_block.texcoord_loc >= 0)
	{
		glEnableVertexAttribArray(context->current_shader_block.texcoord_loc);
		glVertexAttribPointer(context->current_shader_block.texcoord_loc, 2, GL_FLOAT, GL_FALSE, BLIT_BUFFER_STRIDE, (void*)(BLIT_BUFFER_TEX_COORD_OFFSET * sizeof(float)));
	}
	if (context->current_shader_block.color_loc >= 0)
	{
		glEnableVertexAttribArray(context->current_shader_block.color_loc);
		glVertexAttribPointer(context->current_shader_block.color_loc, 4, GL_FLOAT, GL_FALSE, BLIT_BUFFER_STRIDE, (void*)(BLIT_BUFFER_COLOR_OFFSET * sizeof(float)));
	}

	upload_attribute_data(cdata, num_vertices);

	glDrawElements(cdata->last_shape, num_indices, GL_UNSIGNED_SHORT, (void*)0);

	// Disable the vertex arrays again
	if (context->current_shader_block.position_loc >= 0)
		glDisableVertexAttribArray(context->current_shader_block.position_loc);
	if (context->current_shader_block.texcoord_loc >= 0)
		glDisableVertexAttribArray(context->current_shader_block.texcoord_loc);
	if (context->current_shader_block.color_loc >= 0)
		glDisableVertexAttribArray(context->current_shader_block.color_loc);

	disable_attribute_data(cdata);


}

static void DoUntexturedFlush(Context* context, unsigned short num_vertices, float* blit_buffer, unsigned int num_indices, unsigned short* index_buffer)
{
    ContextData* cdata = (ContextData*)context->data;

	// Upload our modelviewprojection matrix
	if (context->current_shader_block.modelViewProjection_loc >= 0)
	{
		float mvp[16];
		float cam_matrix[16];
		GetModelViewProjection(mvp);
		get_camera_matrix(cam_matrix, cdata->last_camera);

		MultiplyAndAssign(mvp, cam_matrix);

		glUniformMatrix4fv(context->current_shader_block.modelViewProjection_loc, 1, 0, mvp);
	}

	// Upload blit buffer to a single buffer object
	glBindBuffer(GL_ARRAY_BUFFER, cdata->blit_VBO[cdata->blit_VBO_flop]);
	cdata->blit_VBO_flop = !cdata->blit_VBO_flop;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cdata->blit_IBO);

	// Copy the whole blit buffer to the GPU
	submit_buffer_data(BLIT_BUFFER_STRIDE * num_vertices, blit_buffer, sizeof(unsigned short)*num_indices, index_buffer);  // Fills GPU buffer with data.

																															   // Specify the formatting of the blit buffer
	if (context->current_shader_block.position_loc >= 0)
	{
		glEnableVertexAttribArray(context->current_shader_block.position_loc);  // Tell GL to use client-side attribute data
		glVertexAttribPointer(context->current_shader_block.position_loc, 2, GL_FLOAT, GL_FALSE, BLIT_BUFFER_STRIDE, 0);  // Tell how the data is formatted
	}
	if (context->current_shader_block.color_loc >= 0)
	{
		glEnableVertexAttribArray(context->current_shader_block.color_loc);
		glVertexAttribPointer(context->current_shader_block.color_loc, 4, GL_FLOAT, GL_FALSE, BLIT_BUFFER_STRIDE, (void*)(BLIT_BUFFER_COLOR_OFFSET * sizeof(float)));
	}

	upload_attribute_data(cdata, num_vertices);

	glDrawElements(cdata->last_shape, num_indices, GL_UNSIGNED_SHORT, (void*)0);

	// Disable the vertex arrays again
	if (context->current_shader_block.position_loc >= 0)
		glDisableVertexAttribArray(context->current_shader_block.position_loc);
	if (context->current_shader_block.color_loc >= 0)
		glDisableVertexAttribArray(context->current_shader_block.color_loc);

	disable_attribute_data(cdata);
}

#define MAX(a, b) ((a) > (b)? (a) : (b))

void Renderer::FlushBlitBuffer()
{
    Context* context;
    ContextData* cdata;
    if(_device->current_context_target == NULL)
        return;

    context = _device->current_context_target->context;
    cdata = (ContextData*)context->data;
    if(cdata->blit_buffer_num_vertices > 0 && cdata->last_target != NULL)
    {
		Target* dest = cdata->last_target;
		int num_vertices;
		int num_indices;
		float* blit_buffer;
		unsigned short* index_buffer;

        changeViewport(dest);
        changeCamera(dest);

        applyTexturing(_device);

        setClipRect(dest);
        
        refresh_attribute_data(cdata);

        blit_buffer = cdata->blit_buffer;
        index_buffer = cdata->index_buffer;

        if(cdata->last_use_texturing)
        {
            while(cdata->blit_buffer_num_vertices > 0)
            {
                num_vertices = MAX(cdata->blit_buffer_num_vertices, get_lowest_attribute_num_values(cdata, cdata->blit_buffer_num_vertices));
                num_indices = num_vertices * 3 / 2;  // 6 indices per sprite / 4 vertices per sprite = 3/2

                DoPartialFlush(dest, context, num_vertices, blit_buffer, num_indices, index_buffer);

                cdata->blit_buffer_num_vertices -= num_vertices;
                // Move our pointers ahead
                blit_buffer += BLIT_BUFFER_FLOATS_PER_VERTEX*num_vertices;
                index_buffer += num_indices;
            }
        }
        else
        {
            DoUntexturedFlush(context, cdata->blit_buffer_num_vertices, blit_buffer, cdata->index_buffer_num_vertices, index_buffer);
        }

        cdata->blit_buffer_num_vertices = 0;
        cdata->index_buffer_num_vertices = 0;

        unsetClipRect(dest);
    }
}

void Renderer::Flip(Target* target)
{
    FlushBlitBuffer();
    makeContextCurrent(target);

#ifdef SINGLE_BUFFERED
	glFlush();	
#endif
    SDL_GL_SwapWindow(SDL_GetWindowFromID(_device->current_context_target->context->windowID));
#ifdef XGPU_USE_OPENGL
	if (vendor_is_Intel) apply_Intel_attrib_workaround = true;
#endif    
}


// Shader API

#include <string.h>

// On some platforms (e.g. Android), it might not be possible to just create a rwops and get the expected #included files.
// To do it, I might want to add an optional argument that specifies a base directory to prepend to #include file names.

static Uint32 GetShaderSourceSize(const char* filename);
static Uint32 GetShaderSource(const char* filename, char* result);

static void read_until_end_of_comment(SDL_RWops* rwops, char multiline)
{
    char buffer;
    while(SDL_RWread(rwops, &buffer, 1, 1) > 0)
    {
        if(!multiline)
        {
            if(buffer == '\n')
                break;
        }
        else
        {
            if(buffer == '*')
            {
                // If the stream ends at the next character or it is a '/', then we're done.
                if(SDL_RWread(rwops, &buffer, 1, 1) <= 0 || buffer == '/')
                    break;
            }
        }
    }
}

static Uint32 GetShaderSourceSize_RW(SDL_RWops* shader_source)
{
	Uint32 size;
	char last_char;
	char buffer[512];
	long len;

    if(shader_source == NULL)
        return 0;

    size = 0;

    // Read 1 byte at a time until we reach the end
    last_char = ' ';
    len = 0;
    while((len = SDL_RWread(shader_source, &buffer, 1, 1)) > 0)
    {
        // Follow through an #include directive?
        if(buffer[0] == '#')
        {
            // Get the rest of the line
            int line_size = 1;
            unsigned long line_len;
			char* token;
            while((line_len = SDL_RWread(shader_source, buffer+line_size, 1, 1)) > 0)
            {
                line_size += line_len;
                if(buffer[line_size - line_len] == '\n')
                    break;
            }
            buffer[line_size] = '\0';

            // Is there "include" after '#'?
            token = strtok(buffer, "# \t");

            if(token != NULL && strcmp(token, "include") == 0)
            {
                // Get filename token
                token = strtok(NULL, "\"");  // Skip the empty token before the quote
                if(token != NULL)
                {
                    // Add the size of the included file and a newline character
                    size += GetShaderSourceSize(token) + 1;
                }
            }
            else
                size += line_size;
            last_char = ' ';
            continue;
        }

        size += len;

        if(last_char == '/')
        {
            if(buffer[0] == '/')
            {
                read_until_end_of_comment(shader_source, 0);
                size++;  // For the end of the comment
            }
            else if(buffer[0] == '*')
            {
                read_until_end_of_comment(shader_source, 1);
                size += 2;  // For the end of the comments
            }
            last_char = ' ';
        }
        else
            last_char = buffer[0];
    }

    // Go back to the beginning of the stream
    SDL_RWseek(shader_source, 0, SEEK_SET);
    return size;
}

static Uint32 GetShaderSource_RW(SDL_RWops* shader_source, char* result)
{
	Uint32 size;
	char last_char;
	char buffer[512];
	long len;

    if(shader_source == NULL)
    {
        result[0] = '\0';
        return 0;
    }

    size = 0;

    // Read 1 byte at a time until we reach the end
    last_char = ' ';
    len = 0;
    while((len = SDL_RWread(shader_source, &buffer, 1, 1)) > 0)
    {
        // Follow through an #include directive?
        if(buffer[0] == '#')
        {
            // Get the rest of the line
            int line_size = 1;
			unsigned long line_len;
			char token_buffer[512];  // strtok() is destructive
			char* token;
            while((line_len = SDL_RWread(shader_source, buffer+line_size, 1, 1)) > 0)
            {
                line_size += line_len;
                if(buffer[line_size - line_len] == '\n')
                    break;
            }

            // Is there "include" after '#'?
            memcpy(token_buffer, buffer, line_size+1);
            token_buffer[line_size] = '\0';
            token = strtok(token_buffer, "# \t");

            if(token != NULL && strcmp(token, "include") == 0)
            {
                // Get filename token
                token = strtok(NULL, "\"");  // Skip the empty token before the quote
                if(token != NULL)
                {
                    // Add the size of the included file and a newline character
                    size += GetShaderSource(token, result + size);
                    result[size] = '\n';
                    size++;
                }
            }
            else
            {
                memcpy(result + size, buffer, line_size);
                size += line_size;
            }
            last_char = ' ';
            continue;
        }

        memcpy(result + size, buffer, len);
        size += len;

        if(last_char == '/')
        {
            if(buffer[0] == '/')
            {
                read_until_end_of_comment(shader_source, 0);
                memcpy(result + size, "\n", 1);
                size++;
            }
            else if(buffer[0] == '*')
            {
                read_until_end_of_comment(shader_source, 1);
                memcpy(result + size, "*/", 2);
                size += 2;
            }
            last_char = ' ';
        }
        else
            last_char = buffer[0];
    }
    result[size] = '\0';

    // Go back to the beginning of the stream
    SDL_RWseek(shader_source, 0, SEEK_SET);
    return size;
}

static Uint32 GetShaderSource(const char* filename, char* result)
{
	SDL_RWops* rwops;
	Uint32 size;

    if(filename == NULL)
        return 0;
    rwops = SDL_RWFromFile(filename, "r");

    size = GetShaderSource_RW(rwops, result);

    SDL_RWclose(rwops);
    return size;
}

static Uint32 GetShaderSourceSize(const char* filename)
{
	SDL_RWops* rwops;
	Uint32 result;

    if(filename == NULL)
        return 0;
    rwops = SDL_RWFromFile(filename, "r");

    result = GetShaderSourceSize_RW(rwops);

    SDL_RWclose(rwops);
    return result;
}

static Uint32 compile_shader_source(ShaderEnum shader_type, const char* shader_source)
{
    // Create the proper new shader object
    GLuint shader_object = 0;
	(void)shader_type;
	(void)shader_source;
    
    GLint compiled;

    switch(shader_type)
    {
    case VERTEX_SHADER:
        shader_object = glCreateShader(GL_VERTEX_SHADER);
        break;
    case FRAGMENT_SHADER:
        shader_object = glCreateShader(GL_FRAGMENT_SHADER);
        break;
    case GEOMETRY_SHADER:
#ifdef GL_GEOMETRY_SHADER
		shader_object = glCreateShader(GL_GEOMETRY_SHADER);
#else
		PushErrorCode("CompileShader", ERROR_BACKEND_ERROR, "Hardware does not support GEOMETRY_SHADER.");
		snprintf(shader_message, 256, "Failed to create geometry shader object.\n");
		return 0;
#endif
        break;
    }

    if(shader_object == 0)
    {
        PushErrorCode("CompileShader", ERROR_BACKEND_ERROR, "Failed to create new shader object");
        snprintf(shader_message, 256, "Failed to create new shader object.\n");
        return 0;
    }

	glShaderSource(shader_object, 1, &shader_source, NULL);

    // Compile the shader source

	glCompileShader(shader_object);

    glGetShaderiv(shader_object, GL_COMPILE_STATUS, &compiled);
    if(!compiled)
    {
        PushErrorCode("CompileShader", ERROR_DATA_ERROR, "Failed to compile shader source");
        glGetShaderInfoLog(shader_object, 256, NULL, shader_message);
        glDeleteShader(shader_object);
        return 0;
    }
    
    return shader_object;
}


Uint32 Renderer::CompileShader_RW(ShaderEnum shader_type, SDL_RWops* shader_source, bool free_rwops)
{
    // Read in the shader source code
    Uint32 size = GetShaderSourceSize_RW(shader_source);
    char* source_string = (char*)SDL_malloc(size+1);
    int result = GetShaderSource_RW(shader_source, source_string);
	Uint32 result2;
	(void)_device;

    if(free_rwops)
        SDL_RWclose(shader_source);
    
    if(!result)
    {
        PushErrorCode("CompileShader", ERROR_DATA_ERROR, "Failed to read shader source");
        snprintf(shader_message, 256, "Failed to read shader source.\n");
        SDL_free(source_string);
        return 0;
    }

    result2 = compile_shader_source(shader_type, source_string);
    SDL_free(source_string);

    return result2;
}

Uint32 Renderer::CompileShader(ShaderEnum shader_type, const char* shader_source)
{
    Uint32 size = (Uint32)strlen(shader_source);
	SDL_RWops* rwops;
    if(size == 0)
        return 0;
    rwops = SDL_RWFromConstMem(shader_source, size);
    return CompileShader_RW(shader_type, rwops, 1);
}

Uint32 Renderer::CreateShaderProgram()
{
    GLuint p;
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return 0;
    p = glCreateProgram();
    return p;
}

bool Renderer::LinkShaderProgram(Uint32 program_object)
{    
	int linked;

    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return false;
    
    // Bind the position attribute to location 0.
    // We always pass position data (right?), but on some systems (e.g. GL 2 on OS X), color is bound to 0
    // and the shader won't run when TriangleBatch uses BATCH_XY_ST (no color array).  Guess they didn't consider default attribute values...
    glBindAttribLocation(program_object, 0, "gpu_Vertex");
	glLinkProgram(program_object);

	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);

	if(!linked)
    {
        PushErrorCode("LinkShaderProgram", ERROR_BACKEND_ERROR, "Failed to link shader program");
        glGetProgramInfoLog(program_object, 256, NULL, shader_message);
        glDeleteProgram(program_object);
        return false;
    }

	return true;
}

void Renderer::FreeShader(Uint32 shader_object)
{
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        glDeleteShader(shader_object);    
}

void Renderer::FreeShaderProgram(Uint32 program_object)
{	
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        glDeleteProgram(program_object);    
}

void Renderer::AttachShader(Uint32 program_object, Uint32 shader_object)
{
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        glAttachShader(program_object, shader_object); 
}

void Renderer::DetachShader(Uint32 program_object, Uint32 shader_object)
{
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        glDetachShader(program_object, shader_object); 
}

void Renderer::ActivateShaderProgram(Uint32 program_object, ShaderBlock* block)
{
	Target* target = _device->current_context_target;	
    if(IsFeatureEnabled(FEATURE_BASIC_SHADERS))
    {
        if(program_object == 0) // Implies default shader
        {
            // Already using a default shader?
            if(target->context->current_shader_program == target->context->default_textured_shader_program
                || target->context->current_shader_program == target->context->default_untextured_shader_program)
                return;

            program_object = target->context->default_textured_shader_program;
        }

        FlushBlitBuffer();
        glUseProgram(program_object);

		// Set up our shader attribute and uniform locations
		if (block == NULL)
		{
			if (program_object == target->context->default_textured_shader_program)
				target->context->current_shader_block = target->context->default_textured_shader_block;
			else if (program_object == target->context->default_untextured_shader_program)
				target->context->current_shader_block = target->context->default_untextured_shader_block;
			else
			{
				ShaderBlock b;
				b.position_loc = -1;
				b.texcoord_loc = -1;
				b.color_loc = -1;
				b.modelViewProjection_loc = -1;
				target->context->current_shader_block = b;
			}
		}
		else
			target->context->current_shader_block = *block;
    }
    
    target->context->current_shader_program = program_object;
}

void Renderer::DeactivateShaderProgram()
{
    ActivateShaderProgram(0, NULL);
}

const char* Renderer::GetShaderMessage()
{
	(void)_device;
    return shader_message;
}

int Renderer::GetAttributeLocation(Uint32 program_object, const char* attrib_name)
{    
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return -1;
    program_object = get_proper_program_id(program_object);
    if(program_object == 0)
        return -1;
    return glGetAttribLocation(program_object, attrib_name);
}

int Renderer::GetUniformLocation(Uint32 program_object, const char* uniform_name)
{    
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return -1;
    program_object = get_proper_program_id(program_object);
    if(program_object == 0)
        return -1;
    return glGetUniformLocation(program_object, uniform_name);
}

ShaderBlock Renderer::LoadShaderBlock(Uint32 program_object, const char* position_name, const char* texcoord_name, const char* color_name, const char* modelViewMatrix_name)
{
    ShaderBlock b;
    program_object = get_proper_program_id(program_object);
    if(program_object == 0 || !IsFeatureEnabled(FEATURE_BASIC_SHADERS))
    {
        b.position_loc = -1;
        b.texcoord_loc = -1;
        b.color_loc = -1;
        b.modelViewProjection_loc = -1;
        return b;
    }

    if(position_name == NULL)
        b.position_loc = -1;
    else
        b.position_loc = GetAttributeLocation(program_object, position_name);

    if(texcoord_name == NULL)
        b.texcoord_loc = -1;
    else
        b.texcoord_loc = GetAttributeLocation(program_object, texcoord_name);

    if(color_name == NULL)
        b.color_loc = -1;
    else
        b.color_loc = GetAttributeLocation(program_object, color_name);

    if(modelViewMatrix_name == NULL)
        b.modelViewProjection_loc = -1;
    else
        b.modelViewProjection_loc = GetUniformLocation(program_object, modelViewMatrix_name);

    return b;
}

void Renderer::SetShaderImage(Image* image, int location, int image_unit)
{    
	Uint32 new_texture;

    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;

    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0 || image_unit < 0)
        return;

    new_texture = 0;
    if(image != NULL)
        new_texture = ((ImageData*)image->data)->handle;

    // Set the new image unit
    glUniform1i(location, image_unit);
    glActiveTexture(GL_TEXTURE0 + image_unit);
    glBindTexture(GL_TEXTURE_2D, new_texture);

    if(image_unit != 0)
        glActiveTexture(GL_TEXTURE0);
}

void Renderer::GetUniformiv(Uint32 program_object, int location, int* values)
{	
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    program_object = get_proper_program_id(program_object);
    if(program_object != 0)
        glGetUniformiv(program_object, location, values); 
}

void Renderer::SetUniformi(int location, int value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    glUniform1i(location, value); 
}

void Renderer::SetUniformiv(int location, int num_elements_per_value, int num_values, int* values)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    switch(num_elements_per_value)
    {
        case 1:
        glUniform1iv(location, num_values, values);
        break;
        case 2:
        glUniform2iv(location, num_values, values);
        break;
        case 3:
        glUniform3iv(location, num_values, values);
        break;
        case 4:
        glUniform4iv(location, num_values, values);
        break;
    }
}

void Renderer::GetUniformuiv(Uint32 program_object, int location, unsigned int* values)
{	
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    program_object = get_proper_program_id(program_object);
    if(program_object != 0)
        #if defined(XGPU_USE_GLES) && XGPU_GLES_MAJOR_VERSION < 3
        glGetUniformiv(program_object, location, (int*)values);
        #else
        glGetUniformuiv(program_object, location, values);
        #endif 
}

void Renderer::SetUniformui(int location, unsigned int value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    #if defined(XGPU_USE_GLES) && XGPU_GLES_MAJOR_VERSION < 3
    glUniform1i(location, (int)value);
    #else
    glUniform1ui(location, value);
    #endif 
}

void Renderer::SetUniformuiv(int location, int num_elements_per_value, int num_values, unsigned int* values)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    #if defined(XGPU_USE_GLES) && XGPU_GLES_MAJOR_VERSION < 3
    switch(num_elements_per_value)
    {
        case 1:
        glUniform1iv(location, num_values, (int*)values);
        break;
        case 2:
        glUniform2iv(location, num_values, (int*)values);
        break;
        case 3:
        glUniform3iv(location, num_values, (int*)values);
        break;
        case 4:
        glUniform4iv(location, num_values, (int*)values);
        break;
    }
    #else
    switch(num_elements_per_value)
    {
        case 1:
        glUniform1uiv(location, num_values, values);
        break;
        case 2:
        glUniform2uiv(location, num_values, values);
        break;
        case 3:
        glUniform3uiv(location, num_values, values);
        break;
        case 4:
        glUniform4uiv(location, num_values, values);
        break;
    }
    #endif
}


void Renderer::GetUniformfv(Uint32 program_object, int location, float* values)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    program_object = get_proper_program_id(program_object);
    if(program_object != 0)
        glGetUniformfv(program_object, location, values);
}

void Renderer::SetUniformf(int location, float value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    glUniform1f(location, value);
}

void Renderer::SetUniformfv(int location, int num_elements_per_value, int num_values, float* values)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    switch(num_elements_per_value)
    {
        case 1:
        glUniform1fv(location, num_values, values);
        break;
        case 2:
        glUniform2fv(location, num_values, values);
        break;
        case 3:
        glUniform3fv(location, num_values, values);
        break;
        case 4:
        glUniform4fv(location, num_values, values);
        break;
    }    
}

void Renderer::SetUniformMatrixfv(int location, int num_matrices, int num_rows, int num_columns, bool transpose, float* values)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;
    if(num_rows < 2 || num_rows > 4 || num_columns < 2 || num_columns > 4)
    {
        PushErrorCode("SetUniformMatrixfv", ERROR_DATA_ERROR, "Given invalid dimensions (%dx%d)", num_rows, num_columns);
        return;
    }
    #if defined(XGPU_USE_GLES)
    // Hide these symbols so it compiles, but make sure they never get called because GLES only supports square matrices.
    #define glUniformMatrix2x3fv glUniformMatrix2fv
    #define glUniformMatrix2x4fv glUniformMatrix2fv
    #define glUniformMatrix3x2fv glUniformMatrix2fv
    #define glUniformMatrix3x4fv glUniformMatrix2fv
    #define glUniformMatrix4x2fv glUniformMatrix2fv
    #define glUniformMatrix4x3fv glUniformMatrix2fv
    if(num_rows != num_columns)
    {
        PushErrorCode("SetUniformMatrixfv", ERROR_DATA_ERROR, "GLES renderers do not accept non-square matrices (given %dx%d)", num_rows, num_columns);
        return;
    }
    #endif

    switch(num_rows)
    {
    case 2:
        if(num_columns == 2)
            glUniformMatrix2fv(location, num_matrices, transpose, values);
        else if(num_columns == 3)
            glUniformMatrix2x3fv(location, num_matrices, transpose, values);
        else if(num_columns == 4)
            glUniformMatrix2x4fv(location, num_matrices, transpose, values);
        break;
    case 3:
        if(num_columns == 2)
            glUniformMatrix3x2fv(location, num_matrices, transpose, values);
        else if(num_columns == 3)
            glUniformMatrix3fv(location, num_matrices, transpose, values);
        else if(num_columns == 4)
            glUniformMatrix3x4fv(location, num_matrices, transpose, values);
        break;
    case 4:
        if(num_columns == 2)
            glUniformMatrix4x2fv(location, num_matrices, transpose, values);
        else if(num_columns == 3)
            glUniformMatrix4x3fv(location, num_matrices, transpose, values);
        else if(num_columns == 4)
            glUniformMatrix4fv(location, num_matrices, transpose, values);
        break;
    }    
}


void Renderer::SetAttributef(int location, float value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    glVertexAttrib1f(location, value);    
}

void Renderer::SetAttributei(int location, int value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    glVertexAttribI1i(location, value);
}

void Renderer::SetAttributeui(int location, unsigned int value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    glVertexAttribI1ui(location, value);
}


void Renderer::SetAttributefv(int location, int num_elements, float* value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    switch(num_elements)
    {
        case 1:
            glVertexAttrib1f(location, value[0]);
            break;
        case 2:
            glVertexAttrib2f(location, value[0], value[1]);
            break;
        case 3:
            glVertexAttrib3f(location, value[0], value[1], value[2]);
            break;
        case 4:
            glVertexAttrib4f(location, value[0], value[1], value[2], value[3]);
            break;
    }
}

void Renderer::SetAttributeiv(int location, int num_elements, int* value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    switch(num_elements)
    {
        case 1:
            glVertexAttribI1i(location, value[0]);
            break;
        case 2:
            glVertexAttribI2i(location, value[0], value[1]);
            break;
        case 3:
            glVertexAttribI3i(location, value[0], value[1], value[2]);
            break;
        case 4:
            glVertexAttribI4i(location, value[0], value[1], value[2], value[3]);
            break;
    }
}

void Renderer::SetAttributeuiv(int location, int num_elements, unsigned int* value)
{
    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    FlushBlitBuffer();
    if(_device->current_context_target->context->current_shader_program == 0)
        return;

    #ifdef XGPU_USE_OPENGL
    if(apply_Intel_attrib_workaround && location == 0)
    {
        apply_Intel_attrib_workaround = false;
        glBegin(GL_TRIANGLES);
        glEnd();
    }
    #endif

    switch(num_elements)
    {
        case 1:
            glVertexAttribI1ui(location, value[0]);
            break;
        case 2:
            glVertexAttribI2ui(location, value[0], value[1]);
            break;
        case 3:
            glVertexAttribI3ui(location, value[0], value[1], value[2]);
            break;
        case 4:
            glVertexAttribI4ui(location, value[0], value[1], value[2], value[3]);
            break;
    }    
}

void Renderer::SetAttributeSource(int num_values, Attribute source)
{
	ContextData* cdata;
	AttributeSource* a;

    if(!IsFeatureEnabled(FEATURE_BASIC_SHADERS))
        return;
    if(source.location < 0 || source.location >= 16)
        return;
    cdata = (ContextData*)_device->current_context_target->context->data;
    a = &cdata->shader_attributes[source.location];
    if(source.format.is_per_sprite)
    {
		int needed_size;

        a->per_vertex_storage_offset_bytes = 0;
        a->per_vertex_storage_stride_bytes = source.format.num_elems_per_value * sizeof_type(source.format.type);
        a->num_values = 4 * num_values;  // 4 vertices now
        needed_size = a->num_values * a->per_vertex_storage_stride_bytes;

        // Make sure we have enough room for converted per-vertex data
        if(a->per_vertex_storage_size < needed_size)
        {
            SDL_free(a->per_vertex_storage);
            a->per_vertex_storage = SDL_malloc(needed_size);
            a->per_vertex_storage_size = needed_size;
        }
    }
    else if(a->per_vertex_storage_size > 0)
    {
        SDL_free(a->per_vertex_storage);
        a->per_vertex_storage = NULL;
        a->per_vertex_storage_size = 0;
    }

    a->enabled = false;
    a->attribute = source;

    if(!source.format.is_per_sprite)
    {
        a->per_vertex_storage = source.values;
        a->num_values = num_values;
        a->per_vertex_storage_stride_bytes = source.format.stride_bytes;
        a->per_vertex_storage_offset_bytes = source.format.offset_bytes;
    }

    a->next_value = a->per_vertex_storage;
}




#ifndef DEGPERRAD
#define DEGPERRAD 57.2957795f
#endif

#ifndef RADPERDEG
#define RADPERDEG 0.0174532925f
#endif

// All shapes start this way for setup and so they can access the blit buffer properly
#define BEGIN_UNTEXTURED(function_name, shape, num_additional_vertices, num_additional_indices) \
	ContextData* cdata; \
	float* blit_buffer; \
	unsigned short* index_buffer; \
	int vert_index; \
	int color_index; \
	float r, g, b, a; \
	unsigned short blit_buffer_starting_index; \
    if(target == NULL) \
    { \
        PushErrorCode(function_name, ERROR_NULL_ARGUMENT, "target"); \
        return; \
    } \
    if(_device != target->renderer) \
    { \
        PushErrorCode(function_name, ERROR_USER_ERROR, "Mismatched _device"); \
        return; \
    } \
    makeContextCurrent(target); \
    if(_device->current_context_target == NULL) \
    { \
        PushErrorCode(function_name, ERROR_USER_ERROR, "NULL context"); \
        return; \
    } \
    if(!bindFramebuffer(target)) \
    { \
        PushErrorCode(function_name, ERROR_BACKEND_ERROR, "Failed to bind framebuffer."); \
        return; \
    } \
    prepareToRenderToTarget(target); \
    prepareToRenderShapes(shape); \
    cdata = (ContextData*)_device->current_context_target->context->data; \
    if(cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices) \
    { \
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices))) \
            FlushBlitBuffer(); \
    } \
    if(cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices) \
    { \
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices))) \
            FlushBlitBuffer(); \
    } \
    blit_buffer = cdata->blit_buffer; \
    index_buffer = cdata->index_buffer; \
    vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX; \
    if(target->use_color) \
    { \
        r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, color.r); \
        g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, color.g); \
        b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, color.b); \
        a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, color.a); \
    } \
    else \
    { \
        r = color.r/255.0f; \
        g = color.g/255.0f; \
        b = color.b/255.0f; \
        a = color.a/255.0f; \
    } \
    blit_buffer_starting_index = cdata->blit_buffer_num_vertices;


#define BEGIN_UNTEXTURED_NOCOLOR(function_name, shape, num_additional_vertices, num_additional_indices) \
	ContextData* cdata; \
	float* blit_buffer; \
	unsigned short* index_buffer; \
	int vert_index; \
	int color_index; \
	unsigned short blit_buffer_starting_index; \
    if(target == NULL) \
    { \
        PushErrorCode(function_name, ERROR_NULL_ARGUMENT, "target"); \
        return; \
    } \
    if(_device != target->renderer) \
    { \
        PushErrorCode(function_name, ERROR_USER_ERROR, "Mismatched _device"); \
        return; \
    } \
    makeContextCurrent(target); \
    if(_device->current_context_target == NULL) \
    { \
        PushErrorCode(function_name, ERROR_USER_ERROR, "NULL context"); \
        return; \
    } \
    if(!bindFramebuffer(target)) \
    { \
        PushErrorCode(function_name, ERROR_BACKEND_ERROR, "Failed to bind framebuffer."); \
        return; \
    } \
    prepareToRenderToTarget(target); \
    prepareToRenderShapes(shape); \
    cdata = (ContextData*)_device->current_context_target->context->data; \
    if(cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices) \
    { \
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices))) \
            FlushBlitBuffer(); \
    } \
    if(cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices) \
    { \
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices))) \
            FlushBlitBuffer(); \
    } \
    blit_buffer = cdata->blit_buffer; \
    index_buffer = cdata->index_buffer; \
    vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX; \
    color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX; \
    blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

float Renderer::SetLineThickness(float thickness)
{
	float old;

	if (_device->current_context_target == NULL)
		return 1.0f;

	old = _device->current_context_target->context->line_thickness;
	if (old != thickness)
		_device->current_context_target->context->line_thickness = thickness;
	return old;
}

float Renderer::GetLineThickness()
{
	return _device->current_context_target->context->line_thickness;
}

void Renderer::Pixel(Target* target, float x, float y, SDL_Color color)
{
	BEGIN_UNTEXTURED("Pixel", GL_POINTS, 1, 1);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);
}

void Renderer::Line(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	float thickness = GetLineThickness();

	float t = thickness / 2;
	float line_angle = atan2f(y2 - y1, x2 - x1);
	float tc = t*cosf(line_angle);
	float ts = t*sinf(line_angle);

	BEGIN_UNTEXTURED("Line", GL_TRIANGLES, 4, 6);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	SET_UNTEXTURED_VERTEX(x1 + ts, y1 - tc, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x1 - ts, y1 + tc, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x2 + ts, y2 - tc, r, g, b, a);

	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	SET_UNTEXTURED_VERTEX(x2 - ts, y2 + tc, r, g, b, a);
}

// Arc() might call Circle()
void Renderer::Arc(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color)
{
	float dx, dy;
	int i;

	float t = GetLineThickness() / 2;
	float inner_radius = radius - t;
	float outer_radius = radius + t;

	float dt;
	int numSegments;

	float tempx;
	float c, s;

	if (inner_radius < 0.0f)
		inner_radius = 0.0f;

	if (start_angle > end_angle)
	{
		float swapa = end_angle;
		end_angle = start_angle;
		start_angle = swapa;
	}
	if (start_angle == end_angle)
		return;

	// Big angle
	if (end_angle - start_angle >= 360)
	{
		Circle(target, x, y, radius, color);
		return;
	}

	// Shift together
	while (start_angle < 0 && end_angle < 0)
	{
		start_angle += 360;
		end_angle += 360;
	}
	while (start_angle > 360 && end_angle > 360)
	{
		start_angle -= 360;
		end_angle -= 360;
	}


	dt = ((end_angle - start_angle) / 360)*(1.25f / sqrtf(outer_radius));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.

	numSegments = (fabs(end_angle - start_angle)*M_PI / 180) / dt;
	if (numSegments == 0)
		return;

	{
		BEGIN_UNTEXTURED("Arc", GL_TRIANGLES, 2 * (numSegments), 6 * (numSegments));
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif		
		c = cos(dt);
		s = sin(dt);

		// Rotate to start
		start_angle *= M_PI / 180;
		dx = cos(start_angle);
		dy = sin(start_angle);

		BEGIN_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);

		for (i = 1; i < numSegments; i++)
		{
			tempx = c * dx - s * dy;
			dy = s * dx + c * dy;
			dx = tempx;
			SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
		}

		// Last point
		end_angle *= M_PI / 180;
		dx = cos(end_angle);
		dy = sin(end_angle);
		END_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
	}
}

void Renderer::ArcFilled(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color)
{
	float dx, dy;
	int i;

	float dt;
	int numSegments;

	float tempx;
	float c, s;

	if (start_angle > end_angle)
	{
		float swapa = end_angle;
		end_angle = start_angle;
		start_angle = swapa;
	}
	if (start_angle == end_angle)
		return;

	// Big angle
	if (end_angle - start_angle >= 360)
	{
		CircleFilled(target, x, y, radius, color);
		return;
	}

	// Shift together
	while (start_angle < 0 && end_angle < 0)
	{
		start_angle += 360;
		end_angle += 360;
	}
	while (start_angle > 360 && end_angle > 360)
	{
		start_angle -= 360;
		end_angle -= 360;
	}

	dt = ((end_angle - start_angle) / 360)*(1.25f / sqrtf(radius));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.

	numSegments = (fabs(end_angle - start_angle)*M_PI / 180) / dt;
	if (numSegments == 0)
		return;

	{
		BEGIN_UNTEXTURED("ArcFilled", GL_TRIANGLES, 3 + (numSegments - 1) + 1, 3 + (numSegments - 1) * 3 + 3);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif        
		c = cos(dt);
		s = sin(dt);

		// Rotate to start
		start_angle *= M_PI / 180;
		dx = cos(start_angle);
		dy = sin(start_angle);

		// First triangle
		SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);
		SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // first point

		tempx = c * dx - s * dy;
		dy = s * dx + c * dy;
		dx = tempx;
		SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // new point

		for (i = 2; i < numSegments + 1; i++)
		{
			tempx = c * dx - s * dy;
			dy = s * dx + c * dy;
			dx = tempx;
			SET_INDEXED_VERTEX(0);  // center
			SET_INDEXED_VERTEX(i);  // last point
			SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // new point
		}

		// Last triangle
		end_angle *= M_PI / 180;
		dx = cos(end_angle);
		dy = sin(end_angle);
		SET_INDEXED_VERTEX(0);  // center
		SET_INDEXED_VERTEX(i);  // last point
		SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // new point
	}
}


/*
Incremental rotation circle algorithm
*/

void Renderer::Circle(Target* target, float x, float y, float radius, SDL_Color color)
{
	float thickness = GetLineThickness();
	float dx, dy;
	int i;
	float t = thickness / 2;
	float inner_radius = radius - t;
	float outer_radius = radius + t;
	float dt = (1.25f / sqrtf(outer_radius));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
	int numSegments = 2 * M_PI / dt + 1;

	float tempx;
	float c = cos(dt);
	float s = sin(dt);

	BEGIN_UNTEXTURED("Circle", GL_TRIANGLES, 2 * (numSegments), 6 * (numSegments));
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	if (inner_radius < 0.0f)
		inner_radius = 0.0f;

	dx = 1.0f;
	dy = 0.0f;

	BEGIN_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);

	for (i = 1; i < numSegments; i++)
	{
		tempx = c * dx - s * dy;
		dy = s * dx + c * dy;
		dx = tempx;

		SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
	}

	LOOP_UNTEXTURED_SEGMENTS();  // back to the beginning
}

void Renderer::CircleFilled(Target* target, float x, float y, float radius, SDL_Color color)
{
	float dt = (1.25f / sqrtf(radius));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
	float dx, dy;
	int numSegments = 2 * M_PI / dt + 1;
	int i;

	float tempx;
	float c = cos(dt);
	float s = sin(dt);

	BEGIN_UNTEXTURED("CircleFilled", GL_TRIANGLES, 3 + (numSegments - 2), 3 + (numSegments - 2) * 3 + 3);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	// First triangle
	SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);  // Center

	dx = 1.0f;
	dy = 0.0f;
	SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // first point

	tempx = c * dx - s * dy;
	dy = s * dx + c * dy;
	dx = tempx;
	SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // new point

	for (i = 2; i < numSegments; i++)
	{
		tempx = c * dx - s * dy;
		dy = s * dx + c * dy;
		dx = tempx;
		SET_INDEXED_VERTEX(0);  // center
		SET_INDEXED_VERTEX(i);  // last point
		SET_UNTEXTURED_VERTEX(x + radius*dx, y + radius*dy, r, g, b, a); // new point
	}

	SET_INDEXED_VERTEX(0);  // center
	SET_INDEXED_VERTEX(i);  // last point
	SET_INDEXED_VERTEX(1);  // first point
}

void Renderer::Ellipse(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color)
{
	float thickness = GetLineThickness();
	float dx, dy;
	int i;
	float t = thickness / 2;
	float rot_x = cos(degrees*M_PI / 180);
	float rot_y = sin(degrees*M_PI / 180);
	float inner_radius_x = rx - t;
	float outer_radius_x = rx + t;
	float inner_radius_y = ry - t;
	float outer_radius_y = ry + t;
	float dt = (1.25f / sqrtf(outer_radius_x > outer_radius_y ? outer_radius_x : outer_radius_y));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
	int numSegments = 2 * M_PI / dt + 1;

	float tempx;
	float c = cos(dt);
	float s = sin(dt);
	float inner_trans_x, inner_trans_y;
	float outer_trans_x, outer_trans_y;

	BEGIN_UNTEXTURED("Ellipse", GL_TRIANGLES, 2 * (numSegments), 6 * (numSegments));
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	if (inner_radius_x < 0.0f)
		inner_radius_x = 0.0f;
	if (inner_radius_y < 0.0f)
		inner_radius_y = 0.0f;

	dx = 1.0f;
	dy = 0.0f;

	inner_trans_x = rot_x * inner_radius_x*dx - rot_y * inner_radius_y*dy;
	inner_trans_y = rot_y * inner_radius_x*dx + rot_x * inner_radius_y*dy;
	outer_trans_x = rot_x * outer_radius_x*dx - rot_y * outer_radius_y*dy;
	outer_trans_y = rot_y * outer_radius_x*dx + rot_x * outer_radius_y*dy;
	BEGIN_UNTEXTURED_SEGMENTS(x + inner_trans_x, y + inner_trans_y, x + outer_trans_x, y + outer_trans_y, r, g, b, a);

	for (i = 1; i < numSegments; i++)
	{
		tempx = c * dx - s * dy;
		dy = (s * dx + c * dy);
		dx = tempx;

		inner_trans_x = rot_x * inner_radius_x*dx - rot_y * inner_radius_y*dy;
		inner_trans_y = rot_y * inner_radius_x*dx + rot_x * inner_radius_y*dy;
		outer_trans_x = rot_x * outer_radius_x*dx - rot_y * outer_radius_y*dy;
		outer_trans_y = rot_y * outer_radius_x*dx + rot_x * outer_radius_y*dy;
		SET_UNTEXTURED_SEGMENTS(x + inner_trans_x, y + inner_trans_y, x + outer_trans_x, y + outer_trans_y, r, g, b, a);
	}

	LOOP_UNTEXTURED_SEGMENTS();  // back to the beginning
}

void Renderer::EllipseFilled(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color)
{
	float dx, dy;
	int i;
	float rot_x = cos(degrees*M_PI / 180);
	float rot_y = sin(degrees*M_PI / 180);
	float dt = (1.25f / sqrtf(rx > ry ? rx : ry));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
	int numSegments = 2 * M_PI / dt + 1;

	float tempx;
	float c = cos(dt);
	float s = sin(dt);
	float trans_x, trans_y;

	BEGIN_UNTEXTURED("EllipseFilled", GL_TRIANGLES, 3 + (numSegments - 2), 3 + (numSegments - 2) * 3 + 3);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	// First triangle
	SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);  // Center

	dx = 1.0f;
	dy = 0.0f;
	trans_x = rot_x * rx*dx - rot_y * ry*dy;
	trans_y = rot_y * rx*dx + rot_x * ry*dy;
	SET_UNTEXTURED_VERTEX(x + trans_x, y + trans_y, r, g, b, a); // first point

	tempx = c * dx - s * dy;
	dy = s * dx + c * dy;
	dx = tempx;

	trans_x = rot_x * rx*dx - rot_y * ry*dy;
	trans_y = rot_y * rx*dx + rot_x * ry*dy;
	SET_UNTEXTURED_VERTEX(x + trans_x, y + trans_y, r, g, b, a); // new point

	for (i = 2; i < numSegments; i++)
	{
		tempx = c * dx - s * dy;
		dy = (s * dx + c * dy);
		dx = tempx;

		trans_x = rot_x * rx*dx - rot_y * ry*dy;
		trans_y = rot_y * rx*dx + rot_x * ry*dy;

		SET_INDEXED_VERTEX(0);  // center
		SET_INDEXED_VERTEX(i);  // last point
		SET_UNTEXTURED_VERTEX(x + trans_x, y + trans_y, r, g, b, a); // new point
	}

	SET_INDEXED_VERTEX(0);  // center
	SET_INDEXED_VERTEX(i);  // last point
	SET_INDEXED_VERTEX(1);  // first point
}

void Renderer::Sector(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color)
{
	bool circled;
	float dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;

	if (inner_radius < 0.0f)
		inner_radius = 0.0f;
	if (outer_radius < 0.0f)
		outer_radius = 0.0f;

	if (inner_radius > outer_radius)
	{
		float s = inner_radius;
		inner_radius = outer_radius;
		outer_radius = s;
	}

	if (start_angle > end_angle)
	{
		float swapa = end_angle;
		end_angle = start_angle;
		start_angle = swapa;
	}
	if (start_angle == end_angle)
		return;

	if (inner_radius == outer_radius)
	{
		Arc(target, x, y, inner_radius, start_angle, end_angle, color);
		return;
	}

	circled = (end_angle - start_angle >= 360);
	// Composited shape...  But that means error codes may be confusing. :-/
	Arc(target, x, y, inner_radius, start_angle, end_angle, color);

	if (!circled)
	{
		dx1 = inner_radius*cos(end_angle*RADPERDEG);
		dy1 = inner_radius*sin(end_angle*RADPERDEG);
		dx2 = outer_radius*cos(end_angle*RADPERDEG);
		dy2 = outer_radius*sin(end_angle*RADPERDEG);
		Line(target, x + dx1, y + dy1, x + dx2, y + dy2, color);
	}

	Arc(target, x, y, outer_radius, start_angle, end_angle, color);

	if (!circled)
	{
		dx3 = inner_radius*cos(start_angle*RADPERDEG);
		dy3 = inner_radius*sin(start_angle*RADPERDEG);
		dx4 = outer_radius*cos(start_angle*RADPERDEG);
		dy4 = outer_radius*sin(start_angle*RADPERDEG);
		Line(target, x + dx3, y + dy3, x + dx4, y + dy4, color);
	}
}

void Renderer::SectorFilled(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color)
{
	float t;
	float dt;
	float dx, dy;

	int numSegments;

	if (inner_radius < 0.0f)
		inner_radius = 0.0f;
	if (outer_radius < 0.0f)
		outer_radius = 0.0f;

	if (inner_radius > outer_radius)
	{
		float s = inner_radius;
		inner_radius = outer_radius;
		outer_radius = s;
	}

	if (inner_radius == outer_radius)
	{
		Arc(target, x, y, inner_radius, start_angle, end_angle, color);
		return;
	}


	if (start_angle > end_angle)
	{
		float swapa = end_angle;
		end_angle = start_angle;
		start_angle = swapa;
	}
	if (start_angle == end_angle)
		return;

	if (end_angle - start_angle >= 360)
		end_angle = start_angle + 360;


	t = start_angle;
	dt = ((end_angle - start_angle) / 360)*(1.25f / sqrtf(outer_radius)) * DEGPERRAD;  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.

	numSegments = fabs(end_angle - start_angle) / dt;
	if (numSegments == 0)
		return;

	{
		int i;
		bool use_inner;
		BEGIN_UNTEXTURED("SectorFilled", GL_TRIANGLES, 3 + (numSegments - 1) + 1, 3 + (numSegments - 1) * 3 + 3);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif
		use_inner = false;  // Switches between the radii for the next point

							// First triangle
		dx = inner_radius*cos(t*RADPERDEG);
		dy = inner_radius*sin(t*RADPERDEG);
		SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a);

		dx = outer_radius*cos(t*RADPERDEG);
		dy = outer_radius*sin(t*RADPERDEG);
		SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a);
		t += dt;
		dx = inner_radius*cos(t*RADPERDEG);
		dy = inner_radius*sin(t*RADPERDEG);
		SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a);
		t += dt;

		for (i = 2; i < numSegments + 1; i++)
		{
			SET_INDEXED_VERTEX(i - 1);
			SET_INDEXED_VERTEX(i);
			if (use_inner)
			{
				dx = inner_radius*cos(t*RADPERDEG);
				dy = inner_radius*sin(t*RADPERDEG);
			}
			else
			{
				dx = outer_radius*cos(t*RADPERDEG);
				dy = outer_radius*sin(t*RADPERDEG);
			}
			SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a); // new point
			t += dt;
			use_inner = !use_inner;
		}

		// Last quad
		t = end_angle;
		if (use_inner)
		{
			dx = inner_radius*cos(t*RADPERDEG);
			dy = inner_radius*sin(t*RADPERDEG);
		}
		else
		{
			dx = outer_radius*cos(t*RADPERDEG);
			dy = outer_radius*sin(t*RADPERDEG);
		}
		SET_INDEXED_VERTEX(i - 1);
		SET_INDEXED_VERTEX(i);
		SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a); // new point
		use_inner = !use_inner;
		i++;

		if (use_inner)
		{
			dx = inner_radius*cos(t*RADPERDEG);
			dy = inner_radius*sin(t*RADPERDEG);
		}
		else
		{
			dx = outer_radius*cos(t*RADPERDEG);
			dy = outer_radius*sin(t*RADPERDEG);
		}
		SET_INDEXED_VERTEX(i - 1);
		SET_INDEXED_VERTEX(i);
		SET_UNTEXTURED_VERTEX(x + dx, y + dy, r, g, b, a); // new point
	}
}

void Renderer::Tri(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
	BEGIN_UNTEXTURED("Tri", GL_LINES, 3, 6);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif    
	SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);

	SET_INDEXED_VERTEX(1);
	SET_UNTEXTURED_VERTEX(x3, y3, r, g, b, a);

	SET_INDEXED_VERTEX(2);
	SET_INDEXED_VERTEX(0);
}

void Renderer::TriFilled(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
	BEGIN_UNTEXTURED("TriFilled", GL_TRIANGLES, 3, 3);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif    
	SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x3, y3, r, g, b, a);
}

void Renderer::Rectangle(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	if (y2 < y1)
	{
		float y = y1;
		y1 = y2;
		y2 = y;
	}
	if (x2 < x1)
	{
		float x = x1;
		x1 = x2;
		x2 = x;
	}

	{
		float thickness = GetLineThickness();

		// Thickness offsets
		float outer = thickness / 2;
		float inner_x = outer;
		float inner_y = outer;

		// Thick lines via filled triangles

		BEGIN_UNTEXTURED("Rectangle", GL_TRIANGLES, 12, 24);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif		
		// Adjust inner thickness offsets to avoid overdraw on narrow/small rects
		if (x1 + inner_x > x2 - inner_x)
			inner_x = (x2 - x1) / 2;
		if (y1 + inner_y > y2 - inner_y)
			inner_y = (y2 - y1) / 2;

		// First triangle
		SET_UNTEXTURED_VERTEX(x1 - outer, y1 - outer, r, g, b, a); // 0
		SET_UNTEXTURED_VERTEX(x1 - outer, y1 + inner_y, r, g, b, a); // 1
		SET_UNTEXTURED_VERTEX(x2 + outer, y1 - outer, r, g, b, a); // 2

		SET_INDEXED_VERTEX(2);
		SET_INDEXED_VERTEX(1);
		SET_UNTEXTURED_VERTEX(x2 + outer, y1 + inner_y, r, g, b, a); // 3

		SET_INDEXED_VERTEX(3);
		SET_UNTEXTURED_VERTEX(x2 - inner_x, y1 + inner_y, r, g, b, a); // 4
		SET_UNTEXTURED_VERTEX(x2 - inner_x, y2 - inner_y, r, g, b, a); // 5

		SET_INDEXED_VERTEX(3);
		SET_INDEXED_VERTEX(5);
		SET_UNTEXTURED_VERTEX(x2 + outer, y2 - inner_y, r, g, b, a); // 6

		SET_INDEXED_VERTEX(6);
		SET_UNTEXTURED_VERTEX(x1 - outer, y2 - inner_y, r, g, b, a); // 7
		SET_UNTEXTURED_VERTEX(x2 + outer, y2 + outer, r, g, b, a); // 8

		SET_INDEXED_VERTEX(7);
		SET_UNTEXTURED_VERTEX(x1 - outer, y2 + outer, r, g, b, a); // 9
		SET_INDEXED_VERTEX(8);

		SET_INDEXED_VERTEX(7);
		SET_UNTEXTURED_VERTEX(x1 + inner_x, y2 - inner_y, r, g, b, a); // 10
		SET_INDEXED_VERTEX(1);

		SET_INDEXED_VERTEX(1);
		SET_INDEXED_VERTEX(10);
		SET_UNTEXTURED_VERTEX(x1 + inner_x, y1 + inner_y, r, g, b, a); // 11
	}
}

void Renderer::RectangleFilled(Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
	BEGIN_UNTEXTURED("RectangleFilled", GL_TRIANGLES, 4, 6);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x1, y2, r, g, b, a);
	SET_UNTEXTURED_VERTEX(x2, y1, r, g, b, a);

	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
}

#define INCREMENT_CIRCLE \
    tempx = c * dx - s * dy; \
    dy = s * dx + c * dy; \
    dx = tempx; \
    ++i;

void Renderer::RectangleRound(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
	if (y2 < y1)
	{
		float temp = y2;
		y2 = y1;
		y1 = temp;
	}
	if (x2 < x1)
	{
		float temp = x2;
		x2 = x1;
		x1 = temp;
	}

	if (radius >(x2 - x1) / 2)
		radius = (x2 - x1) / 2;
	if (radius > (y2 - y1) / 2)
		radius = (y2 - y1) / 2;

	x1 += radius;
	y1 += radius;
	x2 -= radius;
	y2 -= radius;

	{
		float thickness = GetLineThickness();
		float dx, dy;
		int i = 0;
		float t = thickness / 2;
		float inner_radius = radius - t;
		float outer_radius = radius + t;
		float dt = (1.25f / sqrtf(outer_radius));  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
		int numSegments = 2 * M_PI / dt + 1;
		if (numSegments < 4)
			numSegments = 4;

		// Make a multiple of 4 so we can have even corners
		numSegments += numSegments % 4;

		dt = 2 * M_PI / (numSegments - 1);

		{
			float x, y;
			int go_to_second = numSegments / 4;
			int go_to_third = numSegments / 2;
			int go_to_fourth = 3 * numSegments / 4;

			float tempx;
			float c = cos(dt);
			float s = sin(dt);

			// Add another 4 for the extra corner vertices
			BEGIN_UNTEXTURED("RectangleRound", GL_TRIANGLES, 2 * (numSegments + 4), 6 * (numSegments + 4));
#ifdef PREMULTIPLIED_ALPHA
			r *= a; g *= a; b *= a;
#endif            
			if (inner_radius < 0.0f)
				inner_radius = 0.0f;

			dx = 1.0f;
			dy = 0.0f;

			x = x2;
			y = y2;
			BEGIN_UNTEXTURED_SEGMENTS(x + inner_radius, y, x + outer_radius, y, r, g, b, a);
			while (i < go_to_second - 1)
			{
				INCREMENT_CIRCLE;

				SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
			}
			INCREMENT_CIRCLE;

			SET_UNTEXTURED_SEGMENTS(x, y + inner_radius, x, y + outer_radius, r, g, b, a);
			x = x1;
			y = y2;
			SET_UNTEXTURED_SEGMENTS(x, y + inner_radius, x, y + outer_radius, r, g, b, a);
			while (i < go_to_third - 1)
			{
				INCREMENT_CIRCLE;

				SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
			}
			INCREMENT_CIRCLE;

			SET_UNTEXTURED_SEGMENTS(x - inner_radius, y, x - outer_radius, y, r, g, b, a);
			x = x1;
			y = y1;
			SET_UNTEXTURED_SEGMENTS(x - inner_radius, y, x - outer_radius, y, r, g, b, a);
			while (i < go_to_fourth - 1)
			{
				INCREMENT_CIRCLE;

				SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
			}
			INCREMENT_CIRCLE;

			SET_UNTEXTURED_SEGMENTS(x, y - inner_radius, x, y - outer_radius, r, g, b, a);
			x = x2;
			y = y1;
			SET_UNTEXTURED_SEGMENTS(x, y - inner_radius, x, y - outer_radius, r, g, b, a);
			while (i < numSegments - 1)
			{
				INCREMENT_CIRCLE;

				SET_UNTEXTURED_SEGMENTS(x + inner_radius*dx, y + inner_radius*dy, x + outer_radius*dx, y + outer_radius*dy, r, g, b, a);
			}
			SET_UNTEXTURED_SEGMENTS(x + inner_radius, y, x + outer_radius, y, r, g, b, a);

			LOOP_UNTEXTURED_SEGMENTS();  // back to the beginning
		}
	}
}

void Renderer::RectangleRoundFilled(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
	if (y2 < y1)
	{
		float temp = y2;
		y2 = y1;
		y1 = temp;
	}
	if (x2 < x1)
	{
		float temp = x2;
		x2 = x1;
		x1 = temp;
	}

	if (radius >(x2 - x1) / 2)
		radius = (x2 - x1) / 2;
	if (radius > (y2 - y1) / 2)
		radius = (y2 - y1) / 2;

	{
		float tau = 2 * M_PI;

		int verts_per_corner = 7;
		float corner_angle_increment = (tau / 4) / (verts_per_corner - 1);  // 0, 15, 30, 45, 60, 75, 90

																			// Starting angle
		float angle = tau*0.75f;
		int last_index = 2;
		int i;

		BEGIN_UNTEXTURED("RectangleRoundFilled", GL_TRIANGLES, 6 + 4 * (verts_per_corner - 1) - 1, 15 + 4 * (verts_per_corner - 1) * 3 - 3);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif

		// First triangle
		SET_UNTEXTURED_VERTEX((x2 + x1) / 2, (y2 + y1) / 2, r, g, b, a);  // Center
		SET_UNTEXTURED_VERTEX(x2 - radius + cos(angle)*radius, y1 + radius + sin(angle)*radius, r, g, b, a);
		angle += corner_angle_increment;
		SET_UNTEXTURED_VERTEX(x2 - radius + cos(angle)*radius, y1 + radius + sin(angle)*radius, r, g, b, a);
		angle += corner_angle_increment;

		for (i = 2; i < verts_per_corner; i++)
		{
			SET_INDEXED_VERTEX(0);
			SET_INDEXED_VERTEX(last_index++);
			SET_UNTEXTURED_VERTEX(x2 - radius + cos(angle)*radius, y1 + radius + sin(angle)*radius, r, g, b, a);
			angle += corner_angle_increment;
		}

		SET_INDEXED_VERTEX(0);
		SET_INDEXED_VERTEX(last_index++);
		SET_UNTEXTURED_VERTEX(x2 - radius + cos(angle)*radius, y2 - radius + sin(angle)*radius, r, g, b, a);
		for (i = 1; i < verts_per_corner; i++)
		{
			SET_INDEXED_VERTEX(0);
			SET_INDEXED_VERTEX(last_index++);
			SET_UNTEXTURED_VERTEX(x2 - radius + cos(angle)*radius, y2 - radius + sin(angle)*radius, r, g, b, a);
			angle += corner_angle_increment;
		}

		SET_INDEXED_VERTEX(0);
		SET_INDEXED_VERTEX(last_index++);
		SET_UNTEXTURED_VERTEX(x1 + radius + cos(angle)*radius, y2 - radius + sin(angle)*radius, r, g, b, a);
		for (i = 1; i < verts_per_corner; i++)
		{
			SET_INDEXED_VERTEX(0);
			SET_INDEXED_VERTEX(last_index++);
			SET_UNTEXTURED_VERTEX(x1 + radius + cos(angle)*radius, y2 - radius + sin(angle)*radius, r, g, b, a);
			angle += corner_angle_increment;
		}

		SET_INDEXED_VERTEX(0);
		SET_INDEXED_VERTEX(last_index++);
		SET_UNTEXTURED_VERTEX(x1 + radius + cos(angle)*radius, y1 + radius + sin(angle)*radius, r, g, b, a);
		for (i = 1; i < verts_per_corner; i++)
		{
			SET_INDEXED_VERTEX(0);
			SET_INDEXED_VERTEX(last_index++);
			SET_UNTEXTURED_VERTEX(x1 + radius + cos(angle)*radius, y1 + radius + sin(angle)*radius, r, g, b, a);
			angle += corner_angle_increment;
		}

		// Last triangle
		SET_INDEXED_VERTEX(0);
		SET_INDEXED_VERTEX(last_index++);
		SET_INDEXED_VERTEX(1);
	}
}

void Renderer::Polygon(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
	if (num_vertices < 3)
		return;

	{
		int numSegments = 2 * num_vertices;
		int last_index = 0;
		int i;

		BEGIN_UNTEXTURED("Polygon", GL_LINES, num_vertices, numSegments);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif
		SET_UNTEXTURED_VERTEX(vertices[0], vertices[1], r, g, b, a);
		for (i = 2; i < numSegments; i += 2)
		{
			SET_UNTEXTURED_VERTEX(vertices[i], vertices[i + 1], r, g, b, a);
			last_index++;
			SET_INDEXED_VERTEX(last_index);  // Double the last one for the next line
		}
		SET_INDEXED_VERTEX(0);
	}
}

void Renderer::PolygonFilled(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
	if (num_vertices < 3)
		return;

	{
		int numSegments = 2 * num_vertices;

		// Using a fan of triangles assumes that the polygon is convex
		BEGIN_UNTEXTURED("PolygonFilled", GL_TRIANGLES, num_vertices, 3 + (num_vertices - 3) * 3);
#ifdef PREMULTIPLIED_ALPHA
		r *= a; g *= a; b *= a;
#endif
		// First triangle
		SET_UNTEXTURED_VERTEX(vertices[0], vertices[1], r, g, b, a);
		SET_UNTEXTURED_VERTEX(vertices[2], vertices[3], r, g, b, a);
		SET_UNTEXTURED_VERTEX(vertices[4], vertices[5], r, g, b, a);

		if (num_vertices > 3)
		{
			int last_index = 2;
			int i;
			for (i = 6; i < numSegments; i += 2)
			{
				SET_INDEXED_VERTEX(0);  // Start from the first vertex
				SET_INDEXED_VERTEX(last_index);  // Double the last one
				SET_UNTEXTURED_VERTEX(vertices[i], vertices[i + 1], r, g, b, a);
				last_index++;
			}
		}
	}
}

void Renderer::PolygonTextureFilled(Target* target, unsigned int num_vertices, float* vertices, Image* image, float texture_x, float texture_y)
{
	Uint32 tex_w, tex_h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index, tex_index, color_index;
	int i;
	float r, g, b, a;

	if (num_vertices < 3) return;
	int numSegments = 2 * num_vertices;

	if (image == NULL) {
		PushErrorCode("PolygonTextureFilled", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("PolygonTextureFilled", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("PolygonTextureFilled", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}
	makeContextCurrent(target);
	if (_device->current_context_target == NULL) {
		PushErrorCode("PolygonTextureFilled", ERROR_USER_ERROR, "NULL context");
		return;
	}
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);
	// Bind the texture to which subsequent calls refer
	bindTexture(image);
	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("PolygonTextureFilled", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}
	tex_w = image->texture_w;
	tex_h = image->texture_h;
	if (image->using_virtual_resolution)
	{
		// Scale texture coords to fit the original dims		
		tex_w *= (float)image->w / image->base_w;
		tex_h *= (float)image->h / image->base_h;
	}

	cdata = (ContextData*)_device->current_context_target->context->data;
	int num_additional_vertices = num_vertices;
	int num_additional_indices = 3 + (num_vertices - 3) * 3;
	if (cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices)
	{
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices)))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices)
	{
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices)))
			FlushBlitBuffer();
	}
	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;

	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	if (target->use_color) {
		r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
		g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
		b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
		a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
	}
	else {
		r = image->color.r / 255.0f;
		g = image->color.g / 255.0f;
		b = image->color.b / 255.0f;
		a = image->color.a / 255.0f;
	}
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	// First triangle
	SET_TEXTURED_VERTEX(vertices[0], vertices[1], (vertices[0] - texture_x) / tex_w, (vertices[1] - texture_y) / tex_h, r, g, b, a);
	SET_TEXTURED_VERTEX(vertices[2], vertices[3], (vertices[2] - texture_x) / tex_w, (vertices[3] - texture_y) / tex_h, r, g, b, a);
	SET_TEXTURED_VERTEX(vertices[4], vertices[5], (vertices[4] - texture_x) / tex_w, (vertices[5] - texture_y) / tex_h, r, g, b, a);

	if (num_vertices > 3) {
		int last_index = 2;
		for (i = 6; i < numSegments; i += 2) {
			SET_INDEXED_VERTEX(0);  // Start from the first vertex
			SET_INDEXED_VERTEX(last_index);  // Double the last one
			SET_TEXTURED_VERTEX(vertices[i], vertices[i + 1], (vertices[i] - texture_x) / tex_w, (vertices[i + 1] - texture_y) / tex_h, r, g, b, a);
			last_index++;
		}
	}
}

void Renderer::PolygonTextureFilledNPOT(Target* target, unsigned int num_vertices, float* vertices, Image* image)
{
	Uint32 frame_w, frame_h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index, tex_index, color_index;
	int i;
	float r, g, b, a;

	if (num_vertices < 3) return;
	int numSegments = 2 * num_vertices;

	if (image == NULL) {
		PushErrorCode("PolygonTextureFilledNPOT", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("PolygonTextureFilledNPOT", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("PolygonTextureFilledNPOT", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}
	makeContextCurrent(target);
	if (_device->current_context_target == NULL) {
		PushErrorCode("PolygonTextureFilledNPOT", ERROR_USER_ERROR, "NULL context");
		return;
	}
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);
	// Bind the texture to which subsequent calls refer
	bindTexture(image);
	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("PolygonTextureFilledNPOT", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}
	frame_w = target->w;
	frame_h = target->h;

	cdata = (ContextData*)_device->current_context_target->context->data;
	int num_additional_vertices = num_vertices;
	int num_additional_indices = 3 + (num_vertices - 3) * 3;
	if (cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices)
	{
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices)))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices)
	{
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices)))
			FlushBlitBuffer();
	}
	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;

	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;

	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	if (target->use_color) {
		r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
		g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
		b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
		a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
	}
	else {
		r = image->color.r / 255.0f;
		g = image->color.g / 255.0f;
		b = image->color.b / 255.0f;
		a = image->color.a / 255.0f;
	}
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif

	// First triangle
	SET_TEXTURED_VERTEX(vertices[0], vertices[1], vertices[0] / frame_w, vertices[1] / frame_h, r, g, b, a);
	SET_TEXTURED_VERTEX(vertices[2], vertices[3], vertices[2] / frame_w, vertices[3] / frame_h, r, g, b, a);
	SET_TEXTURED_VERTEX(vertices[4], vertices[5], vertices[4] / frame_w, vertices[5] / frame_h, r, g, b, a);

	if (num_vertices > 3) {
		int last_index = 2;
		for (i = 6; i < numSegments; i += 2) {
			SET_INDEXED_VERTEX(0);  // Start from the first vertex
			SET_INDEXED_VERTEX(last_index);  // Double the last one
			SET_TEXTURED_VERTEX(vertices[i], vertices[i + 1], vertices[i] / frame_w, vertices[i + 1] / frame_h, r, g, b, a);
			last_index++;
		}
	}
}

void Renderer::TextureLine(Target* target, float x1, float y1, float x2, float y2, Image* image, float texture_x, float texture_y)
{
	Uint32 tex_w, tex_h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index;
	int tex_index;
	int color_index;
	float r, g, b, a;
	float thickness = GetLineThickness();
	float t = thickness / 2;
	float line_angle = atan2f(y2 - y1, x2 - x1);
	float tc = t * cosf(line_angle);
	float ts = t * sinf(line_angle);

	if (image == NULL) {
		PushErrorCode("TextureLine", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("TextureLine", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("TextureLine", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}
	makeContextCurrent(target);
	if (_device->current_context_target == NULL) {
		PushErrorCode("TextureLine", ERROR_USER_ERROR, "NULL context");
		return;
	}
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);
	// Bind the texture to which subsequent calls refer
	bindTexture(image);
	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("TextureLine", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}
	tex_w = image->texture_w;
	tex_h = image->texture_h;
	if (image->using_virtual_resolution)
	{
		// Scale texture coords to fit the original dims		
		tex_w *= (float)image->w / image->base_w;
		tex_h *= (float)image->h / image->base_h;
	}

	float num_additional_vertices = 4;
	float num_additional_indices = 6;
	cdata = (ContextData*)_device->current_context_target->context->data;
	if (cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices) {
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices)))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices) {
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices)))
			FlushBlitBuffer();
	}
	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;
	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;
	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	if (target->use_color) {
		r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
		g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
		b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
		a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
	}
	else {
		r = image->color.r / 255.0f;
		g = image->color.g / 255.0f;
		b = image->color.b / 255.0f;
		a = image->color.a / 255.0f;
	}
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif	
	SET_TEXTURED_VERTEX(x1 + ts, y1 - tc, ((x1 + ts) - texture_x) / tex_w, ((y1 - tc) - texture_y) / tex_h, r, g, b, a);
	SET_TEXTURED_VERTEX(x1 - ts, y1 + tc, ((x1 - ts) - texture_x) / tex_w, ((y1 + tc) - texture_y) / tex_h, r, g, b, a);
	SET_TEXTURED_VERTEX(x2 + ts, y2 - tc, ((x2 + ts) - texture_x) / tex_w, ((y2 - tc) - texture_y) / tex_h, r, g, b, a);
	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	SET_TEXTURED_VERTEX(x2 - ts, y2 + tc, ((x2 - ts) - texture_x) / tex_w, ((y2 + tc) - texture_y) / tex_h, r, g, b, a);
}

void Renderer::TextureLineNPOT(Target* target, float x1, float y1, float x2, float y2, Image* image)
{
	Uint32 frame_w, frame_h;
	ContextData* cdata;
	float* blit_buffer;
	unsigned short* index_buffer;
	unsigned short blit_buffer_starting_index;
	int vert_index;
	int tex_index;
	int color_index;
	float r, g, b, a;
	float thickness = GetLineThickness();
	float t = thickness / 2;
	float line_angle = atan2f(y2 - y1, x2 - x1);
	float tc = t * cosf(line_angle);
	float ts = t * sinf(line_angle);

	if (image == NULL) {
		PushErrorCode("TextureLineNPOT", ERROR_NULL_ARGUMENT, "image");
		return;
	}
	if (target == NULL) {
		PushErrorCode("TextureLineNPOT", ERROR_NULL_ARGUMENT, "target");
		return;
	}
	if (_device != image->renderer || _device != target->renderer) {
		PushErrorCode("TextureLineNPOT", ERROR_USER_ERROR, "Mismatched _device");
		return;
	}
	makeContextCurrent(target);
	if (_device->current_context_target == NULL) {
		PushErrorCode("TextureLineNPOT", ERROR_USER_ERROR, "NULL context");
		return;
	}
	prepareToRenderToTarget(target);
	prepareToRenderImage(target, image);
	// Bind the texture to which subsequent calls refer
	bindTexture(image);
	// Bind the FBO
	if (!bindFramebuffer(target)) {
		PushErrorCode("TextureLineNPOT", ERROR_BACKEND_ERROR, "Failed to bind framebuffer.");
		return;
	}
	frame_w = target->w;
	frame_h = target->h;

	float num_additional_vertices = 4;
	float num_additional_indices = 6;
	cdata = (ContextData*)_device->current_context_target->context->data;
	if (cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices) {
		if (!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices)))
			FlushBlitBuffer();
	}
	if (cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices) {
		if (!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices)))
			FlushBlitBuffer();
	}
	blit_buffer = cdata->blit_buffer;
	index_buffer = cdata->index_buffer;
	blit_buffer_starting_index = cdata->blit_buffer_num_vertices;
	vert_index = BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	tex_index = BLIT_BUFFER_TEX_COORD_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	color_index = BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*BLIT_BUFFER_FLOATS_PER_VERTEX;
	if (target->use_color) {
		r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, image->color.r);
		g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, image->color.g);
		b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, image->color.b);
		a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.a, image->color.a);
	}
	else {
		r = image->color.r / 255.0f;
		g = image->color.g / 255.0f;
		b = image->color.b / 255.0f;
		a = image->color.a / 255.0f;
	}
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif	
	SET_TEXTURED_VERTEX(x1 + ts, y1 - tc, (x1 + ts) / frame_w, (y1 - tc) / frame_h, r, g, b, a);
	SET_TEXTURED_VERTEX(x1 - ts, y1 + tc, (x1 - ts) / frame_w, (y1 + tc) / frame_h, r, g, b, a);
	SET_TEXTURED_VERTEX(x2 + ts, y2 - tc, (x2 + ts) / frame_w, (y2 - tc) / frame_h, r, g, b, a);
	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	SET_TEXTURED_VERTEX(x2 - ts, y2 + tc, (x2 - ts) / frame_w, (y2 + tc) / frame_h, r, g, b, a);
}

#define MAKE_VERTICE_COLOR(x, y) \
	color = colorfunc(x, y, userdata); \
	if (target->use_color) { \
		r = target->color.r / 255.0f * color.r; \
		g = target->color.g / 255.0f * color.g; \
		b = target->color.b / 255.0f * color.b; \
		a = target->color.a / 255.0f * color.a; \
	} \
	else { \
		r = color.r; g = color.g; b = color.b; a = color.a; \
	}

void Renderer::PolygonColorFilled(Target* target, unsigned int num_vertices, float* vertices, ColorCallback colorfunc, void* userdata)
{
	if (num_vertices < 3)
		return;

	int numSegments = 2 * num_vertices;
	// Using a fan of triangles assumes that the polygon is convex
	BEGIN_UNTEXTURED_NOCOLOR("PolygonColorFilled", GL_TRIANGLES, num_vertices, 3 + (num_vertices - 3) * 3);

	float r, g, b, a;
	GPU_Color color;
	// First triangle
	MAKE_VERTICE_COLOR(vertices[0], vertices[1]);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(vertices[0], vertices[1], r, g, b, a);
	MAKE_VERTICE_COLOR(vertices[2], vertices[3]);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(vertices[2], vertices[3], r, g, b, a);
	MAKE_VERTICE_COLOR(vertices[4], vertices[5]);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(vertices[4], vertices[5], r, g, b, a);

	if (num_vertices > 3)
	{
		int last_index = 2;
		int i = 0;
		for (i = 6; i < numSegments; i += 2)
		{
			SET_INDEXED_VERTEX(0);  // Start from the first vertex
			SET_INDEXED_VERTEX(last_index);  // Double the last one
			MAKE_VERTICE_COLOR(vertices[i], vertices[i + 1]);
#ifdef PREMULTIPLIED_ALPHA
			r *= a; g *= a; b *= a;
#endif
			SET_UNTEXTURED_VERTEX(vertices[i], vertices[i + 1], r, g, b, a);
			last_index++;
		}
	}
}

void Renderer::ColorLine(Target* target, float x1, float y1, float x2, float y2, ColorCallback colorfunc, void* userdata)
{
	float thickness = GetLineThickness();

	float t = thickness / 2;
	float line_angle = atan2f(y2 - y1, x2 - x1);
	float tc = t*cosf(line_angle);
	float ts = t*sinf(line_angle);

	BEGIN_UNTEXTURED_NOCOLOR("ColorLine", GL_TRIANGLES, 4, 6);

	float r, g, b, a;
	GPU_Color color;

	MAKE_VERTICE_COLOR(x1 + ts, y1 - tc);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x1 + ts, y1 - tc, r, g, b, a);
	MAKE_VERTICE_COLOR(x1 - ts, y1 + tc);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x1 - ts, y1 + tc, r, g, b, a);
	MAKE_VERTICE_COLOR(x2 + ts, y2 - tc);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x2 + ts, y2 - tc, r, g, b, a);

	SET_INDEXED_VERTEX(1);
	SET_INDEXED_VERTEX(2);
	MAKE_VERTICE_COLOR(x2 - ts, y2 + tc);
#ifdef PREMULTIPLIED_ALPHA
	r *= a; g *= a; b *= a;
#endif
	SET_UNTEXTURED_VERTEX(x2 - ts, y2 + tc, r, g, b, a);
}

NS_GPU_END