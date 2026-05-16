#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[4096] = {0};
    char message[1024];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to DB Server on port 9000\n");
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n");

    while (1) {
        printf("\ndb > ");
        fgets(message, 1024, stdin);
        
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "EXIT") == 0) {
            break;
        }

        send(sock, message, strlen(message), 0);
        
        memset(buffer, 0, sizeof(buffer));
        int valread = read(sock, buffer, 4096);
        if (valread > 0) {
            printf("%s\n", buffer);
        }
    }
    
    close(sock);
    return 0;
}