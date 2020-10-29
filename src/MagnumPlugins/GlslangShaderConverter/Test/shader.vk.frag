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

/* When validating Vulkan shaders, we set target to SPIR-V, which means
   input/output locations are required */
layout(location=0) out vec4 color;

void main() {
    color = gl_FragCoord*factor;
}
