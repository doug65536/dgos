#pragma once

//#define PROTECTED __attribute__((__visibility__("protected")))
//#define EXPORT __attribute__((__visibility__("default")))
#define HIDDEN __attribute__((__visibility__("hidden")))

#if defined(__DGOS_KERNEL__) && !defined(__DGOS_MODULE__)
#define KERNEL_API __attribute__((__visibility__("protected")))
#else
#define KERNEL_API
#endif

#define EXPORT __attribute__((__visibility__("default")))
