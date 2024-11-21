#include <stdlib.h>
#include <stdio.h>

int main(){
    FILE* componentsFile = NULL;
    fopen_s(&componentsFile, "..\\..\\src\\generator\\components.h", "r");
    if(!componentsFile) {
        fopen_s(&componentsFile, "src\\generator\\components.h", "r");
    }

    if(!componentsFile) {
        perror("couldn't find the component.h file\n");
        return EXIT_FAILURE;
    }
    
    int c;
    while((c=fgetc(componentsFile))!=EOF){
        putchar(c);
    }
    
    if(ferror(componentsFile)){
        puts("I/O error when reading");
    }else if(feof(componentsFile)){
        puts("End of file is reached succesfully");
    }
    fclose(componentsFile);
    return EXIT_SUCCESS;
}