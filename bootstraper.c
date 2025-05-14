#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

int main() {
	struct stat buff;
	time_t foundry_time = 0;
	time_t file_watch_time = 0;

	time_t f_temp = 0;
	time_t fw_temp = 0;
	do {
		stat("./forge/forge.c", &buff);
		f_temp = buff.st_mtime;
		stat("./lib/file_watch.c", &buff);
		fw_temp = buff.st_mtime; 
		if(foundry_time != f_temp || file_watch_time != fw_temp) {
			const char* compile_cmd = "gcc -o forge.bin forge/forge.c lib/file_watch.c";	
			int result = system(compile_cmd);
			if(result == 0) {
				system("./forge.bin");
			} else {
				puts("build failed");
			}
			foundry_time = f_temp;
			file_watch_time = fw_temp;
		}
	} while(1);	
}
