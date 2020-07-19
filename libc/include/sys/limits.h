#pragma once

// Maximum number of open files
#define NR_OPEN	        1024

// Number of unique group identifiers
#define NGROUPS_MAX    65536

// Maximum size of combined arguments and environment
#define ARG_MAX       131072

// Maximum number of file links
#define LINK_MAX         127

// Size of input queue
#define MAX_CANON        255

// Size of typeahead buffer
#define MAX_INPUT        255

// Maximum number of characters in a filename
#define NAME_MAX         255

// Maximum number of bytes you can use in one path parameter
#define PATH_MAX        4096

// Maximum pipe write that is still atomic
#define PIPE_BUF        4096

// Maximum number of characters in an extended attribute name
#define XATTR_NAME_MAX   255

// Maximum size of an extended attribute value in bytes
#define XATTR_SIZE_MAX 65536

// Maximum size of an extended attribute name list
#define XATTR_LIST_MAX 65536

