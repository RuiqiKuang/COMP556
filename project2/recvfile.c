#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define SEQ_SIZE 128
#define ACK_SIZE 3
#define HEADER_SIZE 74
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

int main (int argc, char** argv) {
	if (argc != 3) {
		perror("Invalid arguments number.");
		abort();
	}

	unsigned short recv_port = atoi(argv[2]);
	if (recv_port < 18000 || recv_port > 18200) {
		fprintf(stderr, "The port number should be in the range of [18000, 18200]");
		abort();
	}

	int sock;
	struct sockaddr_in sin, addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	/* Create a UDP socket */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Fail to create socket.");
		abort();
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(recv_port);
	if (bind(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		perror("binding socket to address");
		abort();
	}

	short seq_num = 0;
	int packet_size = HEADER_SIZE + DATA_SIZE + CRC_SIZE;
	char ack_buf[ACK_SIZE];
	char buf[packet_size];

	struct timeval tv;
	tv.tv_sec = 100;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv);

	FILE* fp;
	int rewrite = 0;
	while (1)
     {
		ssize_t count = -1;
		while (count < 0) {
			memset(buf, 0, sizeof(buf));
			count = recvfrom(sock, buf, packet_size, MSG_WAITALL, (struct sockaddr*) &addr, &addr_len);
		}

		char seq_num_received;
		memcpy(&seq_num_received, buf + 1, 1);
		printf("Sequence number %d.\n", seq_num_received);
		if (seq_num_received == (char) -1) {
			printf("Transmission complete.\n");
			memset(ack_buf, 0, ACK_SIZE);
			ack_buf[1] = seq_num_received;
			while (sendto(sock, ack_buf, ACK_SIZE, MSG_CONFIRM, (struct sockaddr*) &addr, sizeof(addr)) < 0);
			break;
		}

		size_t packet_bytes = (short) ntohs(*(short*) (buf + 2));

		char subdir[50];
		memcpy(subdir, buf + 4, 50);
		char fileName[20];
		memcpy(fileName, buf + 54, 20);
		char recv_data[packet_bytes];
		memcpy(recv_data, buf + HEADER_SIZE, packet_bytes);

		// Check CRC and checksum
		uint32_t crc_received = (uint32_t) ntohl(*(uint32_t*) (buf + HEADER_SIZE + DATA_SIZE));
		uint32_t crc_calculated = crc32((uint8_t*) &buf[0], HEADER_SIZE + DATA_SIZE);

		if (crc_received != crc_calculated) {
			printf("Packet corrupted\n");
			continue;
		}

		if ((short) seq_num_received < seq_num || ((short) seq_num_received > seq_num  && seq_num + SEQ_SIZE > (short) seq_num_received)) {
			printf("Ignore duplicated Packet.\n");
		} 
        else {
			printf("Packet Accept\n");
			seq_num = (seq_num + 1) % SEQ_SIZE;
			char* path = (char*) malloc(6 + strlen(subdir) + strlen(fileName));
			sprintf(path, "%s/%s.recv", subdir, fileName);//path=subdir+filename+".recv"
			if (rewrite == 0) {
				if (access(subdir, 0) == -1) {
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
		memset(ack_buf, 0, ACK_SIZE);
		ack_buf[1] = seq_num_received;
		printf("Sending response \"%d\"\n", seq_num_received);
		if (sendto(sock, ack_buf, ACK_SIZE, MSG_CONFIRM, (struct sockaddr*) &addr, sizeof(addr)) < 0)
			printf("Fail to send ack");
	}
	close(sock);
	return 0;
}
