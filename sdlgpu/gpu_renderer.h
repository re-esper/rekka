#ifndef _RENDERERIMPL_H
#define _RENDERERIMPL_H

#include "xgpu.h"
#include "gpu_render_device.h"

NS_GPU_BEGIN

// Internal API for managing window mappings
void AddWindowMapping(Target* target);
void RemoveWindowMapping(Uint32 windowID);
void RemoveWindowMappingByTarget(Target* target);

void SetShaderBlock(ShaderBlock block);

/* Private implementation of renderer members. */
class Renderer {
public:
	Renderer(RenderDevice* device);
	Target* Init(DeviceID renderer_request, Uint16 w, Uint16 h, WindowFlagEnum SDL_flags);	 
	Target* CreateTargetFromWindow(Uint32 windowID, Target* target);        
	Target* CreateAliasTarget(Target* target);    
	void MakeCurrent(Target* target, Uint32 windowID);	
	void SetAsCurrent(RenderDevice* renderer);		
	void ResetRendererState(RenderDevice* renderer);
	bool SetWindowResolution(Uint16 w, Uint16 h);	
	void SetVirtualResolution(Target* target, Uint16 w, Uint16 h);
	void UnsetVirtualResolution(Target* target);	
	void Quit(RenderDevice* renderer);	
	bool SetFullscreen(bool enable_fullscreen, bool use_desktop_resolution);
	Camera SetCamera(Target* target, Camera* cam);
	Image* CreateImage(Uint16 w, Uint16 h, FormatEnum format);		
	Image* CreateAliasImage(Image* image);	
	bool SaveImage(Image* image, const char* filename, FileFormatEnum format);	
	Image* CopyImage(Image* image);	
	void UpdateImage(Image* image, const GPU_Rect* image_rect, SDL_Surface* surface, const GPU_Rect* surface_rect);	
	void UpdateImageBytes(Image* image, const GPU_Rect* image_rect, const unsigned char* bytes, int bytes_per_row);		
	bool ReplaceImage(Image* image, SDL_Surface* surface, const GPU_Rect* surface_rect);	
	Image* CopyImageFromSurface(SDL_Surface* surface);	
	Image* CopyImageFromTarget(Target* target);	
	SDL_Surface* CopySurfaceFromTarget(Target* target);	
	SDL_Surface* CopySurfaceFromImage(Image* image);	
	void FreeImage(Image* image);
		
	Target* LoadTarget(Image* image);
	void FreeTarget(Target* target);

	void Blit(Image* image, GPU_Rect* src_rect, Target* target, float x, float y);
	void BlitRotate(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees);
	void BlitScale(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float scaleX, float scaleY);
	void BlitTransform(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float degrees, float scaleX, float scaleY);
	void BlitTransformX(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, float pivot_x, float pivot_y, float degrees, float scaleX, float scaleY);
	void TriangleBatchX(Image* image, Target* target, unsigned short num_vertices, void* values, unsigned int num_indices, unsigned short* indices, BatchFlagEnum flags);
	void GenerateMipmaps(Image* image);
	GPU_Rect SetClip(Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h);
	void UnsetClip(Target* target);
	SDL_Color GetPixel(Target* target, Sint16 x, Sint16 y);
	void SetImageFilter(Image* image, FilterEnum filter);
	void SetWrapMode(Image* image, WrapEnum wrap_mode_x, WrapEnum wrap_mode_y);
	void ClearRGBA(Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
	void FlushBlitBuffer();
	void Flip(Target* target);
		
	Uint32 CreateShaderProgram();
	void FreeShaderProgram(Uint32 program_object);
	Uint32 CompileShader_RW(ShaderEnum shader_type, SDL_RWops* shader_source, bool free_rwops);
	Uint32 CompileShader(ShaderEnum shader_type, const char* shader_source);
	void FreeShader(Uint32 shader_object);
	void AttachShader(Uint32 program_object, Uint32 shader_object);
	void DetachShader(Uint32 program_object, Uint32 shader_object);
	bool LinkShaderProgram(Uint32 program_object);
	void ActivateShaderProgram(Uint32 program_object, ShaderBlock* block);
	void DeactivateShaderProgram();
	const char* GetShaderMessage();
	int GetAttributeLocation(Uint32 program_object, const char* attrib_name);
	int GetUniformLocation(Uint32 program_object, const char* uniform_name);
	ShaderBlock LoadShaderBlock(Uint32 program_object, const char* position_name, const char* texcoord_name, const char* color_name, const char* modelViewMatrix_name);	      
	void SetShaderImage(Image* image, int location, int image_unit);    
	void GetUniformiv(Uint32 program_object, int location, int* values);
	void SetUniformi(int location, int value);
	void SetUniformiv(int location, int num_elements_per_value, int num_values, int* values);
	void GetUniformuiv(Uint32 program_object, int location, unsigned int* values);
	void SetUniformui(int location, unsigned int value);
	void SetUniformuiv(int location, int num_elements_per_value, int num_values, unsigned int* values);
	void GetUniformfv(Uint32 program_object, int location, float* values);
	void SetUniformf(int location, float value);
	void SetUniformfv(int location, int num_elements_per_value, int num_values, float* values);
	void SetUniformMatrixfv(int location, int num_matrices, int num_rows, int num_columns, bool transpose, float* values);    
	void SetAttributef(int location, float value);    
	void SetAttributei(int location, int value);    
	void SetAttributeui(int location, unsigned int value);    
	void SetAttributefv(int location, int num_elements, float* value);    
	void SetAttributeiv(int location, int num_elements, int* value);    
	void SetAttributeuiv(int location, int num_elements, unsigned int* value);    
	void SetAttributeSource(int num_values, Attribute source);
    
    // Shapes        
	float SetLineThickness(float thickness);
	float GetLineThickness();
	void Pixel(Target* target, float x, float y, SDL_Color color);
	void Line(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);
	void Arc(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color);
	void ArcFilled(Target* target, float x, float y, float radius, float start_angle, float end_angle, SDL_Color color);
	void Circle(Target* target, float x, float y, float radius, SDL_Color color);
	void CircleFilled(Target* target, float x, float y, float radius, SDL_Color color);
	void Ellipse(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color);
	void EllipseFilled(Target* target, float x, float y, float rx, float ry, float degrees, SDL_Color color);
	void Sector(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color);
	void SectorFilled(Target* target, float x, float y, float inner_radius, float outer_radius, float start_angle, float end_angle, SDL_Color color);
	void Tri(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color);
	void TriFilled(Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color);
	void Rectangle(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);
	void RectangleFilled(Target* target, float x1, float y1, float x2, float y2, SDL_Color color);
	void RectangleRound(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color);
	void RectangleRoundFilled(Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color);
	void Polygon(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color);
	void PolygonFilled(Target* target, unsigned int num_vertices, float* vertices, SDL_Color color);
	bool GetImageData(Target* target, unsigned char* data, Sint16 x, Sint16 y, Sint16 w, Sint16 h);
	void BlitTransformA(Image* image, GPU_Rect* src_rect, Target* target, float x, float y, AffineTransform* transform, float scaleX, float scaleY);
	void PolygonTextureFilled(Target* target, unsigned int num_vertices, float* vertices, Image* image, float texture_x, float texture_y);
	void PolygonTextureFilledNPOT(Target* target, unsigned int num_vertices, float* vertices, Image* image);
	void TextureLine(Target* target, float x1, float y1, float x2, float y2, Image* image, float texture_x, float texture_y);
	void TextureLineNPOT(Target* target, float x1, float y1, float x2, float y2, Image* image);
	void BlitTransformAColor(Image* image, Target* target, float x, float y, AffineTransform* transform, GPU_Color colors[4]);
	void PolygonColorFilled(Target* target, unsigned int num_vertices, float* vertices, ColorCallback colorfunc, void* userdata);
	void ColorLine(Target* target, float x1, float y1, float x2, float y2, ColorCallback colorfunc, void* userdata);

	bool IsExtensionSupported(const char* extension_str);
private:
	void init_features();
	bool IsFeatureEnabled(FeatureEnum feature);
	void extBindFramebuffer(GLuint handle);;
	void bindTexture(Image* image);
	void flushAndBindTexture(GLuint handle);
	bool bindFramebuffer(Target* target);
	void flushAndBindFramebuffer(GLuint handle);
	void flushBlitBufferIfCurrentTexture(Image* image);
	void flushAndClearBlitBufferIfCurrentTexture(Image* image);
	bool isCurrentTarget(Target* target);
	void flushAndClearBlitBufferIfCurrentFramebuffer(Target* target);
	void makeContextCurrent(Target* target);
	void setClipRect(Target* target);
	void prepareToRenderToTarget(Target* target);
	void changeBlending(bool enable);
	void forceChangeBlendMode(BlendMode mode);
	void changeBlendMode(BlendMode mode);
	Uint32 get_proper_program_id(Uint32 program_object);
	void changeTexturing(bool enable);
	void enableTexturing();
	void disableTexturing();
	void prepareToRenderImage(Target* target, Image* image);
	void prepareToRenderShapes(unsigned int shape);
	GLuint CreateUninitializedTexture();
	Image* CreateUninitializedImage(Uint16 w, Uint16 h, FormatEnum format);

	bool readTargetPixels(Target* source, GLint format, GLubyte* pixels);
	bool readImagePixels(Image* source, GLint format, GLubyte* pixels);
	unsigned char* getRawTargetData(Target* target);
	unsigned char* getRawImageData(Image* image);

	int compareFormats(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult);
	SDL_PixelFormat* AllocFormat(GLenum glFormat);
	SDL_Surface* copySurfaceIfNeeded(GLenum glFormat, SDL_Surface* surface, GLenum* surfaceFormatResult);
	Image* gpu_copy_image_pixels_only(Image* image);
private:
	RenderDevice* _device;
};

NS_GPU_END

#endif
