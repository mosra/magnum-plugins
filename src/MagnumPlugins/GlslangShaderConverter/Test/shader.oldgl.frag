/* #version is defined externally */

#ifdef GL_SPIRV
#error GL_SPIRV is defined but it should not be
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

/* No layout() can be enforced here because it's not a valid syntax */
varying vec4 color;
uniform float factor;

void main() {
    gl_FragColor = color*factor;
}
