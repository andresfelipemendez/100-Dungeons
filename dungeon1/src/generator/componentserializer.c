#include "codegenerator.h"
#include <stdio.h>
#include <stdlib.h>

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

  fseek(componentsFile, 0, SEEK_END);
  long fileSize = ftell(componentsFile);
  fseek(componentsFile, 0, SEEK_END);

  if (fileSize <= 0) {
    puts("FIle is empty or error ocurred.");
    fclose(componentsFile);
    return EXIT_FAILURE;
  }

  char *fileContent = (char *)malloc(fileSize + 1);
  if (!fileContent) {
    perror("failed to allocate memory for the file content");
    fclose(componentsFile);
    return EXIT_FAILURE;
  }

  fread(fileContent, 1, fileSize, componentsFile);
  fileContent[fileSize] = '\0';
  fclose(componentsFile);

  size_t outputSize = 1024;
  char *outputBuffer = (char *)malloc(outputSize);
  if (!outputBuffer) {
    perror("Failed to allocate memory for the outputBuffer");
    free(fileContent);
    return EXIT_FAILURE;
  }

  if (generate_code_from_buffers(fileContent, outputBuffer, outputSize)) {
    puts("Output generated");
  } else {
    perror("error generating the serializer code");
  }

  free(fileContent);
  free(outputBuffer);

  return EXIT_SUCCESS;
}
