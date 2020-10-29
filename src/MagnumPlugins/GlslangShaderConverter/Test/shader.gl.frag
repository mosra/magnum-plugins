#version 330 core

#ifndef GL_SPIRV
#error GL_SPIRV is not defined but it should be
#endif

#ifdef VULKAN
#error VULKAN is defined but it should not be
#endif

#ifndef A_DEFINE
#error A_DEFINE is not defined but it should be
#endif

#ifdef AN_UNDEFINE
#error AN_UNDEFINE is defined but it should not be
#endif

/* Since it's GL we don't need an explicit location for validation, but we need
   it for SPIR-V compilation */
#ifdef NEED_LOCATION
layout(location=0)
#endif
out vec4 color;

void main() {
    color = gl_FragCoord;
}
