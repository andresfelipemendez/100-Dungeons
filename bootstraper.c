#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

int main() {
	struct stat buff;
	time_t foundry_time = 0;
	puts("begin watch");
	do {
		stat("./forge/forge.c", &buff);
		if(foundry_time != buff.st_mtime) {
			const char* compile_cmd = "gcc -o forge.bin forge/forge.c lib/file_watch.c";	
			int result = system(compile_cmd);
			if(result == 0) {
				puts("build succesful!");
			} else {
				puts("build failed");
			}
			foundry_time = buff.st_mtime;
		}
	} while(1);	
}
