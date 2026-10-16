#ifndef MOCK_EGL_H
#define MOCK_EGL_H

#include <stdint.h>

typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef void* EGLClientBuffer;
typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void* EGLClientBuffer;

#define EGL_DEFAULT_DISPLAY ((EGLDisplay)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_SUCCESS 0x3000
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056

// Function pointer types
typedef EGLBoolean (*PFNEGLSWAPBUFFERS)(EGLDisplay, EGLSurface);
typedef EGLBoolean (*PFNEGLQUERYSURFACE)(EGLDisplay, EGLSurface, EGLint, EGLint*);

#ifdef __cplusplus
extern "C" {
#endif

EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface);
EGLBoolean eglQuerySurface(EGLDisplay, EGLSurface, EGLint, EGLint*);
EGLDisplay eglGetCurrentContext();

#ifdef __cplusplus
}
#endif

#endif // MOCK_EGL_H
