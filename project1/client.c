#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <endian.h>
struct message
{
    unsigned short size;
    // struct timeval time;
    long sec;
    long usec;
    char data[65535];
};
void updatetime(struct message *message)
{
    struct timeval *time = (struct timeval *)malloc(sizeof(struct timeval));
    gettimeofday(time, NULL);
    message->sec = (long)time->tv_sec;
    message->usec = (long)time->tv_usec;
    free(time);
}
int main(int argc, char **argv)
{

    /* our client socket */
    int sock;

    /* variables for identifying the server */
    unsigned int server_addr;
    struct sockaddr_in sin;
    struct addrinfo *getaddrinfo_result, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /* indicates we want IPv4 */

    if (getaddrinfo(argv[1], NULL, &hints, &getaddrinfo_result) == 0)
    {
        server_addr = (unsigned int)((struct sockaddr_in *)(getaddrinfo_result->ai_addr))->sin_addr.s_addr;
        freeaddrinfo(getaddrinfo_result);
    }

    /* server port number */
    unsigned short server_port = atoi(argv[2]);
    char *receivebuffer, *sendbuffer;
    unsigned short size = atoi(argv[3]);
    int iteration = atoi(argv[4]);
    if (size < 18 || size > 65535)
    {
        perror("invalid size");
        abort();
    }
    if (iteration < 0 || iteration > 10000)
    {
        perror("invalid number of message exchange");
        abort();
    }

    receivebuffer = (char *)malloc(size);
    if (!receivebuffer)
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

    /* create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("opening TCP socket");
        abort();
    }

    /* fill in the server's address */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = server_addr;
    sin.sin_port = htons(server_port);

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("connect to server failed");
        abort();
    }

    // create ping message
    // message->data:
    char *ping_data = malloc(size - 18);
    memset(ping_data, '0', size - 18);
    struct message Send, Receive;
    Send.size = (unsigned short)(strlen(ping_data) + 18 > 65535 ? 65535 : strlen(ping_data) + 18);
    printf("Size is:%d.\n", Send.size);
    updatetime(&Send);
    memcpy(Send.data, ping_data, Send.size - 18);
    int epoch = 1;
    float avg_latency = 0.0;
    while (epoch <= iteration)
    {
        updatetime(&Send);
        // generate message
        *(unsigned short *)sendbuffer = (unsigned short)htobe16(Send.size); // size(2 bytes)
        *(long *)(sendbuffer + 2) = (long)htobe64(Send.sec);                // tv_sec(8 bytes)
        *(long *)(sendbuffer + 10) = (long)htobe64(Send.usec);              // tv_usec(8 bytes)
        memcpy(sendbuffer + 18, Send.data, Send.size - 18);                 // data
        int send_cnt_thistime = send(sock, sendbuffer, size, 0);
        while (send_cnt_thistime < size)
        {
            int send_cnt_nexttime = send(sock, sendbuffer + send_cnt_thistime, size - send_cnt_thistime, 0);
            send_cnt_thistime += send_cnt_nexttime;
        }
        int receive_cnt_thistime = recv(sock, receivebuffer, size, 0);
        while (receive_cnt_thistime < size)
        {
            int receive_cnt_nexttime = recv(sock, receivebuffer + receive_cnt_thistime, size - receive_cnt_thistime, 0);
            receive_cnt_thistime += receive_cnt_nexttime;
        }
        Receive.size = (unsigned short)be16toh(*(unsigned short *)receivebuffer);
        Receive.sec = (long)be64toh(*(long *)(receivebuffer + 2));
        Receive.usec = (long)be64toh(*(long *)(receivebuffer + 10));
        memcpy(Receive.data, receivebuffer + 18, Receive.size - 18);
        struct timeval *current_time = (struct timeval *)malloc(sizeof(struct timeval));
        gettimeofday(current_time, NULL);
        float latency = (current_time->tv_sec - Receive.sec) * 1000.000 + (current_time->tv_usec - Receive.usec) / 1000.000;
        printf("====================================================================\n");
        printf("Epoch %d:\n", epoch);
        printf("Latency is %.3f millisecond.\n", latency);
        avg_latency += latency;
        epoch += 1;
    }
    avg_latency /= iteration;
    printf("====================================================================\n");
    printf("After %d epochs\n", iteration);
    printf("The average latency is %.3f millisecond.\n", avg_latency);
    printf("====================================================================\n");
    free(ping_data);
    free(receivebuffer);
    free(sendbuffer);
    return 0;
}
