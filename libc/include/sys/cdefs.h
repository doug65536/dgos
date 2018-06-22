#pragma once

#ifdef	__cplusplus
# define restrict
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

#define _always_inline __attribute__((__always_inline__)) inline
