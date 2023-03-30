#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
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

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		perror("Invalid command.");
		abort();
	}

	unsigned short recv_port = atoi(argv[2]);
	if (recv_port < 18000 || recv_port > 18200)
	{
		perror("Invalid server port");
		abort();
	}

	int sock;
	struct sockaddr_in sin, addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	/* Create a UDP socket */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Fail to create a UDP socket.");
		abort();
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(recv_port);
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("binding socket to address");
		abort();
	}

	short seq_num = 0; // this is a short type because seq number may be -1
	int packet_size = HEADER_SIZE + DATA_SIZE + CRC_SIZE;
	char ack_buffer[ACK_SIZE];
	char recv_buffer[packet_size];

	struct timeval time;
	time.tv_sec = 100;
	time.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&time, sizeof time);

	FILE *fp;
	int rewrite = 0;
	while (1)
	{
		ssize_t size_received = -1;
		while (size_received < 0)
		{
			memset(recv_buffer, 0, sizeof(recv_buffer));
			size_received = recvfrom(sock, recv_buffer, packet_size, MSG_WAITALL, (struct sockaddr *)&addr, &addr_len);
		}

		char seq_num_received;
		memcpy(&seq_num_received, recv_buffer + 1, 1);
		printf("Sequence number %d.\n", seq_num_received);
		if (seq_num_received == (char)-1)
		{
			printf("Transmission complete.\n");
			memset(ack_buffer, 0, ACK_SIZE);
			ack_buffer[1] = seq_num_received;
			while (sendto(sock, ack_buffer, ACK_SIZE, MSG_CONFIRM, (struct sockaddr *)&addr, sizeof(addr)) < 0)
				;
			break;
		}

		size_t data_len = (short)ntohs(*(short *)(recv_buffer + 2));

		char subdir[50];
		memcpy(subdir, recv_buffer + 4, 50);
		char fileName[20];
		memcpy(fileName, recv_buffer + 54, 20);
		char recv_data[data_len];
		memcpy(recv_data, recv_buffer + HEADER_SIZE, data_len);

		// Check CRC
		uint32_t crc_received = (uint32_t)ntohl(*(uint32_t *)(recv_buffer + HEADER_SIZE + DATA_SIZE));
		uint32_t crc_calculated = crc32((uint8_t *)&recv_buffer[0], HEADER_SIZE + DATA_SIZE);

		if (crc_received != crc_calculated)
		{
			printf("Receive corrupt packet.\n");
			continue;
		}

		if ((short)seq_num_received < seq_num)
		{
			printf("IGNORED.\n");
		}
		else if ((short)seq_num_received > seq_num && seq_num + SEQ_SIZE > (short)seq_num_received)
		{
			printf("ACCEPTED(out-of-order).\n");
		}
		else
		{
			printf("ACCEPTED(in-order),\n");
			seq_num = (seq_num + 1) % SEQ_SIZE;
			char *path = (char *)malloc(6 + strlen(subdir) + strlen(fileName)); // 6 = strlen(".recv")+\0
			sprintf(path, "%s/%s.recv", subdir, fileName);						// path=subdir+filename+".recv"
			if (rewrite == 0)
			{
				if (access(subdir, 0) == -1)
				{
					printf("Make subdir %s\n", subdir);
					mkdir(subdir, S_IRWXU | S_IRWXG | S_IRWXO);
				}
				remove(path);
				rewrite = 1;
			}
			fp = fopen(path, "ab+");
			fwrite(recv_data, 1, sizeof(recv_data), fp);
			fclose(fp);
		}
		memset(ack_buffer, 0, ACK_SIZE);
		ack_buffer[1] = seq_num_received;
		printf("Sending response \"%d\"\n", seq_num_received);
		if (sendto(sock, ack_buffer, ACK_SIZE, MSG_CONFIRM, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			printf("Fail to send ack");
		}
	}
	close(sock);
	return 0;
}
