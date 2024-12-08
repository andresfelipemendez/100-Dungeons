#include "build.h"
#include <string.h>

#define PORT 8080

int watchMode = 0;
/*
 * default mode it's build everything and watch the src folders to rebuild the
 * dll when rebuilding set the shared memory to notify the engine to reload the
 * dll
 */

// id like to add the log functionality too
// .\build.exe --engine 2>&1 | Out-File -FilePath build.log -Encoding utf8

int main(int argc, char *argv[]) {
  if (argc < 2) {
    const char *usage =
        "usage: build.exe <flags>\nUse -h or --help for more information.";
    puts(usage);
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printHelp();
      return 0;
    } else if (strcmp(argv[i], "--watch") == 0) {
      watchMode = 1;
    } else if (strcmp(argv[i], "--engine") == 0) {
      build_engine_dll();
    } else {
      printf("Unknown flag: %s\n", argv[i]);
      return 1;
    }
  }

  // initializeSockets();

  if (watchMode) {
    printf("Watching mode enabled.\n");
    while (1) {
      thrd_t builderThread;
      thrd_create(&builderThread, startBuilderServer, NULL);
      thrd_join(builderThread, NULL);
    }
  } else {
    thrd_t builderThread;
    thrd_create(&builderThread, startBuilderServer, NULL);
    thrd_join(builderThread, NULL);
  }

  cleanupSockets();
  return 0;
}

void printHelp() {
  const char *help =
      "Usage: build.exe <flags>\n"
      "Flags:\n"
      "  -h, --help       Show this help message and exit\n"
      "  --watch          Enable watching mode for continuous builds\n";
  puts(help);
}

void initializeSockets() {
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

void cleanupSockets() {
#ifdef _WIN32
  WSACleanup();
#endif
}

int startBuilderServer(void *arg) {
  printf("Builder server is starting on port %d...\n", PORT);
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char *success_message = "BUILD_SUCCESS";

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket creation failed");
    return -1;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt))) {
    perror("Setsockopt failed");
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    perror("Listen failed");
    return -1;
  }

  printf("Builder server is waiting for a connection on port %d...\n", PORT);
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) <
      0) {
    perror("Accept failed");
    return -1;
  }
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);

  send(new_socket, success_message, strlen(success_message), 0);

#ifdef _WIN32
  closesocket(server_fd);
  closesocket(new_socket);
#else
  close(server_fd);
  close(new_socket);
#endif

  return 0;
}

void build_engine_dll() {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("can't get working directory path");
    return;
  }
  size_t cwd_len = strlen(cwd);
  printf("current working directory: %s\n", cwd);

  const char separator[] = "\\\\";
  size_t separator_len = LEN(separator);
  const char include[] = "-I";
  size_t include_len = LEN(include);
  const char *include_dirs[] = {
      INCLUDE_DIR,  CORE_DIR,     EXTERNALS_DIR, FASTGLTF_INCLUDE,
      GLM_INCLUDE,  GLAD_INCLUDE, IMGUI_INCLUDE, IMGUI_BACKENDS_INCLUDE,
      TOML_INCLUDE, GLFW_INCLUDE};

  size_t includes_len = 0;
  for (size_t i = 0; i < LEN(include_dirs); i++) {
    includes_len +=
        cwd_len + strlen(include_dirs[i]) + separator_len + include_len;
  }
  // printf("buffer size needed %zu\n", includes_len);
  char includes[includes_len];
  includes[0] = ' ';

  for (size_t i = 0; i < LEN(include_dirs); i++) {
    strcat_s(includes, includes_len, include);
    strcat_s(includes, includes_len, cwd);
    strcat_s(includes, includes_len, include_dirs[i]);
    strcat_s(includes, includes_len, " ");
  }

  const char lib_dir[] = "-L";
  size_t lib_dir_len = LEN(lib_dir);
  const char *lib_dirs[] = {FASTGLTF_LIB_DIR, TOML_LIB_DIR, GLAD_LIB_DIR,
                            GLFW_LIB_DIR, IMGUI_LIB_DIR};

  size_t lib_dirs_len = 0;
  for (size_t i = 0; i < LEN(lib_dirs); i++) {
    lib_dirs_len += lib_dir_len + cwd_len + strlen(lib_dirs[i]) +
                    separator_len + lib_dir_len;
  }

  char libs_dirs[lib_dirs_len];
  libs_dirs[0] = ' ';

  for (size_t i = 0; i < LEN(lib_dirs); i++) {
    strcat_s(libs_dirs, lib_dirs_len, lib_dir);
    strcat_s(libs_dirs, lib_dirs_len, cwd);
    strcat_s(libs_dirs, lib_dirs_len, lib_dirs[i]);
    strcat_s(libs_dirs, lib_dirs_len, " ");
  }

  const char lib[] = "-l";
  size_t lib_len = LEN(lib);
  const char *lib_names[] = {"fastgltf", "toml", "glad", "glfw3dll", "imgui"};

  size_t libs_len = 0;
  for (size_t i = 0; i < LEN(lib_names); i++) {
    libs_len += strlen(lib_names[i]) + separator_len + lib_dir_len;
  }
  // printf("buffer size needed %zu\n", includes_len);
  char libs[libs_len];
  libs[0] = '\0';

  for (size_t i = 0; i < LEN(lib_names); i++) {
    strcat_s(libs, libs_len, lib);
    strcat_s(libs, libs_len, lib_names[i]);
    strcat_s(libs, libs_len, " ");
  }

  char source_files[1024] = {0};
#ifdef _WIN32
  WIN32_FIND_DATA findFileData;
  HANDLE hFind;

  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s%s\\*.cpp", cwd, ENGINE_DIR);

  hFind = FindFirstFile(search_path, &findFileData);
  if (hFind == INVALID_HANDLE_VALUE) {
    printf("Unable to open directory: %s\n", ENGINE_DIR);
    return;
  }

  do {
    // Print file/directory names, skipping "." and ".."
    if (strcmp(findFileData.cFileName, ".") != 0 &&
        strcmp(findFileData.cFileName, "..") != 0) {
      strcat_s(source_files, sizeof(source_files), cwd);
      strcat_s(source_files, sizeof(source_files), ENGINE_DIR);
      strcat_s(source_files, sizeof(source_files), "\\");
      strcat_s(source_files, sizeof(source_files), findFileData.cFileName);
      strcat_s(source_files, sizeof(source_files), " ");
    }
  } while (FindNextFile(hFind, &findFileData) != 0);

  FindClose(hFind);
#else
  // :?
#endif

  const char out_flag[] = "-o ";
  size_t out_flag_len = LEN(out_flag);
  const char output_file_name[] = "\\engine.dll";
  size_t output_len = out_flag_len + cwd_len + LEN(OUTPUT_DIR) +
                      LEN(output_file_name) + NULL_TERMINATOR;
  char output[output_len];
  strcpy_s(output, output_len, out_flag);
  strcat_s(output, output_len, cwd);
  strcat_s(output, output_len, OUTPUT_DIR);
  strcat_s(output, output_len, output_file_name);

  const char compile_command[] =
      "clang++ -shared -std=c++17 -g -DITERATOR_DEBUG_LEVEL=0 -D_MT -D_DLL "
      "-DIMGUI_DEFINE_MATH_OPERATORS ";
  const char compile_options[] = "-fuse-ld=lld -Wl,/machine:x64";

  size_t command_len = LEN(compile_command) + LEN(includes) +
                       strlen(source_files) + LEN(libs_dirs) + LEN(libs) +
                       LEN(compile_options) + NULL_TERMINATOR;
  char command[command_len];
  strcpy_s(command, command_len, compile_command);
  strcat_s(command, command_len, includes);
  strcat_s(command, command_len, source_files);
  strcat_s(command, command_len, output);
  strcat_s(command, command_len, libs_dirs);
  strcat_s(command, command_len, libs);
  strcat_s(command, command_len, compile_options);

  puts(command);

  int res = system(command);
  
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
GetConsoleScreenBufferInfo(hConsole, &consoleInfo);  // Save current attributes

if (res != 0) {
    // Red background with white text for error
    SetConsoleTextAttribute(hConsole, BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    puts("compilation error");
} else {
    // Green text for success
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
    puts("compilation success");
}

// Restore original console colors
SetConsoleTextAttribute(hConsole, consoleInfo.wAttributes);
}
