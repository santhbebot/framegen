#ifndef MOCK_DLFCN_H
#define MOCK_DLFCN_H

#include <stddef.h>

#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL 0

#ifdef __cplusplus
extern "C" {
#endif

void* dlopen(const char* filename, int flags);
void* dlsym(void* handle, const char* symbol);
int dlclose(void* handle);
const char* dlerror(void);

#ifdef __cplusplus
}
#endif

#endif // MOCK_DLFCN_H
