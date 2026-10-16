#ifndef MOCK_EGLEXT_H
#define MOCK_EGLEXT_H

#include <EGL/egl.h>

#define EGL_ANDROID_presentation_time 1
#define EGL_PRESENTATION_TIME_ANDROID 0x3429

typedef void (*PFNEGLPRESENTATIONTIMEANDROID)(EGLDisplay, EGLSurface, int64_t);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // MOCK_EGLEXT_H
