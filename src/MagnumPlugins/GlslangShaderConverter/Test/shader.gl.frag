#version 330 core

#if defined(VALIDATE_NON_SPIRV) && defined(GL_SPIRV)
#error GL_SPIRV is defined but it should not be
#elif !defined(VALIDATE_NON_SPIRV) && !defined(GL_SPIRV)
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

/* We need the explicit location only if compiling for SPIR-V or validating for
   SPIR-V (which should have the exact same behavior) */
#ifndef VALIDATE_NON_SPIRV
#extension GL_ARB_explicit_uniform_location: require
layout(location=0)
#endif
uniform vec4 flatColor;

/* We need the explicit location only if compiling for SPIR-V or validating for
   SPIR-V (which should have the exact same behavior) */
#ifndef VALIDATE_NON_SPIRV
layout(location=0)
#endif
out vec4 color;

void main() {
    color = flatColor*gl_FragCoord;
}
