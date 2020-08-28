#include "unittest.h"
#include "fileio.h"
#include "vector.h"
#include "printk.h"
#include "rand.h"

UNITTEST(test_filesystem_opendir)
{
    printk("Opening root directory\n");

    int od = file_opendirat(AT_FDCWD, "/");
    lt(0, od);
    dirent_t de;
    dirent_t *dep;
    while (file_readdir_r(od, &de, &dep) > 0) {
        printdbg("File: %s\n", de.d_name);
    }
    file_closedir(od);
}

// Disabled because this now runs too early for a writable filesystem
DISABLED_UNITTEST(test_filesystem_create_unlink_churn)
{
    rand_lfs113_t r;
    ext::vector<int> names(1024, -1);
    for (ssize_t i = 0; i < 1048576 + 1024; ++i) {
        char name[16];
        ssize_t d;

        if (size_t(i) < names.size()) {
            d = -1;
        } else if (i >= 1048576) {
            d = names[i - 1048576];
        } else {
            size_t i = r.lfsr113_rand() & 1023;
            d = names[i];
            names.erase(names.begin() + d);
            bool push_ok = names.push_back(i);
            eq(true, push_ok);
        }

        if (d >= 0) {
            snprintf(name, sizeof(name), "test%07zd", d);
            int unlink_result = file_unlinkat(AT_FDCWD, name);
            eq(0, unlink_result);
        }

        if (i < 1048576) {
            ssize_t name_len = snprintf(name, sizeof(name), "test%07zd", i);
            eq(11, name_len);

            int fileid = file_creatat(AT_FDCWD, name, 644);
            le(0, fileid);

            ssize_t write_result = file_write(fileid, name, name_len);
            eq(name_len, write_result);

            int close_result = file_close(fileid);
            eq(0, close_result);
        }
    }
}
