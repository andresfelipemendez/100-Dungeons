#include "file_watch.h"
#include <stdio.h>
#include <threads.h>
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


int list_dir(void*) {
	ftw(".",display_info, 20);
	return 0;
}

bool watch () {
	puts("calling thread");
	thrd_t t0;
	if(thrd_create(&t0,list_dir,NULL)==-1) {
		perror("cant create thread");
	}
	int res = 0;
	if(thrd_join(t0, &res) == -1){
		perror("can't join thread t0");
	}
	return false;
}
