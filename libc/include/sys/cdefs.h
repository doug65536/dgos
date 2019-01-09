#pragma once

#ifdef	__cplusplus
#define restrict __restrict
#define __BEGIN_DECLS	extern "C" {
#define __END_DECLS	}

#define __BEGIN_NAMESPACE(n)   namespace n {
#define __BEGIN_NAMESPACE_STD   __BEGIN_NAMESPACE(std)
#define __BEGIN_NAMESPACE_EXT   __BEGIN_NAMESPACE(ext)
#define __END_NAMESPACE         }
#define __END_NAMESPACE_STD     __END_NAMESPACE
#else
#define __BEGIN_DECLS
#define __END_DECLS
#define __BEGIN_NAMESPACE(n)
#define __BEGIN_NAMESPACE_STD
#define __BEGIN_NAMESPACE_EXT
#define __END_NAMESPACE
#define __END_NAMESPACE_STD
#endif

#define _always_inline __attribute__((__always_inline__)) inline
