#ifndef _XGPU_H
#define _XGPU_H

#include "xgpu_define.h"
#include "xgpu_debug.h"
#include "xgpu_matrix.h"
#include "xgpu_affine_transform.h"

NS_GPU_BEGIN

class RenderDevice;
class Renderer;
struct Target;

typedef Uint32 RendererEnum;
static const RendererEnum RENDERER_UNKNOWN = 0;  // invalid value
static const RendererEnum RENDERER_OPENGL_1_BASE = 1;
static const RendererEnum RENDERER_OPENGL_1 = 2;
static const RendererEnum RENDERER_OPENGL_2 = 3;
static const RendererEnum RENDERER_OPENGL_3 = 4;
static const RendererEnum RENDERER_OPENGL_4 = 5;
static const RendererEnum RENDERER_GLES_1 = 11;
static const RendererEnum RENDERER_GLES_2 = 12;
static const RendererEnum RENDERER_GLES_3 = 13;
static const RendererEnum RENDERER_D3D9 = 21;
static const RendererEnum RENDERER_D3D10 = 22;
static const RendererEnum RENDERER_D3D11 = 23;

struct DeviceID {
    const char* name;
    RendererEnum renderer;
    int major_version;
    int minor_version;
};

typedef enum {
    FUNC_ZERO = 0,
    FUNC_ONE = 1,
    FUNC_SRC_COLOR = 0x0300,
    FUNC_DST_COLOR = 0x0306,
    FUNC_ONE_MINUS_SRC = 0x0301,
    FUNC_ONE_MINUS_DST = 0x0307,
    FUNC_SRC_ALPHA = 0x0302,
    FUNC_DST_ALPHA = 0x0304,
    FUNC_ONE_MINUS_SRC_ALPHA = 0x0303,
    FUNC_ONE_MINUS_DST_ALPHA = 0x0305
} BlendFuncEnum;

typedef enum {
    EQ_ADD = 0x8006,
    EQ_SUBTRACT = 0x800A,
    EQ_REVERSE_SUBTRACT = 0x800B
} BlendEqEnum;

struct BlendMode {
    BlendFuncEnum source_color;
    BlendFuncEnum dest_color;
    BlendFuncEnum source_alpha;
    BlendFuncEnum dest_alpha;
    
    BlendEqEnum color_equation;
    BlendEqEnum alpha_equation;
};

typedef enum {
    BLEND_NORMAL = 0,
    BLEND_PREMULTIPLIED_ALPHA = 1,
    BLEND_MULTIPLY = 2,
    BLEND_ADD = 3,
    BLEND_SUBTRACT = 4,
    BLEND_MOD_ALPHA = 5,
    BLEND_SET_ALPHA = 6,
    BLEND_SET = 7,
    BLEND_NORMAL_KEEP_ALPHA = 8,
    BLEND_NORMAL_ADD_ALPHA = 9,
    BLEND_NORMAL_FACTOR_ALPHA = 10
} BlendPresetEnum;

typedef enum {
    FILTER_NEAREST = 0,
    FILTER_LINEAR = 1,
    FILTER_LINEAR_MIPMAP = 2
} FilterEnum;

typedef enum {
    SNAP_NONE = 0,
    SNAP_POSITION = 1,
    SNAP_DIMENSIONS = 2,
    SNAP_POSITION_AND_DIMENSIONS = 3
} SnapEnum;

typedef enum {
    WRAP_NONE = 0,
    WRAP_REPEAT = 1,
    WRAP_MIRRORED = 2
} WrapEnum;

typedef enum {
    FORMAT_LUMINANCE = 1,
    FORMAT_LUMINANCE_ALPHA = 2,
    FORMAT_RGB = 3,
    FORMAT_RGBA = 4,
    FORMAT_ALPHA = 5,
    FORMAT_RG = 6,
    FORMAT_YCbCr422 = 7,
    FORMAT_YCbCr420P = 8
} FormatEnum;

typedef enum {
    FILE_AUTO = 0,
    FILE_PNG,
    FILE_BMP,
    FILE_TGA
} FileFormatEnum;

struct Image {
	RenderDevice* renderer;
	Target* context_target;
	Target* target;
	Uint16 w, h;
	bool using_virtual_resolution;
	FormatEnum format;
	int num_layers;
	int bytes_per_pixel;
	Uint16 base_w, base_h;  // Original image dimensions
	Uint16 texture_w, texture_h;  // Underlying texture dimensions
	bool has_mipmaps;
	
	float anchor_x; // Normalized coords for the point at which the image is blitted.  Default is (0.5, 0.5), that is, the image is drawn centered.
	float anchor_y; // These are interpreted according to SetCoordinateMode() and range from (0.0 - 1.0) normally.	
	bool anchor_fixed;
	
	SDL_Color color;
	bool use_blending;
	BlendMode blend_mode;
	FilterEnum filter_mode;
	SnapEnum snap_mode;
	WrapEnum wrap_mode_x;
	WrapEnum wrap_mode_y;
	
	void* data;
	int refcount;
	bool is_alias;
};

struct Camera {
	float x, y, z;
	float angle;
	float zoom;
};

struct ShaderBlock {
    // Attributes
    int position_loc;
    int texcoord_loc;
    int color_loc;
    // Uniforms
    int modelViewProjection_loc;
};

#define MODELVIEW 0
#define PROJECTION 1

#ifndef MATRIX_STACK_MAX
#define MATRIX_STACK_MAX 5
#endif

typedef struct MatrixStack
{
    unsigned int size;
    float matrix[MATRIX_STACK_MAX][16];
} MatrixStack;

struct Context {
    /* SDL_GLContext */
    void* context;
    bool failed;
    
    /* SDL window ID */
	Uint32 windowID;
	
	/* Actual window dimensions */
	int window_w;
	int window_h;
	
	/* Drawable region dimensions */
	int drawable_w;
	int drawable_h;
	
	/* Window dimensions for restoring windowed mode after SetFullscreen(1,1). */
	int stored_window_w;
	int stored_window_h;
	
	/* Internal state */
	Uint32 current_shader_program;
	Uint32 default_textured_shader_program;
	Uint32 default_untextured_shader_program;
	
    ShaderBlock current_shader_block;
    ShaderBlock default_textured_shader_block;
    ShaderBlock default_untextured_shader_block;
	
	bool shapes_use_blending;
	BlendMode shapes_blend_mode;
	float line_thickness;
	bool use_texturing;
	
    int matrix_mode;
    MatrixStack projection_matrix;
    MatrixStack modelview_matrix;
	
	void* data;
};

struct Target {
	RenderDevice* renderer;
	Target* context_target;
	Image* image;
	void* data;
	Uint16 w, h;
	bool using_virtual_resolution;
	Uint16 base_w, base_h;  // The true dimensions of the underlying image or window
	bool use_clip_rect;
	GPU_Rect clip_rect;
	bool use_color;
	SDL_Color color;
	
	GPU_Rect viewport;
	
	/* Perspective and object viewing transforms. */
	Camera camera;
	bool use_camera;
	
	/* Renderer context data.  NULL if the target does not represent a window or rendering context. */
	Context* context;
	int refcount;
	bool is_alias;
};

typedef Uint32 FeatureEnum;
static const FeatureEnum FEATURE_NON_POWER_OF_TWO = 0x1;
static const FeatureEnum FEATURE_RENDER_TARGETS = 0x2;
static const FeatureEnum FEATURE_BLEND_EQUATIONS = 0x4;
static const FeatureEnum FEATURE_BLEND_FUNC_SEPARATE = 0x8;
static const FeatureEnum FEATURE_BLEND_EQUATIONS_SEPARATE = 0x10;
static const FeatureEnum FEATURE_GL_BGR = 0x20;
static const FeatureEnum FEATURE_GL_BGRA = 0x40;
static const FeatureEnum FEATURE_GL_ABGR = 0x80;
static const FeatureEnum FEATURE_VERTEX_SHADER = 0x100;
static const FeatureEnum FEATURE_FRAGMENT_SHADER = 0x200;
static const FeatureEnum FEATURE_PIXEL_SHADER = 0x200;
static const FeatureEnum FEATURE_GEOMETRY_SHADER = 0x400;
static const FeatureEnum FEATURE_WRAP_REPEAT_MIRRORED = 0x800;

/* Combined feature flags */
#define FEATURE_ALL_BASE FEATURE_RENDER_TARGETS
#define FEATURE_ALL_BLEND_PRESETS (FEATURE_BLEND_EQUATIONS | FEATURE_BLEND_FUNC_SEPARATE)
#define FEATURE_ALL_GL_FORMATS (FEATURE_GL_BGR | FEATURE_GL_BGRA | FEATURE_GL_ABGR)
#define FEATURE_BASIC_SHADERS (FEATURE_FRAGMENT_SHADER | FEATURE_VERTEX_SHADER)
#define FEATURE_ALL_SHADERS (FEATURE_FRAGMENT_SHADER | FEATURE_VERTEX_SHADER | FEATURE_GEOMETRY_SHADER)

typedef Uint32 WindowFlagEnum;

typedef Uint32 InitFlagEnum;
static const InitFlagEnum INIT_ENABLE_VSYNC = 0x1;
static const InitFlagEnum INIT_DISABLE_VSYNC = 0x2;
static const InitFlagEnum INIT_DISABLE_DOUBLE_BUFFER = 0x4;
static const InitFlagEnum INIT_DISABLE_AUTO_VIRTUAL_RESOLUTION = 0x8;
static const InitFlagEnum INIT_REQUEST_COMPATIBILITY_PROFILE = 0x10;

#define GPU_DEFAULT_INIT_FLAGS 0

static const Uint32 NONE = 0x0;

typedef Uint32 BatchFlagEnum;
static const BatchFlagEnum BATCH_XY = 0x1;
static const BatchFlagEnum BATCH_XYZ = 0x2;
static const BatchFlagEnum BATCH_ST = 0x4;
static const BatchFlagEnum BATCH_RGB = 0x8;
static const BatchFlagEnum BATCH_RGBA = 0x10;
static const BatchFlagEnum BATCH_RGB8 = 0x20;
static const BatchFlagEnum BATCH_RGBA8 = 0x40;

#define BATCH_XY_ST (BATCH_XY | BATCH_ST)
#define BATCH_XYZ_ST (BATCH_XYZ | BATCH_ST)
#define BATCH_XY_RGB (BATCH_XY | BATCH_RGB)
#define BATCH_XYZ_RGB (BATCH_XYZ | BATCH_RGB)
#define BATCH_XY_RGBA (BATCH_XY | BATCH_RGBA)
#define BATCH_XYZ_RGBA (BATCH_XYZ | BATCH_RGBA)
#define BATCH_XY_ST_RGBA (BATCH_XY | BATCH_ST | BATCH_RGBA)
#define BATCH_XYZ_ST_RGBA (BATCH_XYZ | BATCH_ST | BATCH_RGBA)
#define BATCH_XY_RGB8 (BATCH_XY | BATCH_RGB8)
#define BATCH_XYZ_RGB8 (BATCH_XYZ | BATCH_RGB8)
#define BATCH_XY_RGBA8 (BATCH_XY | BATCH_RGBA8)
#define BATCH_XYZ_RGBA8 (BATCH_XYZ | BATCH_RGBA8)
#define BATCH_XY_ST_RGBA8 (BATCH_XY | BATCH_ST | BATCH_RGBA8)
#define BATCH_XYZ_ST_RGBA8 (BATCH_XYZ | BATCH_ST | BATCH_RGBA8)


typedef Uint32 FlipEnum;
static const FlipEnum FLIP_NONE = 0x0;
static const FlipEnum FLIP_HORIZONTAL = 0x1;
static const FlipEnum FLIP_VERTICAL = 0x2;


typedef Uint32 TypeEnum;
// Use OpenGL's values for simpler translation
static const TypeEnum TYPE_BYTE = 0x1400;
static const TypeEnum TYPE_UNSIGNED_BYTE = 0x1401;
static const TypeEnum TYPE_SHORT = 0x1402;
static const TypeEnum TYPE_UNSIGNED_SHORT = 0x1403;
static const TypeEnum TYPE_INT = 0x1404;
static const TypeEnum TYPE_UNSIGNED_INT = 0x1405;
static const TypeEnum TYPE_FLOAT = 0x1406;
static const TypeEnum TYPE_DOUBLE = 0x140A;

typedef enum {
    VERTEX_SHADER = 0,
    FRAGMENT_SHADER = 1,
    PIXEL_SHADER = 1,
    GEOMETRY_SHADER = 2
} ShaderEnum;

typedef enum {
    LANGUAGE_NONE = 0,
    LANGUAGE_ARB_ASSEMBLY = 1,
    LANGUAGE_GLSL = 2,
    LANGUAGE_GLSLES = 3,
    LANGUAGE_HLSL = 4,
    LANGUAGE_CG = 5
} ShaderLanguageEnum;

typedef struct AttributeFormat {
    bool is_per_sprite;  // Per-sprite values are expanded to 4 vertices
    int num_elems_per_value;
    TypeEnum type;  // TYPE_FLOAT, TYPE_INT, TYPE_UNSIGNED_INT, etc.
    bool normalize;
    int stride_bytes;  // Number of bytes between two vertex specifications
    int offset_bytes;  // Number of bytes to skip at the beginning of 'values'
} AttributeFormat;

typedef struct Attribute {
    int location;
    void* values;  // Expect 4 values for each sprite
    AttributeFormat format;
} Attribute;

typedef struct AttributeSource {
    bool enabled;
    int num_values;
    void* next_value;
    // Automatic storage format
    int per_vertex_storage_stride_bytes;
    int per_vertex_storage_offset_bytes;
    int per_vertex_storage_size;  // Over 0 means that the per-vertex storage has been automatically allocated
    void* per_vertex_storage;  // Could point to the attribute's values or to allocated storage
    Attribute attribute;
} AttributeSource;


/* The window corresponding to 'windowID' will be used to create the rendering context instead of creating a new window. */
void SetInitWindow(Uint32 windowID);

/* Returns the window ID that has been set via SetInitWindow(). */
Uint32 GetInitWindow(void);

/* Set special flags to use for initialization. Set these before calling Init().
 * \param flags An OR'ed combination of InitFlagEnum flags.  Default flags (0) enable late swap vsync and double buffering. */
void SetPreInitFlags(InitFlagEnum flags);

/* Returns the current special flags to use for initialization. */
InitFlagEnum GetPreInitFlags(void);

/* Set required features to use for initialization. Set these before calling Init().
 * \param features An OR'ed combination of FeatureEnum flags.  Required features will force Init() to create a renderer that supports all of the given flags or else fail. */
void SetRequiredFeatures(FeatureEnum features);

/* Returns the current required features to use for initialization. */
FeatureEnum GetRequiredFeatures(void);


/* Initializes SDL and SDL_gpu.  Creates a window and goes through the renderer order to create a renderer context.
 */
Target* Init(Uint16 w, Uint16 h, WindowFlagEnum SDL_flags);

/* Checks for important GPU features which may not be supported depending on a device's extension support.  Feature flags (FEATURE_*) can be bitwise OR'd together. 
 * \return 1 if all of the passed features are enabled/supported
 * \return 0 if any of the passed features are disabled/unsupported
 */
bool IsFeatureEnabled(FeatureEnum feature);

/* Clean up the renderer state and shut down SDL_gpu. */
void Quit(void);

/* Returns an initialized DeviceID. */
DeviceID MakeDeviceID(const char* name, RendererEnum renderer, int major_version, int minor_version);

/* \return The current renderer */
Renderer* GetCurrentRenderer(void);

/* Reapplies the renderer state to the backend API (e.g. OpenGL, Direct3D).  Use this if you want SDL_gpu to be able to render after you've used direct backend calls. */
void ResetRendererState(void);

/* Sets the default image blitting anchor for newly created images.
 * \see SetAnchor
 */
void SetDefaultAnchor(float anchor_x, float anchor_y);

/* Returns the default image blitting anchor through the given variables.
 * \see GetAnchor
 */
void GetDefaultAnchor(float* anchor_x, float* anchor_y);


// Context / window controls

/* \return The renderer's current context target. */
Target* GetContextTarget(void);

/* \return The target that is associated with the given windowID. */
Target* GetWindowTarget(Uint32 windowID);

/* Creates a separate context for the given window using the current renderer and returns a Target that represents it. */
Target* CreateTargetFromWindow(Uint32 windowID);

/* Makes the given window the current rendering destination for the given context target.
 * This also makes the target the current context for image loading and window operations.
 * If the target does not represent a window, this does nothing.
 */
void MakeCurrent(Target* target, Uint32 windowID);

/* Change the actual size of the current context target's window.  This resets the virtual resolution and viewport of the context target.
 * Aside from direct resolution changes, this should also be called in response to SDL_WINDOWEVENT_RESIZED window events for resizable windows. */
bool SetWindowResolution(Uint16 w, Uint16 h);

/* Enable/disable fullscreen mode for the current context target's window.
 * On some platforms, this may destroy the renderer context and require that textures be reloaded.  Unfortunately, SDL does not provide a notification mechanism for this.
 * \param enable_fullscreen If true, make the application go fullscreen.  If false, make the application go to windowed mode.
 * \param use_desktop_resolution If true, lets the window change its resolution when it enters fullscreen mode (via SDL_WINDOW_FULLSCREEN_DESKTOP).
 * \return 0 if the new mode is windowed, 1 if the new mode is fullscreen.  */
bool SetFullscreen(bool enable_fullscreen, bool use_desktop_resolution);

/* Returns true if the current context target's window is in fullscreen mode. */
bool GetFullscreen(void);

/* Enables/disables alpha blending for shape rendering on the current window. */
void SetShapeBlending(bool enable);

/* Translates a blend preset into a blend mode. */
BlendMode  GetBlendModeFromPreset(BlendPresetEnum preset);

/* Sets the blending component functions for shape rendering. */
void SetShapeBlendFunction(BlendFuncEnum source_color, BlendFuncEnum dest_color, BlendFuncEnum source_alpha, BlendFuncEnum dest_alpha);

/* Sets the blending component equations for shape rendering. */
void SetShapeBlendEquation(BlendEqEnum color_equation, BlendEqEnum alpha_equation);
	
/* Sets the blending mode for shape rendering on the current window, if supported by the renderer. */
void SetShapeBlendMode(BlendPresetEnum mode);

/* Sets the thickness of lines for the current context. 
 * \param thickness New line thickness in pixels measured across the line.  Default is 1.0f.
 * \return The old thickness value
 */
float SetLineThickness(float thickness);

/* Returns the current line thickness value. */
float GetLineThickness(void);




/* Creates a target that aliases the given target.  Aliases can be used to store target settings (e.g. viewports) for easy switching.
 * FreeTarget() frees the alias's memory, but does not affect the original. */
Target* CreateAliasTarget(Target* target);

/* Creates a new render target from the given image.  It can then be accessed from image->target. */
Target* LoadTarget(Image* image);

/* Deletes a render target in the proper way for this renderer. */
void FreeTarget(Target* target);

/* Change the logical size of the given target.  Rendering to this target will be scaled as if the dimensions were actually the ones given. */
void SetVirtualResolution(Target* target, Uint16 w, Uint16 h);

/* Query the logical size of the given target. */
void GetVirtualResolution(Target* target, Uint16* w, Uint16* h);


/* Reset the logical size of the given target to its original value. */
void UnsetVirtualResolution(Target* target);

/* \return A GPU_Rect with the given values. */
GPU_Rect MakeRect(float x, float y, float w, float h);

/* \return An SDL_Color with the given values. */
SDL_Color MakeColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* Sets the given target's viewport. */
void SetViewport(Target* target, GPU_Rect viewport);

/* Resets the given target's viewport to the entire target area. */
void UnsetViewport(Target* target);

/* \return A Camera with position (0, 0, -10), angle of 0, and zoom of 1. */
Camera GetDefaultCamera(void);

/* \return The camera of the given render target.  If target is NULL, returns the default camera. */
Camera GetCamera(Target* target);

/* Sets the current render target's current camera.
 * \param target A pointer to the target that will copy this camera.
 * \param cam A pointer to the camera data to use or NULL to use the default camera.
 * \return The old camera. */
Camera SetCamera(Target* target, Camera* cam);

/* Enables or disables using the built-in camera matrix transforms. */
void EnableCamera(Target* target, bool use_camera);

/* Returns 1 if the camera transforms are enabled, 0 otherwise. */
bool IsCameraEnabled(Target* target);

/* \return The RGBA color of a pixel. */
SDL_Color GetPixel(Target* target, Sint16 x, Sint16 y);

/* \return The RGBA colors of a rect. */
bool GetImageData(Target* target, unsigned char* data, Sint16 x, Sint16 y, Sint16 w, Sint16 h);

/* Sets the clipping rect for the given render target. */
GPU_Rect SetClipRect(Target* target, GPU_Rect rect);

/* Sets the clipping rect for the given render target. */
GPU_Rect SetClip(Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h);

/* Turns off clipping for the given target. */
void UnsetClip(Target* target);

/* Returns true if the given rects A and B overlap, in which case it also fills the given result rect with the intersection.  `result` can be NULL if you don't need the intersection. */
bool IntersectRect(GPU_Rect A, GPU_Rect B, GPU_Rect* result);

/* Returns true if the given target's clip rect and the given B rect overlap, in which case it also fills the given result rect with the intersection.  `result` can be NULL if you don't need the intersection.
 * If the target doesn't have a clip rect enabled, this uses the whole target area.
 */
bool IntersectClipRect(Target* target, GPU_Rect B, GPU_Rect* result);

/* Sets the modulation color for subsequent drawing of images and shapes on the given target. 
 *  This has a cumulative effect with the image coloring functions.
 *  e.g. SetRGB(image, 255, 128, 0); SetTargetRGB(target, 128, 128, 128);
 *  Would make the image draw with color of roughly (128, 64, 0).
 */
void SetTargetColor(Target* target, SDL_Color color);

/* Sets the modulation color for subsequent drawing of images and shapes on the given target. 
 *  This has a cumulative effect with the image coloring functions.
 *  e.g. SetRGB(image, 255, 128, 0); SetTargetRGB(target, 128, 128, 128);
 *  Would make the image draw with color of roughly (128, 64, 0).
 */
void SetTargetRGB(Target* target, Uint8 r, Uint8 g, Uint8 b);

/* Sets the modulation color for subsequent drawing of images and shapes on the given target. 
 *  This has a cumulative effect with the image coloring functions.
 *  e.g. SetRGB(image, 255, 128, 0); SetTargetRGB(target, 128, 128, 128);
 *  Would make the image draw with color of roughly (128, 64, 0).
 */
void SetTargetRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* Unsets the modulation color for subsequent drawing of images and shapes on the given target. 
 *  This has the same effect as coloring with pure opaque white (255, 255, 255, 255).
 */
void UnsetTargetColor(Target* target);


/* Save surface to a file.
 * With a format of FILE_AUTO, the file type is deduced from the extension.  Supported formats are: png, bmp, tga.
 * Returns 0 on failure. */
bool SaveSurface(SDL_Surface* surface, const char* filename, FileFormatEnum format);

/* Save surface to a RWops stream.
 * Does not support format of FILE_AUTO, because the file type cannot be deduced.  Supported formats are: png, bmp, tga.
 * Returns 0 on failure. */
bool SaveSurface_RW(SDL_Surface* surface, SDL_RWops* rwops, bool free_rwops, FileFormatEnum format);


/* Create a new, blank image with the given format.  Don't forget to FreeImage() it.
	 * \param w Image width in pixels
	 * \param h Image height in pixels
	 * \param format Format of color channels.
	 */
Image* CreateImage(Uint16 w, Uint16 h, FormatEnum format);

/* Load image from an image file that is supported by this renderer.  Don't forget to FreeImage() it. */
Image* LoadImage(const char* filename);

/* Load image from an image file in memory.  Don't forget to FreeImage() it. */
Image* LoadImage_RW(SDL_RWops* rwops, bool free_rwops);

/* Creates an image that aliases the given image.  Aliases can be used to store image settings (e.g. modulation color) for easy switching.
 * FreeImage() frees the alias's memory, but does not affect the original. */
Image* CreateAliasImage(Image* image);

/* Copy an image to a new image.  Don't forget to FreeImage() both. */
Image* CopyImage(Image* image);

/* Deletes an image in the proper way for this renderer.  Also deletes the corresponding Target if applicable.  Be careful not to use that target afterward! */
void FreeImage(Image* image);

/* Change the logical size of the given image.  Rendering this image will scaled it as if the dimensions were actually the ones given. */
void SetImageVirtualResolution(Image* image, Uint16 w, Uint16 h);

/* Reset the logical size of the given image to its original value. */
void UnsetImageVirtualResolution(Image* image);

/* Update an image from surface data.  Ignores virtual resolution on the image so the number of pixels needed from the surface is known. */
void UpdateImage(Image* image, const GPU_Rect* image_rect, SDL_Surface* surface, const GPU_Rect* surface_rect);

/* Update an image from an array of pixel data.  Ignores virtual resolution on the image so the number of pixels needed from the surface is known. */
void UpdateImageBytes(Image* image, const GPU_Rect* image_rect, const unsigned char* bytes, int bytes_per_row);

/* Update an image from surface data, replacing its underlying texture to allow for size changes.  Ignores virtual resolution on the image so the number of pixels needed from the surface is known. */
bool ReplaceImage(Image* image, SDL_Surface* surface, const GPU_Rect* surface_rect);

/* Save image to a file.
 * With a format of FILE_AUTO, the file type is deduced from the extension.  Supported formats are: png, bmp, tga.
 * Returns 0 on failure. */
bool SaveImage(Image* image, const char* filename, FileFormatEnum format);

/* Save image to a RWops stream.
 * Does not support format of FILE_AUTO, because the file type cannot be deduced.  Supported formats are: png, bmp, tga.
 * Returns 0 on failure. */
bool SaveImage_RW(Image* image, SDL_RWops* rwops, bool free_rwops, FileFormatEnum format);

/* Loads mipmaps for the given image, if supported by the renderer. */
void GenerateMipmaps(Image* image);

/* Sets the modulation color for subsequent drawing of the given image. */
void SetColor(Image* image, SDL_Color color);

/* Sets the modulation color for subsequent drawing of the given image. */
void SetRGB(Image* image, Uint8 r, Uint8 g, Uint8 b);

/* Sets the modulation color for subsequent drawing of the given image. */
void SetRGBA(Image* image, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* Unsets the modulation color for subsequent drawing of the given image.
 *  This is equivalent to coloring with pure opaque white (255, 255, 255, 255). */
void UnsetColor(Image* image);

/* Gets the current alpha blending setting. */
bool GetBlending(Image* image);

/* Enables/disables alpha blending for the given image. */
void SetBlending(Image* image, bool enable);

/* Sets the blending component functions. */
void SetBlendFunction(Image* image, BlendFuncEnum source_color, BlendFuncEnum dest_color, BlendFuncEnum source_alpha, BlendFuncEnum dest_alpha);

/* Sets the blending component equations. */
void SetBlendEquation(Image* image, BlendEqEnum color_equation, BlendEqEnum alpha_equation);

/* Sets the blending mode, if supported by the renderer. */
void SetBlendMode(Image* image, BlendPresetEnum mode);

/* Sets the image filtering mode, if supported by the renderer. */
void SetImageFilter(Image* image, FilterEnum filter);

/* Sets the image anchor, which is the point about which the image is blitted.  The default is to blit the image on-center (0.5, 0.5).  The anchor is in normalized coordinates (0.0-1.0). */
void SetAnchor(Image* image, float anchor_x, float anchor_y);

/* Returns the image anchor via the passed parameters.  The anchor is in normalized coordinates (0.0-1.0). */
void GetAnchor(Image* image, float* anchor_x, float* anchor_y);

/* Gets the current pixel snap setting.  The default value is SNAP_POSITION_AND_DIMENSIONS.  */
SnapEnum GetSnapMode(Image* image);

/* Sets the pixel grid snapping mode for the given image. */
void SetSnapMode(Image* image, SnapEnum mode);

/* Sets the image wrapping mode, if supported by the renderer. */
void SetWrapMode(Image* image, WrapEnum wrap_mode_x, WrapEnum wrap_mode_y);



// Surface / Image / Target conversions

/* Copy SDL_Surface data into a new Image.  Don't forget to SDL_FreeSurface() the surface and FreeImage() the image.*/
Image* CopyImageFromSurface(SDL_Surface* surface);

/* Copy Target data into a new Image.  Don't forget to FreeImage() the image.*/
Image* CopyImageFromTarget(Target* target);

/* Copy Target data into a new SDL_Surface.  Don't forget to SDL_FreeSurface() the surface.*/
SDL_Surface* CopySurfaceFromTarget(Target* target);

/* Copy Image data into a new SDL_Surface.  Don't forget to SDL_FreeSurface() the surface and FreeImage() the image.*/
SDL_Surface* CopySurfaceFromImage(Image* image);


/* Clears the contents of the given render target.  Fills the target with color {0, 0, 0, 0}. */
void Clear(Target* target);

/* Fills the given render target with a color. */
void ClearColor(Target* target, SDL_Color color);

/* Fills the given render target with a color (alpha is 255, fully opaque). */
void ClearRGB(Target* target, Uint8 r, Uint8 g, Uint8 b);

/* Fills the given render target with a color. */
void ClearRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* Draws the given image to the given render target.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param x Destination x-position
    * \param y Destination y-position */
void Blit(Image* image, GPU_Rect* src_rect, Target* target, float x, float y);

/* Rotates and draws the given image to the given render target.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param x Destination x-position
    * \param y Destination y-position
    * \param degrees Rotation angle (in degrees) */
void BlitRotate(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees);

/* Scales and draws the given image to the given render target.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param x Destination x-position
    * \param y Destination y-position
    * \param scaleX Horizontal stretch factor
    * \param scaleY Vertical stretch factor */
void BlitScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float scaleX, float scaleY);

/* Scales, rotates, and draws the given image to the given render target.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param x Destination x-position
    * \param y Destination y-position
    * \param degrees Rotation angle (in degrees)
    * \param scaleX Horizontal stretch factor
    * \param scaleY Vertical stretch factor */
void BlitTransform(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees, float scaleX, float scaleY);

/* Scales, rotates around a pivot point, and draws the given image to the given render target.  The drawing point (x, y) coincides with the pivot point on the src image (pivot_x, pivot_y).
	* \param src_rect The region of the source image to use.  Pass NULL for the entire image.
	* \param x Destination x-position
	* \param y Destination y-position
	* \param pivot_x Pivot x-position (in image coordinates)
	* \param pivot_y Pivot y-position (in image coordinates)
	* \param degrees Rotation angle (in degrees)
	* \param scaleX Horizontal stretch factor
	* \param scaleY Vertical stretch factor */
void BlitTransformX(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float pivot_x, float pivot_y, float degrees, float scaleX, float scaleY);

/* Draws the given image to the given render target, scaling it to fit the destination region.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param dest_rect The region of the destination target image to draw upon.  Pass NULL for the entire target.
    */
void BlitRect(Image* image, GPU_Rect* src_rect, Target* target, GPU_Rect* dest_rect);

/* Draws the given image to the given render target, scaling it to fit the destination region.
    * \param src_rect The region of the source image to use.  Pass NULL for the entire image.
    * \param dest_rect The region of the destination target image to draw upon.  Pass NULL for the entire target.
	* \param degrees Rotation angle (in degrees)
	* \param pivot_x Pivot x-position (in image coordinates)
	* \param pivot_y Pivot y-position (in image coordinates)
	* \param flip_direction A FlipEnum value (or bitwise OR'd combination) that specifies which direction the image should be flipped.
    */
void BlitRectX(Image* image, GPU_Rect* src_rect, Target* target, GPU_Rect* dest_rect, float degrees, float pivot_x, float pivot_y, FlipEnum flip_direction);


/* Renders triangles from the given set of vertices.  This lets you render arbitrary 2D geometry.  It is a direct path to the GPU, so the format is different than typical SDL_gpu calls.
 * \param values A tightly-packed array of vertex position (e.g. x,y), texture coordinates (e.g. s,t), and color (e.g. r,g,b,a) values.  Texture coordinates and color values are expected to be already normalized to 0.0 - 1.0.  Pass NULL to render with only custom shader attributes.
 * \param indices If not NULL, this is used to specify which vertices to use and in what order (i.e. it indexes the vertices in the 'values' array).
 * \param flags Bit flags to control the interpretation of the 'values' array parameters.
 */
void TriangleBatch(Image* image, Target* target, unsigned short num_vertices, float* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags);

/* Renders triangles from the given set of vertices.  This lets you render arbitrary 2D geometry.  It is a direct path to the GPU, so the format is different than typical SDL_gpu calls.
 * \param values A tightly-packed array of vertex position (e.g. x,y), texture coordinates (e.g. s,t), and color (e.g. r,g,b,a) values.  Texture coordinates and color values are expected to be already normalized to 0.0 - 1.0 (or 0 - 255 for 8-bit color components).  Pass NULL to render with only custom shader attributes.
 * \param indices If not NULL, this is used to specify which vertices to use and in what order (i.e. it indexes the vertices in the 'values' array).
 * \param flags Bit flags to control the interpretation of the 'values' array parameters.
 */
void TriangleBatchX(Image* image, Target* target, unsigned short num_vertices, void* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags);

/* Send all buffered blitting data to the current context target. */
void FlushBlitBuffer(void);

/* Updates the given target's associated window.  For non-context targets (e.g. image targets), this will flush the blit buffer. */
void Flip(Target* target);



/* Renders a colored point.
 * \param target The destination render target
 * \param x x-coord of the point
 * \param y y-coord of the point
 * \param color The color of the shape to render
 */
void Pixel(Target* target, float x, float y, SDL_Color color);

/* Renders a colored line.
 * \param target The destination render target
 * \param x1 x-coord of starting point
 * \param y1 y-coord of starting point
 * \param x2 x-coord of ending point
 * \param y2 y-coord of ending point
 * \param color The color of the shape to render
 */
void Line(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);

/* Renders a colored arc curve (circle segment).
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param radius The radius of the circle / distance from the center point that rendering will occur
 * \param start_angle The angle to start from, in degrees.  Measured clockwise from the positive x-axis.
 * \param end_angle The angle to end at, in degrees.  Measured clockwise from the positive x-axis.
 * \param color The color of the shape to render
 */
void Arc(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color);

/* Renders a colored filled arc (circle segment / pie piece).
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param radius The radius of the circle / distance from the center point that rendering will occur
 * \param start_angle The angle to start from, in degrees.  Measured clockwise from the positive x-axis.
 * \param end_angle The angle to end at, in degrees.  Measured clockwise from the positive x-axis.
 * \param color The color of the shape to render
 */
void ArcFilled(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color);

/* Renders a colored circle outline.
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param radius The radius of the circle / distance from the center point that rendering will occur
 * \param color The color of the shape to render
 */
void Circle(Target* target, float x, float y, float radius, SDL_Color color);

/* Renders a colored filled circle.
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param radius The radius of the circle / distance from the center point that rendering will occur
 * \param color The color of the shape to render
 */
void CircleFilled(Target* target, float x, float y, float radius, SDL_Color color);

/* Renders a colored ellipse outline.
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param rx x-radius of ellipse
 * \param ry y-radius of ellipse
 * \param degrees The angle to rotate the ellipse
 * \param color The color of the shape to render
 */
void Ellipse(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color);

/* Renders a colored filled ellipse.
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param rx x-radius of ellipse
 * \param ry y-radius of ellipse
 * \param degrees The angle to rotate the ellipse
 * \param color The color of the shape to render
 */
void EllipseFilled(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color);

/* Renders a colored annular sector outline (ring segment).
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param inner_radius The inner radius of the ring
 * \param outer_radius The outer radius of the ring
 * \param start_angle The angle to start from, in degrees.  Measured clockwise from the positive x-axis.
 * \param end_angle The angle to end at, in degrees.  Measured clockwise from the positive x-axis.
 * \param color The color of the shape to render
 */
void Sector(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color);

/* Renders a colored filled annular sector (ring segment).
 * \param target The destination render target
 * \param x x-coord of center point
 * \param y y-coord of center point
 * \param inner_radius The inner radius of the ring
 * \param outer_radius The outer radius of the ring
 * \param start_angle The angle to start from, in degrees.  Measured clockwise from the positive x-axis.
 * \param end_angle The angle to end at, in degrees.  Measured clockwise from the positive x-axis.
 * \param color The color of the shape to render
 */
void SectorFilled(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color);

/* Renders a colored triangle outline.
 * \param target The destination render target
 * \param x1 x-coord of first point
 * \param y1 y-coord of first point
 * \param x2 x-coord of second point
 * \param y2 y-coord of second point
 * \param x3 x-coord of third point
 * \param y3 y-coord of third point
 * \param color The color of the shape to render
 */
void Tri(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color);

/* Renders a colored filled triangle.
 * \param target The destination render target
 * \param x1 x-coord of first point
 * \param y1 y-coord of first point
 * \param x2 x-coord of second point
 * \param y2 y-coord of second point
 * \param x3 x-coord of third point
 * \param y3 y-coord of third point
 * \param color The color of the shape to render
 */
void TriFilled(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color);

/* Renders a colored rectangle outline.
 * \param target The destination render target
 * \param x1 x-coord of top-left corner
 * \param y1 y-coord of top-left corner
 * \param x2 x-coord of bottom-right corner
 * \param y2 y-coord of bottom-right corner
 * \param color The color of the shape to render
 */
void Rectangle(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);

/* Renders a colored rectangle outline.
 * \param target The destination render target
 * \param rect The rectangular area to draw
 * \param color The color of the shape to render
 */
void Rectangle2(Target* target, GPU_Rect rect, SDL_Color color);

/* Renders a colored filled rectangle.
 * \param target The destination render target
 * \param x1 x-coord of top-left corner
 * \param y1 y-coord of top-left corner
 * \param x2 x-coord of bottom-right corner
 * \param y2 y-coord of bottom-right corner
 * \param color The color of the shape to render
 */
void RectangleFilled(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);

/* Renders a colored filled rectangle.
 * \param target The destination render target
 * \param rect The rectangular area to draw
 * \param color The color of the shape to render
 */
void RectangleFilled2(Target* target, GPU_Rect rect, SDL_Color color);

/* Renders a colored rounded (filleted) rectangle outline.
 * \param target The destination render target
 * \param x1 x-coord of top-left corner
 * \param y1 y-coord of top-left corner
 * \param x2 x-coord of bottom-right corner
 * \param y2 y-coord of bottom-right corner
 * \param radius The radius of the corners
 * \param color The color of the shape to render
 */
void RectangleRound(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color);

/* Renders a colored rounded (filleted) rectangle outline.
 * \param target The destination render target
 * \param rect The rectangular area to draw
 * \param radius The radius of the corners
 * \param color The color of the shape to render
 */
void RectangleRound2(Target* target, GPU_Rect rect, float radius, SDL_Color color);

/* Renders a colored filled rounded (filleted) rectangle.
 * \param target The destination render target
 * \param x1 x-coord of top-left corner
 * \param y1 y-coord of top-left corner
 * \param x2 x-coord of bottom-right corner
 * \param y2 y-coord of bottom-right corner
 * \param radius The radius of the corners
 * \param color The color of the shape to render
 */
void RectangleRoundFilled(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color);

/* Renders a colored filled rounded (filleted) rectangle.
 * \param target The destination render target
 * \param rect The rectangular area to draw
 * \param radius The radius of the corners
 * \param color The color of the shape to render
 */
void RectangleRoundFilled2(Target* target, GPU_Rect rect, float radius, SDL_Color color);

/* Renders a colored polygon outline.  The vertices are expected to define a convex polygon.
 * \param target The destination render target
 * \param num_vertices Number of vertices (x and y pairs)
 * \param vertices An array of vertex positions stored as interlaced x and y coords, e.g. {x1, y1, x2, y2, ...}
 * \param color The color of the shape to render
 */
void Polygon(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color);

/* Renders a colored filled polygon.  The vertices are expected to define a convex polygon.
 * \param target The destination render target
 * \param num_vertices Number of vertices (x and y pairs)
 * \param vertices An array of vertex positions stored as interlaced x and y coords, e.g. {x1, y1, x2, y2, ...}
 * \param color The color of the shape to render
 */
void PolygonFilled(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color);



/* Creates a new, empty shader program.  You will need to compile shaders, attach them to the program, then link the program.
 * \see AttachShader
 * \see LinkShaderProgram
 */
Uint32 CreateShaderProgram(void);

/* Deletes a shader program. */
void FreeShaderProgram(Uint32 program_object);

/* Loads shader source from an SDL_RWops, compiles it, and returns the new shader object. */
Uint32 CompileShader_RW(ShaderEnum shader_type, SDL_RWops* shader_source, bool free_rwops);

/* Compiles shader source and returns the new shader object. */
Uint32 CompileShader(ShaderEnum shader_type, const char* shader_source);

/* Loads shader source from a file, compiles it, and returns the new shader object. */
Uint32 LoadShader(ShaderEnum shader_type, const char* filename);

/* Creates and links a shader program with the given shader objects. */
Uint32 LinkShaders(Uint32 shader_object1, Uint32 shader_object2);

/* Creates and links a shader program with the given shader objects. */
Uint32 LinkManyShaders(Uint32 *shader_objects, int count);

/* Deletes a shader object. */
void FreeShader(Uint32 shader_object);

/* Attaches a shader object to a shader program for future linking. */
void AttachShader(Uint32 program_object, Uint32 shader_object);

/* Detaches a shader object from a shader program. */
void DetachShader(Uint32 program_object, Uint32 shader_object);

/* Links a shader program with any attached shader objects. */
bool LinkShaderProgram(Uint32 program_object);

/* \return The current shader program */
Uint32 GetCurrentShaderProgram(void);

/* Returns 1 if the given shader program is a default shader for the current context, 0 otherwise. */
bool IsDefaultShaderProgram(Uint32 program_object);

/* Activates the given shader program.  Passing NULL for 'block' will disable the built-in shader variables for custom shaders until a ShaderBlock is set again. */
void ActivateShaderProgram(Uint32 program_object, ShaderBlock* block);

/* Deactivates the current shader program (activates program 0). */
void DeactivateShaderProgram(void);

/* Returns the last shader log message. */
const char* GetShaderMessage(void);

/* Returns an integer representing the location of the specified attribute shader variable. */
int GetAttributeLocation(Uint32 program_object, const char* attrib_name);

/* Returns a filled AttributeFormat object. */
AttributeFormat MakeAttributeFormat(int num_elems_per_vertex, TypeEnum type, bool normalize, int stride_bytes, int offset_bytes);

/* Returns a filled Attribute object. */
Attribute MakeAttribute(int location, void* values, AttributeFormat format);

/* Returns an integer representing the location of the specified uniform shader variable. */
int GetUniformLocation(Uint32 program_object, const char* uniform_name);

/* Loads the given shader program's built-in attribute and uniform locations. */
ShaderBlock LoadShaderBlock(Uint32 program_object, const char* position_name, const char* texcoord_name, const char* color_name, const char* modelViewMatrix_name);

/* Sets the given image unit to the given image so that a custom shader can sample multiple textures.
    \param image The source image/texture.  Pass NULL to disable the image unit.
    \param location The uniform location of a texture sampler
    \param image_unit The index of the texture unit to set.  0 is the first unit, which is used by SDL_gpu's blitting functions.  1 would be the second unit. */
void SetShaderImage(Image* image, int location, int image_unit);

/* Fills "values" with the value of the uniform shader variable at the given location. */
void GetUniformiv(Uint32 program_object, int location, int* values);

/* Sets the value of the integer uniform shader variable at the given location.
    This is equivalent to calling SetUniformiv(location, 1, 1, &value). */
void SetUniformi(int location, int value);

/* Sets the value of the integer uniform shader variable at the given location. */
void SetUniformiv(int location, int num_elements_per_value, int num_values, int* values);

/* Fills "values" with the value of the uniform shader variable at the given location. */
void GetUniformuiv(Uint32 program_object, int location, unsigned int* values);

/* Sets the value of the unsigned integer uniform shader variable at the given location.
    This is equivalent to calling SetUniformuiv(location, 1, 1, &value). */
void SetUniformui(int location, unsigned int value);

/* Sets the value of the unsigned integer uniform shader variable at the given location. */
void SetUniformuiv(int location, int num_elements_per_value, int num_values, unsigned int* values);

/* Fills "values" with the value of the uniform shader variable at the given location. */
void GetUniformfv(Uint32 program_object, int location, float* values);

/* Sets the value of the floating point uniform shader variable at the given location.
    This is equivalent to calling SetUniformfv(location, 1, 1, &value). */
void SetUniformf(int location, float value);

/* Sets the value of the floating point uniform shader variable at the given location. */
void SetUniformfv(int location, int num_elements_per_value, int num_values, float* values);

/* Fills "values" with the value of the uniform shader variable at the given location.  The results are identical to calling GetUniformfv().  Matrices are gotten in column-major order. */
void GetUniformMatrixfv(Uint32 program_object, int location, float* values);

/* Sets the value of the matrix uniform shader variable at the given location.  The size of the matrices sent is specified by num_rows and num_columns.  Rows and columns must be between 2 and 4. */
void SetUniformMatrixfv(int location, int num_matrices, int num_rows, int num_columns, bool transpose, float* values);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributef(int location, float value);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributei(int location, int value);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributeui(int location, unsigned int value);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributefv(int location, int num_elements, float* value);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributeiv(int location, int num_elements, int* value);

/* Sets a constant-value shader attribute that will be used for each rendered vertex. */
void SetAttributeuiv(int location, int num_elements, unsigned int* value);

/* Enables a shader attribute and sets its source data. */
void SetAttributeSource(int num_values, Attribute source);



bool IsExtensionSupported(const char* extension_str);

void BlitTransformA(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, AffineTransform* transform);
void BlitTransformAScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, AffineTransform* transform, float scale_x, float scale_y);

void PolygonTextureFilled(Target* target, unsigned int num_vertices, float* vertices, Image* image, float texture_x, float texture_y);
void PolygonTextureFilledNPOT(Target* target, unsigned int num_vertices, float* vertices, Image* image);
void TextureLine(Target* target, float x1, float y1, float x2, float y2, Image* image, float texture_x, float texture_y);
void TextureLineNPOT(Target* target, float x1, float y1, float x2, float y2, Image* image);

typedef GPU_Color (*ColorCallback)(float x, float y, void* userdata);
void BlitTransformAColor(Image* image, Target* target, float x, float y, AffineTransform* transform, GPU_Color colors[4]);
void PolygonColorFilled(Target* target, unsigned int num_vertices, float* vertices, ColorCallback colorfunc, void* userdata);
void ColorLine(Target* target, float x1, float y1, float x2, float y2, ColorCallback colorfunc, void* userdata);


NS_GPU_END

#endif

