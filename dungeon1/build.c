#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h> // For inet_pton
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
#include <threads.h>

#define PORT 8080

void initializeSockets();
void cleanupSockets();
int startBuilderServer(void *arg);

int main() {
  initializeSockets();

  thrd_t builderThread;
  thrd_create(&builderThread, startBuilderServer, NULL);
  thrd_join(builderThread, NULL);

  cleanupSockets();
  return 0;
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
