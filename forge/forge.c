#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <ftw.h>
#include <string.h>

int display_info(const char *fpath, const struct stat *sb, int typeflag) {
	struct stat buff;
	if(stat(fpath, &buff) == 0) {
		char *time_str = ctime(&buff.st_mtime);
		time_str[strlen(time_str) - 1] = '\0';
		printf("%s ", time_str);
	}

    printf("%s", fpath);
    
    if (typeflag == FTW_D) {
        printf("/");
    }
    printf("\n");
    
    return 0;  // Continue walking
}

int main() {
    ftw(".", display_info, 20);
    return 0;
}
