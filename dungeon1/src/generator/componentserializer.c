#include "codegenerator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <path to toml input> <ouput directory>",
            argv[0]);
    return EXIT_FAILURE;
  }

  const char *inputPath = argv[1];
  const char *outputDir = argv[2];

  FILE *inputFile;
  if (fopen_s(&inputFile, inputPath, "r") != 0 || inputFile == NULL) {
    perror("Failed to open input TOML file");
    return EXIT_FAILURE;
  }

  fseek(inputFile, 0, SEEK_END);
  size_t fileSize = ftell(inputFile);
  fseek(inputFile, 0, SEEK_SET);

  if (fileSize <= 0) {
    puts("Input file is empty or error corred.");
    fclose(inputFile);
    return EXIT_FAILURE;
  }

  char *fileContent = (char *)malloc(fileSize + 1);
  if (!fileContent) {
    perror("Failed to allocate memory for the input file content");
    fclose(inputFile);
    return EXIT_FAILURE;
  }

  fread(fileContent, 1, fileSize, inputFile);
  fileContent[fileSize] = '\0';
  fclose(inputFile);

  size_t outputSize = 1024;
  char *outputHeader = (char *)malloc(outputSize);
  if (!outputHeader) {
    perror("Failed to allocate memory for the output header buffer");
    free(fileContent);
    return EXIT_FAILURE;
  }

  char *outputSource = (char *)malloc(outputSize);
  if (!outputSource) {
    perror("Failed to allocate memory for the output source buffer");
    free(fileContent);
    return EXIT_FAILURE;
  }



  if (generate_code_from_buffers(fileContent, outputHeader,outputSource, outputSize)) {
    char outputFilePath[1024];
    snprintf(outputFilePath, sizeof(outputFilePath), "%s/generated_code.c",
             outputDir);

    FILE *outputFile = NULL;
    if (fopen_s(&outputFile, outputFilePath, "w") != 0 || outputFile == NULL) {
      perror("Failed to create output file");
      free(fileContent);
      free(outputHeader);
      return EXIT_FAILURE;
    }

    fwrite(outputHeader, 1, strlen(outputHeader), outputFile);
    fclose(outputFile);

    printf("Code succesfully generated at: %s\n", outputFilePath);
  } else {
    perror("error generating the serializer code");
  }

  free(fileContent);
  free(outputHeader);

  return EXIT_SUCCESS;
}
