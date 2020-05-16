#define DEFAULT_TEXTURED_VERTEX_SHADER_SOURCE \
"#version 100\n\
precision highp float;\n\
precision mediump int;\n\
attribute vec2 gpu_Vertex;\n\
attribute vec2 gpu_TexCoord;\n\
attribute mediump vec4 gpu_Color;\n\
uniform mat4 gpu_ModelViewProjectionMatrix;\n\
varying mediump vec4 color;\n\
varying vec2 texCoord;\n\
void main(void)\n\
{\n\
	color = gpu_Color;\n\
	texCoord = vec2(gpu_TexCoord);\n\
	gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0);\n\
}"

// Tier 3 uses shader attributes to send position, texcoord, and color data for each vertex.
#define DEFAULT_UNTEXTURED_VERTEX_SHADER_SOURCE \
"#version 100\n\
precision highp float;\n\
precision mediump int;\n\
attribute vec2 gpu_Vertex;\n\
attribute mediump vec4 gpu_Color;\n\
uniform mat4 gpu_ModelViewProjectionMatrix;\n\
varying mediump vec4 color;\n\
void main(void)\n\
{\n\
	color = gpu_Color;\n\
	gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0);\n\
}"


#define DEFAULT_TEXTURED_FRAGMENT_SHADER_SOURCE \
"#version 100\n\
#ifdef GL_FRAGMENT_PRECISION_HIGH\n\
precision highp float;\n\
#else\n\
precision mediump float;\n\
#endif\n\
precision mediump int;\n\
varying mediump vec4 color;\n\
varying vec2 texCoord;\n\
uniform sampler2D tex;\n\
void main(void)\n\
{\n\
    gl_FragColor = texture2D(tex, texCoord) * color;\n\
}"

#define DEFAULT_UNTEXTURED_FRAGMENT_SHADER_SOURCE \
"#version 100\n\
#ifdef GL_FRAGMENT_PRECISION_HIGH\n\
precision highp float;\n\
#else\n\
precision mediump float;\n\
#endif\n\
precision mediump int;\n\
varying mediump vec4 color;\n\
void main(void)\n\
{\n\
    gl_FragColor = color;\n\
}"
