//-----------------------------------------------------------------------------
// Dear ImGui's OpenGL 3 backend, built for ANGLE on Windows. The backend makes a vertex array
// every frame and deletes it after drawing, but our ANGLE's D3D11 vertex arrays don't take back
// the callbacks they give their buffers when deleted: the next frame's glBufferData on ImGui's
// vertex buffer calls into the freed array (std::bad_function_call, or worse). So the backend
// gets one vertex array, kept for the app's single GL context; it sets every attribute each
// frame anyway.
//-----------------------------------------------------------------------------
#include <GLES3/gl3.h> // declares the real functions; the backend's include then does nothing

static GLuint keptVertexArray;

static void GenKeptVertexArray(GLsizei, GLuint *arrays) {
    if(keptVertexArray == 0) glGenVertexArrays(1, &keptVertexArray);
    *arrays = keptVertexArray;
}

static void KeepVertexArray(GLsizei, const GLuint *) {}

#define glGenVertexArrays GenKeptVertexArray
#define glDeleteVertexArrays KeepVertexArray
#include "backends/imgui_impl_opengl3.cpp"
