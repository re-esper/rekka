#include "xgpu.h"
#include "gpu_renderer.h"
#include "SDL_platform.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
	#define __func__ __FUNCTION__
	#pragma warning(push)
	// Visual Studio wants to complain about while(0)
	#pragma warning(disable: 4127)
	#define strcasecmp stricmp
	#define strncasecmp strnicmp
#endif

#include "stb_image.h"

#define CHECK_RENDERER (_gpu_current_device != NULL)
#define MAKE_CURRENT_IF_NONE(target) do{ if(_gpu_current_device->current_context_target == NULL && target != NULL && target->context != NULL) MakeCurrent(target, target->context->windowID); } while(0)
#define CHECK_CONTEXT (_gpu_current_device->current_context_target != NULL)
#define RETURN_ERROR(code, details) do{ PushErrorCode(__func__, code, "%s", details); return; } while(0)

NS_GPU_BEGIN

/*! A mapping of windowID to a Target to facilitate GetWindowTarget(). */
typedef struct WindowMapping
{
    Uint32 windowID;
    Target* target;
} WindowMapping;

static RenderDevice* _gpu_current_device = NULL;
static Renderer* _gpu_current_renderer = NULL;

#define INITIAL_WINDOW_MAPPINGS_SIZE 10
static WindowMapping* _gpu_window_mappings = NULL;
static int _gpu_window_mappings_size = 0;
static int _gpu_num_window_mappings = 0;

static Uint32 _gpu_init_windowID = 0;

static InitFlagEnum _gpu_preinit_flags = GPU_DEFAULT_INIT_FLAGS;
static InitFlagEnum _gpu_required_features = 0;

static bool _gpu_initialized_SDL_core = false;
static bool _gpu_initialized_SDL = false;

void gpu_init_error_queue(void);
void gpu_free_error_queue(void);

void ResetRendererState(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->ResetRendererState(_gpu_current_device);
}

Renderer* GetCurrentRenderer(void)
{
    return _gpu_current_renderer;
}

Uint32 GetCurrentShaderProgram(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;
    
    return _gpu_current_device->current_context_target->context->current_shader_program;
}


static bool gpu_init_SDL(void)
{
    if(!_gpu_initialized_SDL)
    {
        if(!_gpu_initialized_SDL_core && !SDL_WasInit(SDL_INIT_EVERYTHING))
        {
            // Nothing has been set up, so init SDL and the video subsystem.
            if(SDL_Init(SDL_INIT_VIDEO) < 0)
            {
                PushErrorCode("Init", ERROR_BACKEND_ERROR, "Failed to initialize SDL video subsystem");
                return false;
            }
            _gpu_initialized_SDL_core = true;
        }

        // SDL is definitely ready now, but we're going to init the video subsystem to be sure that SDL_gpu keeps it available until Quit().
        if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        {
            PushErrorCode("Init", ERROR_BACKEND_ERROR, "Failed to initialize SDL video subsystem");
            return false;
        }
        _gpu_initialized_SDL = true;
    }
    return true;
}

void SetInitWindow(Uint32 windowID)
{
    _gpu_init_windowID = windowID;
}

Uint32 GetInitWindow(void)
{
    return _gpu_init_windowID;
}

void SetPreInitFlags(InitFlagEnum flags)
{
    _gpu_preinit_flags = flags;
}

InitFlagEnum GetPreInitFlags(void)
{
    return _gpu_preinit_flags;
}

void SetRequiredFeatures(FeatureEnum features)
{
    _gpu_required_features = features;
}

FeatureEnum GetRequiredFeatures(void)
{
    return _gpu_required_features;
}

static void gpu_init_window_mappings(void)
{
    if(_gpu_window_mappings == NULL)
    {
        _gpu_window_mappings_size = INITIAL_WINDOW_MAPPINGS_SIZE;
        _gpu_window_mappings = (WindowMapping*)SDL_malloc(_gpu_window_mappings_size * sizeof(WindowMapping));
        _gpu_num_window_mappings = 0;
    }
}

void AddWindowMapping(Target* target)
{
    Uint32 windowID;
    int i;

    if(_gpu_window_mappings == NULL)
        gpu_init_window_mappings();

    if(target == NULL || target->context == NULL)
        return;

    windowID = target->context->windowID;
    if(windowID == 0)  // Invalid window ID
        return;

    // Check for duplicates
    for(i = 0; i < _gpu_num_window_mappings; i++)
    {
        if(_gpu_window_mappings[i].windowID == windowID)
        {
            if(_gpu_window_mappings[i].target != target)
                PushErrorCode(__func__, ERROR_DATA_ERROR, "WindowID %u already has a mapping.", windowID);
            return;
        }
        // Don't check the target because it's okay for a single target to be used with multiple windows
    }

    // Check if list is big enough to hold another
    if(_gpu_num_window_mappings >= _gpu_window_mappings_size)
    {
        WindowMapping* new_array;
        _gpu_window_mappings_size *= 2;
        new_array = (WindowMapping*)SDL_malloc(_gpu_window_mappings_size * sizeof(WindowMapping));
        memcpy(new_array, _gpu_window_mappings, _gpu_num_window_mappings * sizeof(WindowMapping));
        SDL_free(_gpu_window_mappings);
        _gpu_window_mappings = new_array;
    }

    // Add to end of list
    {
        WindowMapping m;
        m.windowID = windowID;
        m.target = target;
        _gpu_window_mappings[_gpu_num_window_mappings] = m;
    }
    _gpu_num_window_mappings++;
}

void RemoveWindowMapping(Uint32 windowID)
{
    int i;

    if(_gpu_window_mappings == NULL)
        gpu_init_window_mappings();

    if(windowID == 0)  // Invalid window ID
        return;

    // Find the occurrence
    for(i = 0; i < _gpu_num_window_mappings; i++)
    {
        if(_gpu_window_mappings[i].windowID == windowID)
        {
            int num_to_move;

            // Unset the target's window
            _gpu_window_mappings[i].target->context->windowID = 0;

            // Move the remaining entries to replace the removed one
            _gpu_num_window_mappings--;
            num_to_move = _gpu_num_window_mappings - i;
            if(num_to_move > 0)
                memmove(&_gpu_window_mappings[i], &_gpu_window_mappings[i+1], num_to_move * sizeof(WindowMapping));
            return;
        }
    }

}

void RemoveWindowMappingByTarget(Target* target)
{
    Uint32 windowID;
    int i;

    if(_gpu_window_mappings == NULL)
        gpu_init_window_mappings();

    if(target == NULL || target->context == NULL)
        return;

    windowID = target->context->windowID;
    if(windowID == 0)  // Invalid window ID
        return;

    // Unset the target's window
    target->context->windowID = 0;

    // Find the occurrences
    for(i = 0; i < _gpu_num_window_mappings; ++i)
    {
        if(_gpu_window_mappings[i].target == target)
        {
            // Move the remaining entries to replace the removed one
            int num_to_move;
            _gpu_num_window_mappings--;
            num_to_move = _gpu_num_window_mappings - i;
            if(num_to_move > 0)
                memmove(&_gpu_window_mappings[i], &_gpu_window_mappings[i+1], num_to_move * sizeof(WindowMapping));
            return;
        }
    }
}

Target* GetWindowTarget(Uint32 windowID)
{
    int i;

    if(_gpu_window_mappings == NULL)
        gpu_init_window_mappings();

    if(windowID == 0)  // Invalid window ID
        return NULL;

    // Find the occurrence
    for(i = 0; i < _gpu_num_window_mappings; ++i)
    {
        if(_gpu_window_mappings[i].windowID == windowID)
            return _gpu_window_mappings[i].target;
    }

    return NULL;
}


Target* Init(Uint16 w, Uint16 h, WindowFlagEnum SDL_flags)
{
    gpu_init_error_queue();

    if(!gpu_init_SDL())
        return NULL;

#if defined(XGPU_USE_GLES)
	DeviceID renderer_request = MakeDeviceID("OpenGLES 2", RENDERER_GLES_2, 2, 0);
#else
	DeviceID renderer_request = MakeDeviceID("OpenGL 2", RENDERER_OPENGL_2, 2, 1);
#endif
	RenderDevice* device = CreateRenderer(renderer_request);
	if (device) {
		_gpu_current_device = device;
		_gpu_current_renderer = new Renderer(device);
		_gpu_current_renderer->SetAsCurrent(device);
		Target* screen = _gpu_current_renderer->Init(renderer_request, w, h, SDL_flags);
		if (screen != NULL) return screen;
	}
    PushErrorCode("Init", ERROR_BACKEND_ERROR, "Renderer was not able to initialize properly");
    return NULL;
}


bool IsFeatureEnabled(FeatureEnum feature)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    return ((_gpu_current_device->enabled_features & feature) == feature);
}

Target* CreateTargetFromWindow(Uint32 windowID)
{
    if(_gpu_current_device == NULL)
        return NULL;

    return _gpu_current_renderer->CreateTargetFromWindow(windowID, NULL);
}

Target* CreateAliasTarget(Target* target)
{
    if(!CHECK_RENDERER)
        return NULL;
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        return NULL;

    return _gpu_current_renderer->CreateAliasTarget(target);
}

void MakeCurrent(Target* target, Uint32 windowID)
{
    if(_gpu_current_device == NULL)
        return;

    _gpu_current_renderer->MakeCurrent(target, windowID);
}

bool SetFullscreen(bool enable_fullscreen, bool use_desktop_resolution)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    return _gpu_current_renderer->SetFullscreen(enable_fullscreen, use_desktop_resolution);
}

bool GetFullscreen(void)
{
    Target* target = GetContextTarget();
    if(target == NULL)
        return false;
    return (SDL_GetWindowFlags(SDL_GetWindowFromID(target->context->windowID))
                   & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

bool SetWindowResolution(Uint16 w, Uint16 h)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL || w == 0 || h == 0)
        return false;

    return _gpu_current_renderer->SetWindowResolution(w, h);
}

void GetVirtualResolution(Target* target, Uint16* w, Uint16* h)
{
    // No checking here for NULL w or h...  Should we?
	if (target == NULL)
	{
		*w = 0;
		*h = 0;
	}
	else {
		*w = target->w;
		*h = target->h;
	}
}

void SetVirtualResolution(Target* target, Uint16 w, Uint16 h)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");
    if(w == 0 || h == 0)
        return;

    _gpu_current_renderer->SetVirtualResolution(target, w, h);
}

void UnsetVirtualResolution(Target* target)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->UnsetVirtualResolution(target);
}

void SetImageVirtualResolution(Image* image, Uint16 w, Uint16 h)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL || w == 0 || h == 0)
        return;

    if(image == NULL)
        return;

    image->w = w;
    image->h = h;
    image->using_virtual_resolution = 1;
}

void UnsetImageVirtualResolution(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    if(image == NULL)
        return;

    image->w = image->base_w;
    image->h = image->base_h;
    image->using_virtual_resolution = 0;
}

void Quit(void)
{
    gpu_free_error_queue();

    if(_gpu_current_device == NULL)
        return;

    _gpu_current_renderer->Quit(_gpu_current_device);
	FreeRenderer(_gpu_current_device);

    _gpu_current_device = NULL;
    _gpu_init_windowID = 0;

	delete _gpu_current_renderer;
	_gpu_current_renderer = NULL;

    // Free window mappings
    SDL_free(_gpu_window_mappings);
    _gpu_window_mappings = NULL;
    _gpu_window_mappings_size = 0;
    _gpu_num_window_mappings = 0;

    if(_gpu_initialized_SDL)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        _gpu_initialized_SDL = 0;

        if(_gpu_initialized_SDL_core)
        {
            SDL_Quit();
            _gpu_initialized_SDL_core = 0;
        }
    }
}

GPU_Rect MakeRect(float x, float y, float w, float h)
{
    GPU_Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;

    return r;
}

SDL_Color MakeColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Color c;
    c.r = r;
    c.g = g;
    c.b = b;
	c.a = a;

    return c;
}

DeviceID MakeDeviceID(const char* name, RendererEnum renderer, int major_version, int minor_version)
{
    DeviceID r;
    r.name = name;
    r.renderer = renderer;
    r.major_version = major_version;
    r.minor_version = minor_version;

    return r;
}

void SetViewport(Target* target, GPU_Rect viewport)
{
    if(target != NULL)
        target->viewport = viewport;
}

void UnsetViewport(Target* target)
{
    if(target != NULL)
        target->viewport = MakeRect(0, 0, target->w, target->h);
}

Camera GetDefaultCamera(void)
{
    Camera cam = {0.0f, 0.0f, -10.0f, 0.0f, 1.0f};
    return cam;
}

Camera GetCamera(Target* target)
{
    if(target == NULL)
        return GetDefaultCamera();
    return target->camera;
}

Camera SetCamera(Target* target, Camera* cam)
{
    if(_gpu_current_device == NULL)
        return GetDefaultCamera();
    MAKE_CURRENT_IF_NONE(target);
    if(_gpu_current_device->current_context_target == NULL)
        return GetDefaultCamera();
	// TODO: Remove from renderer and flush here
    return _gpu_current_renderer->SetCamera(target, cam);
}

void EnableCamera(Target* target, bool use_camera)
{
	if (target == NULL)
		return;
	// TODO: Flush here
	target->use_camera = use_camera;
}

bool IsCameraEnabled(Target* target)
{
	if (target == NULL)
		return false;
	return target->use_camera;
}

Image* CreateImage(Uint16 w, Uint16 h, FormatEnum format)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CreateImage(w, h, format);
}

Image* LoadImage(const char* filename)
{
    return LoadImage_RW(SDL_RWFromFile(filename, "rb"), 1);
}

#define RGB_PREMULTIPLY_ALPHA(vr, vg, vb, va) \
    (unsigned)(((unsigned)((unsigned char)(vr) * ((unsigned char)(va) + 1)) >> 8) | \
    ((unsigned)((unsigned char)(vg) * ((unsigned char)(va) + 1) >> 8) << 8) | \
    ((unsigned)((unsigned char)(vb) * ((unsigned char)(va) + 1) >> 8) << 16) | \
    ((unsigned)(unsigned char)(va) << 24))

Image* LoadImage_RW(SDL_RWops* rwops, bool free_rwops)
{
	Image* result;
	int width, height, channels;
	unsigned char* data;
	int data_bytes;
	unsigned char* c_data;
	int i;

	if (_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
		return NULL;
	if (rwops == NULL) {
		PushErrorCode("LoadImage_RW", ERROR_NULL_ARGUMENT, "rwops");
		return NULL;
	}

	// Get count of bytes
	SDL_RWseek(rwops, 0, SEEK_SET);
	data_bytes = SDL_RWseek(rwops, 0, SEEK_END);
	SDL_RWseek(rwops, 0, SEEK_SET);
	// Read in the rwops data
	c_data = (unsigned char*)SDL_malloc(data_bytes);
	SDL_RWread(rwops, c_data, 1, data_bytes);
	
	// Load image
	data = stbi_load_from_memory(c_data, data_bytes, &width, &height, &channels, 4);
	if (data == NULL) {
		PushErrorCode("LoadImage_RW", ERROR_DATA_ERROR, "Failed to load from rwops: %s", stbi_failure_reason());
		return NULL;
	}

	// Refetch image info (for channels)
	stbi_info_from_memory(c_data, data_bytes, &width, &height, &channels);

	// Clean up temp data
	SDL_free(c_data);
	if (free_rwops) SDL_RWclose(rwops);

	result = CreateImage(width, height, FORMAT_RGBA);

	// ! Premultiplied alpha 
	if (channels == 4) {
		uint32_t* data32 = (uint32_t*)data;
		for (i = 0; i < width * height; ++i) {
			unsigned char* p = data + i * 4;
			data32[i] = RGB_PREMULTIPLY_ALPHA(p[0], p[1], p[2], p[3]);
		}
	}

	UpdateImageBytes(result, NULL, data, 4 * width);
	stbi_image_free(data);
    return result;
}

Image* CreateAliasImage(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CreateAliasImage(image);
}

bool SaveImage(Image* image, const char* filename, FileFormatEnum format)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    return _gpu_current_renderer->SaveImage(image, filename, format);
}

bool SaveImage_RW(Image* image, SDL_RWops* rwops, bool free_rwops, FileFormatEnum format)
{
    bool result;
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    SDL_Surface* surface = CopySurfaceFromImage(image);
    result = SaveSurface_RW(surface, rwops, free_rwops, format);
    SDL_FreeSurface(surface);
    return result;
}

Image* CopyImage(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CopyImage(image);
}

void UpdateImage(Image* image, const GPU_Rect* image_rect, SDL_Surface* surface, const GPU_Rect* surface_rect)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->UpdateImage(image, image_rect, surface, surface_rect);
}

void UpdateImageBytes(Image* image, const GPU_Rect* image_rect, const unsigned char* bytes, int bytes_per_row)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->UpdateImageBytes(image, image_rect, bytes, bytes_per_row);
}

bool ReplaceImage(Image* image, SDL_Surface* surface, const GPU_Rect* surface_rect)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    return _gpu_current_renderer->ReplaceImage(image, surface, surface_rect);
}


// From http://stackoverflow.com/questions/5309471/getting-file-extension-in-c
static const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return "";
    return dot + 1;
}

bool SaveSurface(SDL_Surface* surface, const char* filename, FileFormatEnum format)
{
    bool result;
    unsigned char* data;

    if(surface == NULL || filename == NULL ||
            surface->w < 1 || surface->h < 1)
    {
        return false;
    }

    data = (unsigned char*)surface->pixels;

    if(format == FILE_AUTO)
    {
        const char* extension = get_filename_ext(filename);
        if (strcasecmp(extension, "png") == 0)
            format = FILE_PNG;
        else if (strcasecmp(extension, "bmp") == 0)
            format = FILE_BMP;
        else if (strcasecmp(extension, "tga") == 0)
            format = FILE_TGA;
        else
        {
            PushErrorCode(__func__, ERROR_DATA_ERROR, "Could not detect output file format from file name");
            return false;
        }
    }

    switch(format)
    {
    case FILE_PNG:
        result = (stbi_write_png(filename, surface->w, surface->h, surface->format->BytesPerPixel, (const unsigned char *const)data, 0) > 0);
        break;
    case FILE_BMP:
        result = (stbi_write_bmp(filename, surface->w, surface->h, surface->format->BytesPerPixel, (void*)data) > 0);
        break;
    case FILE_TGA:
        result = (stbi_write_tga(filename, surface->w, surface->h, surface->format->BytesPerPixel, (void*)data) > 0);
        break;
    default:
        PushErrorCode(__func__, ERROR_DATA_ERROR, "Unsupported output file format");
        result = false;
        break;
    }

    return result;
}

static void write_func(void *context, void *data, int size)
{
    SDL_RWwrite((SDL_RWops*)context, data, 1, size);
}

bool SaveSurface_RW(SDL_Surface* surface, SDL_RWops* rwops, bool free_rwops, FileFormatEnum format)
{
    bool result;
    unsigned char* data;

    if(surface == NULL || rwops == NULL ||
            surface->w < 1 || surface->h < 1)
    {
        return false;
    }

    data = (unsigned char*)surface->pixels;

    if(format == FILE_AUTO)
    {
        PushErrorCode(__func__, ERROR_DATA_ERROR, "Invalid output file format (FILE_AUTO)");
        return false;
    }

    // FIXME: The limitations here are not communicated clearly.  BMP and TGA won't support arbitrary row length/pitch.
    switch(format)
    {
    case FILE_PNG:
        result = (stbi_write_png_to_func(write_func, rwops, surface->w, surface->h, surface->format->BytesPerPixel, (const unsigned char *const)data, surface->pitch) > 0);
        break;
    case FILE_BMP:
        result = (stbi_write_bmp_to_func(write_func, rwops, surface->w, surface->h, surface->format->BytesPerPixel, (const unsigned char *const)data) > 0);
        break;
    case FILE_TGA:
        result = (stbi_write_tga_to_func(write_func, rwops, surface->w, surface->h, surface->format->BytesPerPixel, (const unsigned char *const)data) > 0);
        break;
    default:
        PushErrorCode(__func__, ERROR_DATA_ERROR, "Unsupported output file format");
        result = false;
        break;
    }

    if(result && free_rwops)
        SDL_RWclose(rwops);
    return result;
}

Image* CopyImageFromSurface(SDL_Surface* surface)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CopyImageFromSurface(surface);
}

Image* CopyImageFromTarget(Target* target)
{
    if(_gpu_current_device == NULL)
        return NULL;
    MAKE_CURRENT_IF_NONE(target);
    if(_gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CopyImageFromTarget(target);
}

SDL_Surface* CopySurfaceFromTarget(Target* target)
{
    if(_gpu_current_device == NULL)
        return NULL;
    MAKE_CURRENT_IF_NONE(target);
    if(_gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CopySurfaceFromTarget(target);
}

SDL_Surface* CopySurfaceFromImage(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->CopySurfaceFromImage(image);
}

void FreeImage(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->FreeImage(image);
}


Target* GetContextTarget(void)
{
    if(_gpu_current_device == NULL)
        return NULL;

    return _gpu_current_device->current_context_target;
}


Target* LoadTarget(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->LoadTarget(image);
}


void FreeTarget(Target* target)
{
    if(_gpu_current_device == NULL)
        return;

    _gpu_current_renderer->FreeTarget(target);
}

void Blit(Image* image, GPU_Rect* src_rect, Target* target, float x, float y)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(image == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "image");
    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    _gpu_current_renderer->Blit(image, src_rect, target, x, y);
}

void BlitRotate(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(image == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "image");
    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    _gpu_current_renderer->BlitRotate(image, src_rect, target, x, y, degrees);
}

void BlitScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(image == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "image");
    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    _gpu_current_renderer->BlitScale(image, src_rect, target, x, y, scaleX, scaleY);
}

void BlitTransform(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(image == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "image");
    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    _gpu_current_renderer->BlitTransform(image, src_rect, target, x, y, degrees, scaleX, scaleY);
}

void BlitTransformX(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float pivot_x, float pivot_y, float degrees, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(image == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "image");
    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    _gpu_current_renderer->BlitTransformX(image, src_rect, target, x, y, pivot_x, pivot_y, degrees, scaleX, scaleY);
}

void BlitTransformA(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, AffineTransform* transform)
{	
	_gpu_current_renderer->BlitTransformA(image, src_rect, target, x, y, transform, 1.0, 1.0);
}

void BlitTransformAScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, AffineTransform* transform, float scaleX, float scaleY)
{
	_gpu_current_renderer->BlitTransformA(image, src_rect, target, x, y, transform, scaleX, scaleY);
}

void BlitTransformAColor(Image * image, Target * target, float x, float y, AffineTransform * transform, GPU_Color colors[4])
{
	_gpu_current_renderer->BlitTransformAColor(image, target, x, y, transform, colors);
}

void BlitRect(Image* image, GPU_Rect* src_rect, Target* target, GPU_Rect* dest_rect)
{
    float w = 0.0f;
    float h = 0.0f;
    
    if(image == NULL)
        return;
    
    if(src_rect == NULL)
    {
        w = image->w;
        h = image->h;
    }
    else
    {
        w = src_rect->w;
        h = src_rect->h;
    }
    
    BlitRectX(image, src_rect, target, dest_rect, 0.0f, w*0.5f, h*0.5f, FLIP_NONE);
}

void BlitRectX(Image* image, GPU_Rect* src_rect, Target* target, GPU_Rect* dest_rect, float degrees, float pivot_x, float pivot_y, FlipEnum flip_direction)
{
    float w, h;
    float dx, dy;
    float dw, dh;
    float scale_x, scale_y;
    
    if(image == NULL || target == NULL)
        return;
    
    if(src_rect == NULL)
    {
        w = image->w;
        h = image->h;
    }
    else
    {
        w = src_rect->w;
        h = src_rect->h;
    }
    
    if(dest_rect == NULL)
    {
        dx = 0.0f;
        dy = 0.0f;
        dw = target->w;
        dh = target->h;
    }
    else
    {
        dx = dest_rect->x;
        dy = dest_rect->y;
        dw = dest_rect->w;
        dh = dest_rect->h;
    }
    
    scale_x = dw / w;
    scale_y = dh / h;
    
	if (flip_direction & FLIP_HORIZONTAL) {
		scale_x = -scale_x;
		dx += dw;
		pivot_x = w - pivot_x;
	}
	if (flip_direction & FLIP_VERTICAL) {
		scale_y = -scale_y;
		dy += dh;
		pivot_y = h - pivot_y;
	}
    
    BlitTransformX(image, src_rect, target, dx + pivot_x * scale_x, dy + pivot_y * scale_y, pivot_x, pivot_y, degrees, scale_x, scale_y);
}

void TriangleBatch(Image* image, Target* target, unsigned short num_vertices, float* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags)
{
    TriangleBatchX(image, target, num_vertices, (void*)values, num_indices, indices, flags);
}

void TriangleBatchX(Image* image, Target* target, unsigned short num_vertices, void* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    if(target == NULL)
        RETURN_ERROR(ERROR_NULL_ARGUMENT, "target");

    if(num_vertices == 0)
        return;

    _gpu_current_renderer->TriangleBatchX(image, target, num_vertices, values, num_indices, indices, flags);
}

void GenerateMipmaps(Image* image)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->GenerateMipmaps(image);
}

GPU_Rect SetClipRect(Target* target, GPU_Rect rect)
{
    if(target == NULL || _gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
    {		
        return { 0, 0, 0, 0 };
    }

    return _gpu_current_renderer->SetClip(target, (Sint16)rect.x, (Sint16)rect.y, (Uint16)rect.w, (Uint16)rect.h);
}

GPU_Rect SetClip(Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h)
{
    if(target == NULL || _gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
    {
		return{ 0, 0, 0, 0 };
    }

    return _gpu_current_renderer->SetClip(target, x, y, w, h);
}

void UnsetClip(Target* target)
{
    if(target == NULL || _gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->UnsetClip(target);
}

/* Adapted from SDL_IntersectRect() */
bool IntersectRect(GPU_Rect A, GPU_Rect B, GPU_Rect* result)
{
    bool has_horiz_intersection = false;
    float Amin, Amax, Bmin, Bmax;
    GPU_Rect intersection;

    // Special case for empty rects
    if (A.w <= 0.0f || A.h <= 0.0f || B.w <= 0.0f || B.h <= 0.0f)
        return false;

    // Horizontal intersection
    Amin = A.x;
    Amax = Amin + A.w;
    Bmin = B.x;
    Bmax = Bmin + B.w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    
    intersection.x = Amin;
    intersection.w = Amax - Amin;
    
    has_horiz_intersection = (Amax > Amin);

    // Vertical intersection
    Amin = A.y;
    Amax = Amin + A.h;
    Bmin = B.y;
    Bmax = Bmin + B.h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    
    intersection.y = Amin;
    intersection.h = Amax - Amin;
    
    if(has_horiz_intersection && Amax > Amin)
    {
        if(result != NULL)
            *result = intersection;
        return true;
    }
    else
        return false;
}


bool IntersectClipRect(Target* target, GPU_Rect B, GPU_Rect* result)
{
    if(target == NULL)
        return false;
    
    if(!target->use_clip_rect)
    {
        GPU_Rect A = { 0, 0, target->w, target->h };
        return IntersectRect(A, B, result);
    }
    
    return IntersectRect(target->clip_rect, B, result);
}


void SetColor(Image* image, SDL_Color color)
{
    if(image == NULL)
        return;

    image->color = color;
}

void SetRGB(Image* image, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Color c;
    c.r = r;
    c.g = g;
    c.b = b;
	c.a = 255;
    
    if(image == NULL)
        return;

    image->color = c;
}

void SetRGBA(Image* image, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Color c;
    c.r = r;
    c.g = g;
    c.b = b;
	c.a = a;
    
    if(image == NULL)
        return;

    image->color = c;
}

void UnsetColor(Image* image)
{
    SDL_Color c = {255, 255, 255, 255};
    if(image == NULL)
        return;

    image->color = c;
}

void SetTargetColor(Target* target, SDL_Color color)
{
    if(target == NULL)
        return;

    target->use_color = 1;
    target->color = color;
}

void SetTargetRGB(Target* target, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Color c;
    c.r = r;
    c.g = g;
    c.b = b;
	c.a = 255;
    
    if(target == NULL)
        return;

    target->use_color = !(r == 255 && g == 255 && b == 255);
    target->color = c;
}

void SetTargetRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Color c;
    c.r = r;
    c.g = g;
    c.b = b;
	c.a = a;
    
    if(target == NULL)
        return;

    target->use_color = !(r == 255 && g == 255 && b == 255 && a == 255);
    target->color = c;
}

void UnsetTargetColor(Target* target)
{
    SDL_Color c = {255, 255, 255, 255};
    if(target == NULL)
        return;

    target->use_color = false;
    target->color = c;
}

bool GetBlending(Image* image)
{
    if(image == NULL)
        return false;

    return image->use_blending;
}


void SetBlending(Image* image, bool enable)
{
    if(image == NULL)
        return;

    image->use_blending = enable;
}

void SetShapeBlending(bool enable)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_device->current_context_target->context->shapes_use_blending = enable;
}

BlendMode GetBlendModeFromPreset(BlendPresetEnum preset)
{
    switch(preset)
    {
    case BLEND_NORMAL:
    {
        BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_PREMULTIPLIED_ALPHA:
    {
        BlendMode b = {FUNC_ONE, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ONE, FUNC_ONE_MINUS_SRC_ALPHA, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_MULTIPLY:
    {
        BlendMode b = {FUNC_DST_COLOR, FUNC_ZERO, FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_ADD:
    {
        BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE, FUNC_SRC_ALPHA, FUNC_ONE, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_SUBTRACT:
        // FIXME: Use src alpha for source components?
    {
        BlendMode b = {FUNC_ONE, FUNC_ONE, FUNC_ONE, FUNC_ONE, EQ_SUBTRACT, EQ_SUBTRACT};
        return b;
    }
    break;
    case BLEND_MOD_ALPHA:
        // Don't disturb the colors, but multiply the dest alpha by the src alpha
    {
        BlendMode b = {FUNC_ZERO, FUNC_ONE, FUNC_ZERO, FUNC_SRC_ALPHA, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_SET_ALPHA:
        // Don't disturb the colors, but set the alpha to the src alpha
    {
        BlendMode b = {FUNC_ZERO, FUNC_ONE, FUNC_ONE, FUNC_ZERO, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_SET:
    {
        BlendMode b = {FUNC_ONE, FUNC_ZERO, FUNC_ONE, FUNC_ZERO, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_NORMAL_KEEP_ALPHA:
    {
        BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ZERO, FUNC_ONE, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_NORMAL_ADD_ALPHA:
    {
        BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ONE, FUNC_ONE, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    case BLEND_NORMAL_FACTOR_ALPHA:
    {
        BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ONE_MINUS_DST_ALPHA, FUNC_ONE, EQ_ADD, EQ_ADD};
        return b;
    }
    break;
    default:
        PushErrorCode(__func__, ERROR_USER_ERROR, "Blend preset not supported: %d", preset);
        {
            BlendMode b = {FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, EQ_ADD, EQ_ADD};
            return b;
        }
        break;
    }
}

void SetBlendFunction(Image* image, BlendFuncEnum source_color, BlendFuncEnum dest_color, BlendFuncEnum source_alpha, BlendFuncEnum dest_alpha)
{
    if(image == NULL)
        return;

    image->blend_mode.source_color = source_color;
    image->blend_mode.dest_color = dest_color;
    image->blend_mode.source_alpha = source_alpha;
    image->blend_mode.dest_alpha = dest_alpha;
}

void SetBlendEquation(Image* image, BlendEqEnum color_equation, BlendEqEnum alpha_equation)
{
    if(image == NULL)
        return;

    image->blend_mode.color_equation = color_equation;
    image->blend_mode.alpha_equation = alpha_equation;
}

void SetBlendMode(Image* image, BlendPresetEnum preset)
{
    BlendMode b;
    if(image == NULL)
        return;

    b = GetBlendModeFromPreset(preset);
    SetBlendFunction(image, b.source_color, b.dest_color, b.source_alpha, b.dest_alpha);
    SetBlendEquation(image, b.color_equation, b.alpha_equation);
}

void SetShapeBlendFunction(BlendFuncEnum source_color, BlendFuncEnum dest_color, BlendFuncEnum source_alpha, BlendFuncEnum dest_alpha)
{
    Context* context;
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    context = _gpu_current_device->current_context_target->context;

    context->shapes_blend_mode.source_color = source_color;
    context->shapes_blend_mode.dest_color = dest_color;
    context->shapes_blend_mode.source_alpha = source_alpha;
    context->shapes_blend_mode.dest_alpha = dest_alpha;
}

void SetShapeBlendEquation(BlendEqEnum color_equation, BlendEqEnum alpha_equation)
{
    Context* context;
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    context = _gpu_current_device->current_context_target->context;

    context->shapes_blend_mode.color_equation = color_equation;
    context->shapes_blend_mode.alpha_equation = alpha_equation;
}

void SetShapeBlendMode(BlendPresetEnum preset)
{
    BlendMode b;
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    b = GetBlendModeFromPreset(preset);
    SetShapeBlendFunction(b.source_color, b.dest_color, b.source_alpha, b.dest_alpha);
    SetShapeBlendEquation(b.color_equation, b.alpha_equation);
}

void SetImageFilter(Image* image, FilterEnum filter)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;
    if(image == NULL)
        return;

    _gpu_current_renderer->SetImageFilter(image, filter);
}


void SetDefaultAnchor(float anchor_x, float anchor_y)
{
    if(_gpu_current_device == NULL)
        return;
    
    _gpu_current_device->default_image_anchor_x = anchor_x;
    _gpu_current_device->default_image_anchor_y = anchor_y;
}

void GetDefaultAnchor(float* anchor_x, float* anchor_y)
{
    if(_gpu_current_device == NULL)
        return;
    
    if(anchor_x != NULL)
        *anchor_x = _gpu_current_device->default_image_anchor_x;
    
    if(anchor_y != NULL)
        *anchor_y = _gpu_current_device->default_image_anchor_y;
}

void SetAnchor(Image* image, float anchor_x, float anchor_y)
{
    if(image == NULL)
        return;

    image->anchor_x = anchor_x;
    image->anchor_y = anchor_y;
}

void GetAnchor(Image* image, float* anchor_x, float* anchor_y)
{
    if(image == NULL)
        return;
    
    if(anchor_x != NULL)
        *anchor_x = image->anchor_x;
    
    if(anchor_y != NULL)
        *anchor_y = image->anchor_y;
}

SnapEnum GetSnapMode(Image* image)
{
    if (image == NULL) return SNAP_NONE;
    return image->snap_mode;
}

void SetSnapMode(Image* image, SnapEnum mode)
{
    if(image == NULL)
        return;
    image->snap_mode = mode;
}

void SetWrapMode(Image* image, WrapEnum wrap_mode_x, WrapEnum wrap_mode_y)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;
    if(image == NULL)
        return;

    _gpu_current_renderer->SetWrapMode(image, wrap_mode_x, wrap_mode_y);
}


SDL_Color GetPixel(Target* target, Sint16 x, Sint16 y)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
    {
        SDL_Color c = {0,0,0,0};
        return c;
    }

    return _gpu_current_renderer->GetPixel(target, x, y);
}


bool GetImageData(Target* target, unsigned char* data, Sint16 x, Sint16 y, Sint16 w, Sint16 h)
{
	if (_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL) {
		return false;
	}
	return _gpu_current_renderer->GetImageData(target, data, x, y, w, h);
}


void Clear(Target* target)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->ClearRGBA(target, 0, 0, 0, 0);
}

void ClearColor(Target* target, SDL_Color color)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->ClearRGBA(target, color.r, color.g, color.b, color.a);
}

void ClearRGB(Target* target, Uint8 r, Uint8 g, Uint8 b)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->ClearRGBA(target, r, g, b, 255);
}

void ClearRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->ClearRGBA(target, r, g, b, a);
}

void FlushBlitBuffer(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->FlushBlitBuffer();
}

void Flip(Target* target)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL renderer");
    
    if(target != NULL && target->context == NULL)
    {
        _gpu_current_renderer->FlushBlitBuffer();
        return;
    }
    
    MAKE_CURRENT_IF_NONE(target);
    if(!CHECK_CONTEXT)
        RETURN_ERROR(ERROR_USER_ERROR, "NULL context");

    _gpu_current_renderer->Flip(target);
}


// Shader API


Uint32 CompileShader_RW(ShaderEnum shader_type, SDL_RWops* shader_source, bool free_rwops)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
    {
        if(free_rwops)
            SDL_RWclose(shader_source);
        return false;
    }

    return _gpu_current_renderer->CompileShader_RW(shader_type, shader_source, free_rwops);
}

Uint32 LoadShader(ShaderEnum shader_type, const char* filename)
{
    SDL_RWops* rwops;

    if(filename == NULL)
    {
        PushErrorCode(__func__, ERROR_NULL_ARGUMENT, "filename");
        return 0;
    }
    
    rwops = SDL_RWFromFile(filename, "r");
    if(rwops == NULL)
    {
        PushErrorCode(__func__, ERROR_FILE_NOT_FOUND, "%s", filename);
        return 0;
    }
    
    return CompileShader_RW(shader_type, rwops, 1);
}

Uint32 CompileShader(ShaderEnum shader_type, const char* shader_source)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;

    return _gpu_current_renderer->CompileShader(shader_type, shader_source);
}

bool LinkShaderProgram(Uint32 program_object)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    return _gpu_current_renderer->LinkShaderProgram(program_object);
}

Uint32 CreateShaderProgram(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;

    return _gpu_current_renderer->CreateShaderProgram();
}

Uint32 LinkShaders(Uint32 shader_object1, Uint32 shader_object2)
{
    Uint32 shaders[2];
    shaders[0] = shader_object1;
    shaders[1] = shader_object2;
    return LinkManyShaders(shaders, 2);
}

Uint32 LinkManyShaders(Uint32 *shader_objects, int count)
{
    Uint32 p;
    int i;

    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;

    if((_gpu_current_device->enabled_features & FEATURE_BASIC_SHADERS) != FEATURE_BASIC_SHADERS)
        return 0;

    p = _gpu_current_renderer->CreateShaderProgram();

    for (i = 0; i < count; i++)
        _gpu_current_renderer->AttachShader(p, shader_objects[i]);

    if(_gpu_current_renderer->LinkShaderProgram(p))
        return p;

    _gpu_current_renderer->FreeShaderProgram(p);
    return 0;
}

void FreeShader(Uint32 shader_object)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->FreeShader(shader_object);
}

void FreeShaderProgram(Uint32 program_object)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->FreeShaderProgram(program_object);
}

void AttachShader(Uint32 program_object, Uint32 shader_object)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->AttachShader(program_object, shader_object);
}

void DetachShader(Uint32 program_object, Uint32 shader_object)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->DetachShader(program_object, shader_object);
}

bool IsDefaultShaderProgram(Uint32 program_object)
{
    Context* context;

    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return false;

    context = _gpu_current_device->current_context_target->context;
    return (program_object == context->default_textured_shader_program || program_object == context->default_untextured_shader_program);
}

void ActivateShaderProgram(Uint32 program_object, ShaderBlock* block)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->ActivateShaderProgram(program_object, block);
}

void DeactivateShaderProgram(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->DeactivateShaderProgram();
}

const char* GetShaderMessage(void)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return NULL;

    return _gpu_current_renderer->GetShaderMessage();
}

int GetAttributeLocation(Uint32 program_object, const char* attrib_name)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;

    return _gpu_current_renderer->GetAttributeLocation(program_object, attrib_name);
}

AttributeFormat MakeAttributeFormat(int num_elems_per_vertex, TypeEnum type, bool normalize, int stride_bytes, int offset_bytes)
{
    AttributeFormat f;
    f.is_per_sprite = false;
    f.num_elems_per_value = num_elems_per_vertex;
    f.type = type;
    f.normalize = normalize;
    f.stride_bytes = stride_bytes;
    f.offset_bytes = offset_bytes;
    return f;
}

Attribute MakeAttribute(int location, void* values, AttributeFormat format)
{
    Attribute a;
    a.location = location;
    a.values = values;
    a.format = format;
    return a;
}

int GetUniformLocation(Uint32 program_object, const char* uniform_name)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return 0;

    return _gpu_current_renderer->GetUniformLocation(program_object, uniform_name);
}

ShaderBlock LoadShaderBlock(Uint32 program_object, const char* position_name, const char* texcoord_name, const char* color_name, const char* modelViewMatrix_name)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
    {
        ShaderBlock b;
        b.position_loc = -1;
        b.texcoord_loc = -1;
        b.color_loc = -1;
        b.modelViewProjection_loc = -1;
        return b;
    }

    return _gpu_current_renderer->LoadShaderBlock(program_object, position_name, texcoord_name, color_name, modelViewMatrix_name);
}

void SetShaderBlock(ShaderBlock block)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_device->current_context_target->context->current_shader_block = block;
}

void SetShaderImage(Image* image, int location, int image_unit)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetShaderImage(image, location, image_unit);
}

void GetUniformiv(Uint32 program_object, int location, int* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->GetUniformiv(program_object, location, values);
}

void SetUniformi(int location, int value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformi(location, value);
}

void SetUniformiv(int location, int num_elements_per_value, int num_values, int* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformiv(location, num_elements_per_value, num_values, values);
}


void GetUniformuiv(Uint32 program_object, int location, unsigned int* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->GetUniformuiv(program_object, location, values);
}

void SetUniformui(int location, unsigned int value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformui(location, value);
}

void SetUniformuiv(int location, int num_elements_per_value, int num_values, unsigned int* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformuiv(location, num_elements_per_value, num_values, values);
}


void GetUniformfv(Uint32 program_object, int location, float* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->GetUniformfv(program_object, location, values);
}

void SetUniformf(int location, float value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformf(location, value);
}

void SetUniformfv(int location, int num_elements_per_value, int num_values, float* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformfv(location, num_elements_per_value, num_values, values);
}

// Same as GetUniformfv()
void GetUniformMatrixfv(Uint32 program_object, int location, float* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->GetUniformfv(program_object, location, values);
}

void SetUniformMatrixfv(int location, int num_matrices, int num_rows, int num_columns, bool transpose, float* values)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetUniformMatrixfv(location, num_matrices, num_rows, num_columns, transpose, values);
}


void SetAttributef(int location, float value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributef(location, value);
}

void SetAttributei(int location, int value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributei(location, value);
}

void SetAttributeui(int location, unsigned int value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributeui(location, value);
}

void SetAttributefv(int location, int num_elements, float* value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributefv(location, num_elements, value);
}

void SetAttributeiv(int location, int num_elements, int* value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributeiv(location, num_elements, value);
}

void SetAttributeuiv(int location, int num_elements, unsigned int* value)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributeuiv(location, num_elements, value);
}

void SetAttributeSource(int num_values, Attribute source)
{
    if(_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL)
        return;

    _gpu_current_renderer->SetAttributeSource(num_values, source);
}

bool IsExtensionSupported(const char* extension_str)
{
	if (_gpu_current_device == NULL || _gpu_current_device->current_context_target == NULL) return false;
	return _gpu_current_renderer->IsExtensionSupported(extension_str);
}

NS_GPU_END

#ifdef _MSC_VER
    #pragma warning(pop) 
#endif

