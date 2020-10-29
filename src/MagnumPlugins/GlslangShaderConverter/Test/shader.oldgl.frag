/* #version is defined externally */

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

varying vec4 color;

void main() {
    gl_FragColor = color;
}
