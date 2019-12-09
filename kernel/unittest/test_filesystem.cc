#include "unittest.h"
#include "fileio.h"

UNITTEST(test_filesystem_opendir)
{
    printk("Opening root directory\n");

    int od = file_opendir("");
    lt(0, od);
    dirent_t de;
    dirent_t *dep;
    while (file_readdir_r(od, &de, &dep) > 0) {
        printdbg("File: %s\n", de.d_name);
    }
    file_closedir(od);
}
