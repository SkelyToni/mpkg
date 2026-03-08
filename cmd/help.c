#include <stdio.h>

int help()
{
    printf("mpkg - a small nix-like package manager");
    printf("Usage: \n");
    printf("mpkg init\n");
    printf("mpkg install <path-to-package-declaration>\n");
    printf("mpkg remove <package-name>\n");
    printf("mpkg list\n");
    printf("mpkg gc\n");
    printf("mpkg help\n");
    return 0;
}