#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <ftw.h>
#include <string.h>
#include "../lib/file_watch.h"
//#include "../lib/compile.h"

int main() {
	watch();
    //ftw(".", display_info, 20);
    return 0;
}
