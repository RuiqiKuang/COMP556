//
// Created by 陈镜泽 on 2023/1/31.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv)
{
    int mySock;
    unsigned int serverAdd;
    struct sockaddr_in serin;
    struct addrinfo *getServer, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /* indicates we want IPv4 */
    if (getaddrinfo(argv[1], NULL, &hints, &getServer) == 0)
    {
        serverAdd = (unsigned int)((struct sockaddr_in *)(getServer->ai_addr))->sin_addr.s_addr;
        freeaddrinfo(getServer);
    }

    unsigned short server_port = atoi(argv[2]);
    char *buffer, *sendbuffer;
    short size = atoi(argv[3]);
    int count, iteration = atoi(argv[4]);

    buffer = (char *)malloc(size);
    if (!buffer)
    {
        perror("failed to allocated buffer");
        abort();
    }

    sendbuffer = (char *)malloc(size);
    if (!sendbuffer)
    {
        perror("failed to allocated sendbuffer");
        abort();
    }

    if ((mySock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("opening TCP socket");
        abort();
    }

    /* fill in the server's address */
    memset(&serin, 0, sizeof(serin));
    serin.sin_family = AF_INET;
    serin.sin_addr.s_addr = serverAdd;
    serin.sin_port = htons(server_port);

    if (connect(mySock, (struct sockaddr *)&serin, sizeof(serin)) < 0)
    {
        perror("connect to server failed");
        abort();
    }

    count = recv(mySock, buffer, 50, 0);
    if (count < 0)
    {
        perror("receive failure");
        abort();
    }

    if (buffer[count - 1] != 0)
    {
        /* In general, TCP recv can return any number of bytes, not
       necessarily forming a complete message, so you need to
       parse the input to see if a complete message has been received.
           if not, more calls to recv is needed to get a complete message.
        */
        printf("Message incomplete, something is still being transmitted\n");
    }
    else
    {
        printf("Here is what we got: %s", buffer);
    }

    *(short *)(buffer) = size;

    // printf("size is: %d\n",*(short *)(buffer));

    struct timeval time, recvTime;
    long *locator = buffer + 2;
    long interval;

    while (iteration > 0)
    {
        gettimeofday(&time, NULL);
        *(long *)locator = time.tv_sec;
        *(long *)(locator + 1) = time.tv_usec;
        //printf("time %ld       %ld\n", time.tv_sec, time.tv_usec);
      // printf("send %d   %ld      %ld\n", *(short *)(buffer), *(long *)(locator), *(long *)(locator + 1));
        send(mySock, buffer, size, 0);
        count = recv(mySock, buffer, size, 0);
        gettimeofday(&recvTime, NULL);
        interval = (recvTime.tv_usec - time.tv_usec);
       // printf("recieve %d   %ld      %ld\n", *(short *)(buffer), *(long *)(locator), *(long *)(locator + 1));
        printf("trip time in milsec is: %ld \n", interval);

        iteration--;
    }
    return 0;
}
