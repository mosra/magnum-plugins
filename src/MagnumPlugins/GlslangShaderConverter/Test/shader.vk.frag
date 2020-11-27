#version 440 core

#ifndef VULKAN
#error VULKAN is not defined but it should be
#endif

#ifdef GL_SPIRV
#error GL_SPIRV is defined but it should not be
#endif

#ifndef A_DEFINE
#error A_DEFINE is not defined but it should be
#endif

#ifdef AN_UNDEFINE
#error AN_UNDEFINE is defined but it should not be
#endif

layout(push_constant) uniform Thing {
    float factor;
};

/* Uniform and output locations are always required, and omitting either should
   cause an error in all scenarios. */
#ifndef NO_EXPLICIT_BINDING
layout(binding=0)
#endif
uniform sampler2D textureData;
#ifndef NO_EXPLICIT_LOCATION
layout(location=0)
#endif
out vec4 color;

void main() {
    color = factor*texture(textureData, vec2(0.0))*gl_FragCoord;
}
