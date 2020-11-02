#version 330 core

/* Verify that glslang correctly exposes the extension macro as well */
#ifdef GL_GOOGLE_include_directive
#extension GL_GOOGLE_include_directive: require

#include "sub/directory/basics.glsl"

#ifdef MAKE_THIS_BROKEN
#include "../notfound.glsl"
#endif
#else
#error GL_GOOGLE_include_directive not present
#endif

void main() {
    gl_Position = fullScreenTriangle();
}
