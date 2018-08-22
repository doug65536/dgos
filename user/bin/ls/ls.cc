#include <sys/types.h>
#include <dirent.h>

int main(int argc, char const **argv)
{
    DIR *dir = opendir(".");
    if (!dir)
        return 1;
    struct dirent *de;
    while (de = readdir(dir)) {

    }
    closedir(dir);
    return 0;
}
