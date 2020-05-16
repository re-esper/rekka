#include "extra.h"


bool IsExtensionSupported(const char* extension_str);

NS_REK_BEGIN

const char* shader_vert = R"(
attribute vec2 gpu_Vertex;
attribute vec2 gpu_TexCoord;
attribute vec4 gpu_Color;
uniform mat4 gpu_ModelViewProjectionMatrix;
varying vec4 color;
varying vec2 texCoord;
void main(void)
{
	color = gpu_Color;
	texCoord = vec2(gpu_TexCoord);
	gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0);
}
)";

const char* blend_shader_frag = R"(
varying vec4 color;
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec4 blendColor;
vec4 source_atop(in vec4 src, in vec4 dst) {
	return vec4(src.rgb * src.a * dst.a + dst.rgb * (1.0 - src.a) * dst.a, dst.a);
}
vec4 destination_in(in vec4 src, in vec4 dst) {
	return vec4(dst.rgb * src.a * dst.a, src.a * dst.a);
}
void main(void)
{	
    vec4 dst = texture2D(tex, texCoord);
	vec4 clr = source_atop(blendColor, dst);	
    gl_FragColor = destination_in(dst, clr);		
}
)";

const char* tone_blend_shader_frag = R"(
varying vec4 color;
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec4 blendColor;
uniform vec4 toneColor;
vec4 source_atop(in vec4 src, in vec4 dst) {
	return vec4(src.rgb * src.a * dst.a + dst.rgb * (1.0 - src.a) * dst.a, dst.a);
}
vec4 destination_in(in vec4 src, in vec4 dst) {
	return vec4(dst.rgb * src.a * dst.a, src.a * dst.a);
}
vec3 lighten(in vec3 src, in vec3 dst) {
    return max(src, dst);
}
vec3 difference(in vec3 src, in vec3 dst) {
    return abs(dst - src);   
}
void main(void)
{	
    vec4 dst = texture2D(tex, texCoord);
	vec4 clr = dst;
	vec3 tone1 = max(vec3(0.0, 0.0, 0.0), toneColor.xyz);
	clr.rgb = lighten(tone1, clr.rgb);
	clr.rgb = difference(vec3(1.0, 1.0, 1.0), clr.rgb);
	vec3 tone2 = max(vec3(0.0, 0.0, 0.0), -toneColor.xyz);
	clr.rgb = lighten(tone2, clr.rgb);
	clr.rgb = difference(vec3(1.0, 1.0, 1.0), clr.rgb);	
	clr = source_atop(blendColor, clr);
    gl_FragColor = destination_in(dst, clr);
}
)";

const char* gray_tone_blend_shader_frag = R"(
varying vec4 color;
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec4 blendColor;
uniform vec4 toneColor;
vec3 rgb2hsl(in vec3 c) {
	const float epsilon = 0.00000001;
	float cmin = min(c.r, min(c.g, c.b));
	float cmax = max(c.r, max(c.g, c.b));
	float cd = cmax - cmin;
	vec3 hsl = vec3(0.0);
	hsl.z = (cmax + cmin) / 2.0;
	hsl.y = mix(cd / (cmax + cmin + epsilon), cd / (epsilon + 2.0 - (cmax + cmin)), step(0.5, hsl.z));
	vec3 a = vec3(1.0 - step(epsilon, abs(cmax - c)));
	a = mix(vec3(a.x, 0.0, a.z), a, step(0.5, 2.0 - a.x - a.y));
	a = mix(vec3(a.x, a.y, 0.0), a, step(0.5, 2.0 - a.x - a.z));
	a = mix(vec3(a.x, a.y, 0.0), a, step(0.5, 2.0 - a.y - a.z));
	hsl.x = dot(vec3(0.0, 2.0, 4.0) + ((c.gbr - c.brg) / (epsilon + cd)), a);
	hsl.x = (hsl.x + (1.0 - step(0.0, hsl.x)) * 6.0) / 6.0;
	return hsl;
}
vec3 hsl2rgb(in vec3 c) {
	vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
	return c.z + c.y * (rgb - 0.5)*(1.0 - abs(2.0*c.z - 1.0));
}
vec3 saturation(in vec3 src, in vec3 dst) {
    vec3 dst_hsl = rgb2hsl(dst);
    vec3 src_hsl = rgb2hsl(src);
    return hsl2rgb(vec3(dst_hsl.r, src_hsl.g, dst_hsl.b));
}
vec4 source_atop(in vec4 src, in vec4 dst) {
	return vec4(src.rgb * src.a * dst.a + dst.rgb * (1.0 - src.a) * dst.a, dst.a);
}
vec4 destination_in(in vec4 src, in vec4 dst) {
	return vec4(dst.rgb * src.a * dst.a, src.a * dst.a);
}
vec3 lighten(in vec3 src, in vec3 dst) {
    return max(src, dst);
}
vec3 difference(in vec3 src, in vec3 dst) {
    return abs(dst - src);   
}
void main(void)
{	
    vec4 dst = texture2D(tex, texCoord);
	vec4 gray = vec4(1.0, 1.0, 1.0, toneColor.a);
	vec3 clr = saturation(gray.rgb * dst.a, dst.rgb * gray.a) + dst.rgb * (1.0 - gray.a) + gray.rgb * (1.0 - dst.a);
	vec3 tone1 = max(vec3(0.0, 0.0, 0.0), toneColor.xyz);
	clr = lighten(tone1, clr);
	clr = difference(vec3(1.0, 1.0, 1.0), clr);
	vec3 tone2 = max(vec3(0.0, 0.0, 0.0), -toneColor.xyz);
	clr = lighten(tone2, clr);
	clr = difference(vec3(1.0, 1.0, 1.0), clr);
	vec4 clr4 = source_atop(blendColor, vec4(clr, 1.0));
    gl_FragColor = destination_in(dst, clr4);
}
)";

ShaderData _shaderBlend;
ShaderData _shaderToneBlend;
ShaderData _shaderGrayToneBlend;
void CanvasExtra::tintImage(gpu::Image * image, gpu::Target* target, float x, float y, float w, float h, GPU_Color blendColor, GPU_Color toneColor)
{		
	float bcolor[4] = { blendColor.r, blendColor.g, blendColor.b, blendColor.a };
	float tcolor[4] = { toneColor.r, toneColor.g, toneColor.b, toneColor.a };
	gpu::SetBlending(image, false);
	if (toneColor.a > 0) {
		if (_shaderGrayToneBlend.program) {
			gpu::ActivateShaderProgram(_shaderGrayToneBlend.program, &_shaderGrayToneBlend.block);
			gpu::SetUniformfv(_shaderGrayToneBlend.locations[0], 4, 1, bcolor);
			gpu::SetUniformfv(_shaderGrayToneBlend.locations[1], 4, 1, tcolor);
		}
	}
	else if (toneColor.r != 0 || toneColor.g != 0 || toneColor.b != 0) {
		if (_shaderToneBlend.program) {
			gpu::ActivateShaderProgram(_shaderToneBlend.program, &_shaderToneBlend.block);
			gpu::SetUniformfv(_shaderToneBlend.locations[0], 4, 1, bcolor);
			gpu::SetUniformfv(_shaderToneBlend.locations[1], 4, 1, tcolor);
		}
	}
	else {
		if (_shaderBlend.program) {
			gpu::ActivateShaderProgram(_shaderBlend.program, &_shaderBlend.block);
			gpu::SetUniformfv(_shaderBlend.locations[0], 4, 1, bcolor);
		}
	}
	GPU_Rect src_rect = { x, y, w, h };
	gpu::Blit(image, &src_rect, target, 0, 0);
	gpu::SetBlending(image, true);
	gpu::DeactivateShaderProgram();
}


const char* texture_repeat_shader_frag = R"(
precision highp float;
varying vec4 color;
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 uvScale;
uniform vec2 uvOffset;
void main(void)
{
    gl_FragColor = texture2D(tex, fract(texCoord * uvScale + uvOffset)) * color;	
}
)";
ShaderData _shaderTextureRepeat;
void CanvasExtra::beginNPOTRepeat(gpu::Target * target, gpu::Image * image, float texture_x, float texture_y)
{
	uint16_t frame_w = target->w;
	uint16_t frame_h = target->h;
	uint16_t tex_w = image->texture_w;
	uint16_t tex_h = image->texture_h;
	float uvScale[2] = { (float)frame_w / tex_w, (float)frame_h / tex_h };
	float uvOffset[2] = { (0 - texture_x) / tex_w, (0 - texture_y) / tex_h };
	gpu::ActivateShaderProgram(_shaderTextureRepeat.program, &_shaderTextureRepeat.block);
	gpu::SetUniformfv(_shaderTextureRepeat.locations[0], 2, 1, uvScale);
	gpu::SetUniformfv(_shaderTextureRepeat.locations[1], 2, 1, uvOffset);
}

void CanvasExtra::endNPOTRepeat()
{
	gpu::DeactivateShaderProgram();
}

bool CanvasExtra::supportNPOTRepeat = true;
void CanvasExtra::initialize()
{	
#ifdef XGPU_DISABLE_OPENGL // OpenGL ES
	supportNPOTRepeat = gpu::IsExtensionSupported("GL_OES_texture_npot") || gpu::IsExtensionSupported("GL_IMG_texture_npot");
#endif
	uint32_t v = gpu::CompileShader(gpu::VERTEX_SHADER, shader_vert);
	if (v == 0) return;

	uint32_t blend_f = gpu::CompileShader(gpu::FRAGMENT_SHADER, blend_shader_frag);
	if (blend_f) {
		uint32_t p = gpu::CreateShaderProgram();
		gpu::AttachShader(p, v);
		gpu::AttachShader(p, blend_f);
		gpu::LinkShaderProgram(p);
		_shaderBlend.program = p;
		_shaderBlend.block = gpu::LoadShaderBlock(p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		_shaderBlend.locations[0] = gpu::GetUniformLocation(p, "blendColor");
	}

	uint32_t toneblend_f = gpu::CompileShader(gpu::FRAGMENT_SHADER, tone_blend_shader_frag);
	if (toneblend_f) {
		uint32_t p = gpu::CreateShaderProgram();
		gpu::AttachShader(p, v);
		gpu::AttachShader(p, toneblend_f);
		gpu::LinkShaderProgram(p);
		_shaderToneBlend.program = p;
		_shaderToneBlend.block = gpu::LoadShaderBlock(p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		_shaderToneBlend.locations[0] = gpu::GetUniformLocation(p, "blendColor");
		_shaderToneBlend.locations[1] = gpu::GetUniformLocation(p, "toneColor");
	}

	uint32_t graytoneblend_f = gpu::CompileShader(gpu::FRAGMENT_SHADER, gray_tone_blend_shader_frag);
	if (graytoneblend_f) {
		uint32_t p = gpu::CreateShaderProgram();
		gpu::AttachShader(p, v);
		gpu::AttachShader(p, graytoneblend_f);
		gpu::LinkShaderProgram(p);
		_shaderGrayToneBlend.program = p;
		_shaderGrayToneBlend.block = gpu::LoadShaderBlock(p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		_shaderGrayToneBlend.locations[0] = gpu::GetUniformLocation(p, "blendColor");
		_shaderGrayToneBlend.locations[1] = gpu::GetUniformLocation(p, "toneColor");
	}

	if (!supportNPOTRepeat) {
		uint32_t texturerepeat_f = gpu::CompileShader(gpu::FRAGMENT_SHADER, texture_repeat_shader_frag);
		if (texturerepeat_f) {
			uint32_t p = gpu::CreateShaderProgram();
			gpu::AttachShader(p, v);
			gpu::AttachShader(p, texturerepeat_f);
			gpu::LinkShaderProgram(p);
			_shaderTextureRepeat.program = p;
			_shaderTextureRepeat.block = gpu::LoadShaderBlock(p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
			_shaderTextureRepeat.locations[0] = gpu::GetUniformLocation(p, "uvScale");
			_shaderTextureRepeat.locations[1] = gpu::GetUniformLocation(p, "uvOffset");
		}
	}
}
NS_REK_END