#include <stdio.h>
#include <stdlib.h>


enum parsingState {
	start,
	structKeyword,
	componentName,
	startFields,
	filedType,
	fieldName,
	endFiledDefinition,
	endComponentDefinition,
};

int main() {
	FILE *componentsFile = NULL;
	fopen_s(&componentsFile, "..\\..\\src\\generator\\components.h", "r");
	if (!componentsFile) {
		fopen_s(&componentsFile, "src\\generator\\components.h", "r");
	}

	if (!componentsFile) {
		perror("couldn't find the component.h file\n");
		return EXIT_FAILURE;
	}
	enum parsingState state = structKeyword;
	int c;
	while ((c = fgetc(componentsFile)) != EOF) {
		switch (state) {
		case start:
			break;
		case structKeyword: {
			char structKeyword[] = "struct";
			static int i = 0;
			if (c == structKeyword[i]) {
                ++i;
			} else if (c ==' ' && i == 6) {
                puts("finished the struct keyword, component name is:");
                i = 0;
                state = componentName;
            }
			break;
		}
		case componentName:
        
		    putchar(c);
            if(c == ' ') {
                putchar('\n');
                state = startFields;
            }
			break;
		case startFields:
        puts("the fields are:");
			break;
		case filedType:
			break;
		case fieldName:
			break;
		case endFiledDefinition:
			break;
		case endComponentDefinition:
			break;
		}
	}

	if (ferror(componentsFile)) {
		puts("I/O error when reading");
	} else if (feof(componentsFile)) {
		puts("End of file is reached succesfully");
	}
	fclose(componentsFile);
	return EXIT_SUCCESS;
}