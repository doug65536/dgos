#include <stdlib.h>

static int download_temp(char *name_template,
                         char *url)
{
    char const *download_command = "curl '%s' > %s";

    mkstemp(name_template);

    int size = snprintf(0, 0, download_command,
                        url, name_template);

    char *command_str = malloc(size+1);
    snprintf(command_str, size+1, download_command,
             url, name_template);

    int exitCode = system(command_str);

    if (exitCode)
        return exitCode;
}

int main(int argc, char **argv)
{
    char hw_map_name[] = "hwmapXXXXXX";
    mkstemp(hw_map_name);



    system("curl http://www.unicode.org/"
           "repos/cldr/tags/latest/"
           "keyboards/windows/_platform.xml > ");
}

