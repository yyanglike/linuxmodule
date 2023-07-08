#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    char buffer[BUFFER_SIZE];

    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    // 绑定套接字到指定端口
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // 监听连接请求
    if (listen(serverSocket, 5) == -1) {
        perror("Failed to listen for connections");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);

    // 接受连接
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientSocket == -1) {
        perror("Failed to accept connection");
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 接收数据并打印
    ssize_t bytesRead;
    while ((bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        printf("Received: %.*s\n", (int)bytesRead, buffer);
        memset(buffer, 0, sizeof(buffer));
    }

    if (bytesRead == -1) {
        perror("Failed to receive data");
        exit(EXIT_FAILURE);
    }

    // 关闭套接字
    close(clientSocket);
    close(serverSocket);

    return 0;
}
