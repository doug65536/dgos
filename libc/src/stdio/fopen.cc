#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "bits/cfile.h"

///
///  "r"   read 
///     Open a file for reading
/// 	file already exists: read from start
///     file does not exist: failure to open
/// 
///  "w"   write
///     Create a file for writing
/// 	file already exists: destroy contents
/// 	file does not exist: create new
/// 
///  "a"   append        
///     Append to a file
/// 	file already exists: write to end
/// 	file does not exist: create new
/// 
///  "r+"  read extended
/// 	Open a file for read/write
/// 	file already exists: read from start	
///     file does not exist: error
/// 
///  "w+"  write extended
/// 	Create a file for read/write
/// 	file already exists: destroy contents
/// 	file does not exist: create new
/// 
///  "a+"  append extended
/// 	Open a file for read/write
/// 	file already exists: write to end
/// 	file does not exist: create new
/// 

FILE *fopen(const char * restrict filename, const char * restrict mode)
{
    int flags = 0;
    mode_t open_mode = 0;    // fixme
    
    for (size_t i = 0; mode[i]; ++i) {
        switch (mode[0]) {
        case 'r':
            flags = O_RDONLY | O_EXCL;
            break;
        case 'w':
            flags = O_CREAT;
            break;
        case 'a':
            flags = O_APPEND;
            break;
        case '+':
            flags &= ~O_RDONLY;
        }
    }
    
    //int fd = open(filename, flags)
    int fd = open(filename, flags, open_mode);
    _FILE *file = new _FILE(fd);
    return file;
}
