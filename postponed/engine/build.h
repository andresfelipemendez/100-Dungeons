#ifndef BUILD_H
#define BUILD_H

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h> // For inet_pton
#define getcwd _getcwd
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _CRT_DECLARE_NONSTDC_NAMES 1
#include <threads.h>

void initializeSockets();
void cleanupSockets();
int startBuilderServer(void *arg);
void printHelp();
void build_engine_dll();

#define LEN(array) (sizeof(array) / sizeof((array[0])))

#define NULL_TERMINATOR 1;

const char OUTPUT_DIR[] = "\\build\\Debug";

const char INCLUDE_DIR[] = "\\include";
const char CORE_DIR[] = "\\src\\core";
const char EXTERNALS_DIR[] = "\\src\\externals";
const char ENGINE_DIR[] = "\\src\\engine";

/// # Libs
// ## Fast GLTF
const char FASTGLTF_INCLUDE[] = "\\lib\\fastgltf\\include";
const char FASTGLTF_LIB_DIR[] = "\\lib\\fastgltf\\lib";

// ## GLAD
const char GLAD_INCLUDE[] = "\\lib\\glad";
const char GLAD_LIB_DIR[] = "\\lib\\glad";

// ## GLM
const char GLM_INCLUDE[] = "\\lib\\glm";

const char IMGUI_INCLUDE[] = "\\lib\\imgui-1.90.9";
const char IMGUI_BACKENDS_INCLUDE[] = "\\lib\\imgui-1.90.9\\backends";
const char IMGUI_LIB_DIR[] = "\\lib\\imgui-1.90.9";

// ## GLAD
const char TOML_INCLUDE[] = "\\lib\\toml";
const char TOML_LIB_DIR[] = "\\lib\\toml";

// ## GLAD
const char GLFW_INCLUDE[] = "\\lib\\glfw\\include";
const char GLFW_LIB_DIR[] = "\\lib\\glfw\\lib";

#endif // BUILD_H
