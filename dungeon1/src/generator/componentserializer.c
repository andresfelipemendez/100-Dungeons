#include "codegenerator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  size_t outputSize = 1024 * 30;
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

  if (generate_code_from_buffers(fileContent, outputHeader, outputSource,
                                 outputSize)) {
    char outputSourceFilePath[1024 * 30];
    snprintf(outputSourceFilePath, sizeof(outputSourceFilePath),
             "%s\\componets.gen.cpp", outputDir);

    FILE *outputSourceFile = NULL;
    if (fopen_s(&outputSourceFile, outputSourceFilePath, "w") != 0 ||
        outputSourceFile == NULL) {
      perror("Failed to create output source file");
      free(fileContent);
      free(outputHeader);
      return EXIT_FAILURE;
    }
    fwrite(outputSource, 1, strlen(outputSource), outputSourceFile);
    fclose(outputSourceFile);

    char outputHeaderFilePath[1024 * 30];
    snprintf(outputHeaderFilePath, sizeof(outputHeaderFilePath),
             "%s\\components.gen.h", outputDir);
    FILE *outputHeaderFile = NULL;
    if (fopen_s(&outputHeaderFile, outputHeaderFilePath, "w") != 0 ||
        outputSourceFile == NULL) {
      perror("Failed to create output source file");
      free(fileContent);
      free(outputHeader);
      return EXIT_FAILURE;
    }
    fwrite(outputHeader, 1, strlen(outputHeader), outputHeaderFile);
    fclose(outputHeaderFile);

    printf("Code succesfully generated at: %s\n", outputSourceFilePath);
  } else {
    perror("error generating the serializer code");
  }

  free(fileContent);
  free(outputHeader);

  return EXIT_SUCCESS;
}
