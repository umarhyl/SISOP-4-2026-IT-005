#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUFFER_SIZE 2048

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char command[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    printf("[+] Connected to Mini Database Server at port 9000\n");

    while (1) {
        printf("MOO_DB> ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) break;
        
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) break;

        send(sock, command, strlen(command), 0);
        
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        } else {
            printf("[-] Server disconnected\n");
            break;
        }
    }

    close(sock);
    return 0;
}