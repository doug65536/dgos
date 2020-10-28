#pragma once

//#define PROTECTED __attribute__((__visibility__("protected")))
//#define EXPORT __attribute__((__visibility__("default")))
#define HIDDEN __attribute__((__visibility__("hidden")))

// Compiler thinks it knows better, let it be
#define KERNEL_API_BUILTIN __attribute__((__visibility__("default")))

#if defined(__DGOS_KERNEL__) && !defined(__DGOS_MODULE__)
#define KERNEL_API __attribute__((__visibility__("protected")))
#define __BEGIN_KERNEL_API _Pragma("GCC visibility push(protected)")
#define __END_KERNEL_API _Pragma("GCC visibility pop")
#else
#define __BEGIN_KERNEL_API
#define __END_KERNEL_API
#define KERNEL_API
#endif

#define EXPORT __attribute__((__visibility__("default")))
