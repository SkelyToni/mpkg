#include "mpkg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        return help();
    }

    if(strcmp(argv[1], "help") == 0)
    {
        return help();
    }
    else if(strcmp(argv[1], "init") == 0)
    {
        if(argc == 2)
        {
            return init(NULL);
        }
        else if (argc == 3)
        {
            return init(argv[2]);
        } 
    }
    else if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mpkg install <file.mpkg>\n");
            return 1;
        }
        return install(argv[2]);
}
    
}

