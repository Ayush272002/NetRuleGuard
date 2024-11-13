#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 10000

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <serverHost> <serverPort> <command> [<args>...]\n", argv[0]);
        exit(1);
    }

    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    char *hostname = argv[1];
    portno = atoi(argv[2]);
    if (portno <= 0 || portno > 65535)
    {
        fprintf(stderr, "Invalid port number\n");
        exit(1);
    }

    strcpy(buffer, argv[3]);
    for (int i = 4; i < argc; i++)
    {
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        close(sockfd);
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        error("ERROR writing to socket");

    // n = write(sockfd, "\n", 1);
    // if (n < 0)
    //     error("ERROR writing newline to socket");

    memset(buffer, 0, BUFFER_SIZE);

    n = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (n < 0)
        error("ERROR reading from socket");

    printf("%s", buffer);

    close(sockfd);

    return 0;
}