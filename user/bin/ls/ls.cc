#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

int main(int argc, char const **argv)
{
    DIR *dir = opendir(".");
    if (!dir)
        return 1;
    struct dirent *de;
    while (de = readdir(dir)) {
        printf("%s\n", de->d_name);
    }
    closedir(dir);
    return 0;
}
