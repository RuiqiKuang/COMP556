#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SEQ_SIZE 128 // maximum for 1 byte seq number, but only 64 is useful
#define ACK_SIZE 3
#define HEADER_SIZE 74 // 00000000 + Sequence number(1 byte) + data length(2 byte) + subdir name(50byte) + filename(20 byte).
#define DATA_SIZE 20000
#define CRC_SIZE 4

uint32_t crc32(uint8_t *buf, int32_t len)
{
	int i, j;
	uint32_t crc, mask;
	crc = 0xFFFFFFFF;
	for (i = 0; i < len; i++)
	{
		crc = crc ^ (uint32_t)buf[i];
		for (j = 7; j >= 0; j--)
		{
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	return ~crc;
}

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		perror("Invalid commmand");
		abort();
	}

	char *recv_addr = argv[2];
	char *file_path = argv[4];

	/* Process input arguments */
	char *substr = strtok(recv_addr, ":");
	char *recv_host = substr;
	substr = strtok(NULL, ":");
	unsigned short recv_port = atoi(substr);

	/* Process sendfile */
	char sendfile[sizeof(file_path)];
	strcpy(sendfile, file_path);

	substr = strtok(file_path, "/");
	char *subdir = substr;
	substr = strtok(NULL, "/");
	char *fileName = substr;

	int sock;
	/* Create a UDP socket */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Fail to create a UDP socket.");
		abort();
	}

	/* Open sendfile */
	FILE *fp = fopen(sendfile, "rb");
	if (!fp)
	{
		perror("Fail to open file.");
		abort();
	}
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(recv_host);
	sin.sin_port = htons(recv_port);

	socklen_t addr_len = sizeof(struct sockaddr_in);

	int packet_size = HEADER_SIZE + DATA_SIZE + CRC_SIZE;
	char *sendfile_data = (char *)malloc(DATA_SIZE * sizeof(char));

	size_t data_len;
	char *packet_message = malloc(packet_size);

	short seq_num = 0;
	char ack_buffer[ACK_SIZE];

	struct timeval time;
	double timeout = 10.0;
	double offset = 1.0;
	int start = 0;

	while ((data_len = fread(sendfile_data, 1, DATA_SIZE, fp)) >= 0)
	{
		if (data_len == 0)
		{
			seq_num = -1;
		}
		start += data_len;
		printf("[send data] start:%d, length:%zu.\n", start, data_len);
		memset(packet_message, 0, packet_size);
		memset(packet_message, 0, 1);
		memset(packet_message + 1, (char)seq_num, 1);
		*(short *)(packet_message + 2) = htons(data_len);
		memcpy(packet_message + 4, subdir, 50);
		memcpy(packet_message + 54, fileName, 20);

		memcpy(packet_message + HEADER_SIZE, sendfile_data, DATA_SIZE);

		memset(sendfile_data, 0, DATA_SIZE);

		/* calculate CRC */
		uint32_t crc = crc32((uint8_t *)&packet_message[0], HEADER_SIZE + DATA_SIZE);
		*(uint32_t *)(packet_message + HEADER_SIZE + DATA_SIZE) = htonl(crc);

		while (1)
		{
			ssize_t count = -1;
			while (count < 0)
			{
				count = sendto(sock, packet_message, packet_size, MSG_WAITALL, (struct sockaddr *)&sin, sizeof(sin));
			}
			time.tv_sec = (int)timeout;
			time.tv_usec = (timeout - time.tv_sec) * 1000000;
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&time, sizeof time);
			clock_t start = clock();
			ssize_t ack = recvfrom(sock, ack_buffer, ACK_SIZE, MSG_WAITALL, (struct sockaddr *)&sin, &addr_len);

			if (ack > 0 && ack_buffer[1] == (char)seq_num)
			{
				/* Adaptive timeout*/
				clock_t end = clock();
				timeout = ((double)(end - start) / CLOCKS_PER_SEC) * 1000 + offset;
				if (seq_num >= 0)
				{
					seq_num = (seq_num + 1) % SEQ_SIZE;
				}
				printf("Succeed to send packet\n");
				break;
			}
			else
			{
				timeout += offset;
				perror("Fail to receive ACK.");
				printf("Resend packet.\n");
			}
		}

		if (seq_num == -1)
		{
			printf("Transmission complete.\n");
			break;
		}
	}
	fclose(fp);
	free(sendfile_data);
	free(packet_message);
	close(sock);
	return 0;
}