#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <endian.h>
/**************************************************/
/* a few simple linked list functions             */
/**************************************************/

/* A linked list node data structure to maintain application
   information related to a connected socket */
struct node
{
  int socket;
  struct sockaddr_in client_addr;
  int pending_data; /* flag to indicate whether there is more data to send */
  /* you will need to introduce some variables here to record
     all the information regarding this socket.
     e.g. what data needs to be sent next */
  char *receivebuffer;
  int BUFFER_LEN;
  struct node *next;
  int count;
  unsigned short size;
};

/* remove the data structure associated with a connected socket
   used when tearing down the connection */
void dump(struct node *head, int socket)
{
  struct node *current, *temp;

  current = head;

  while (current->next)
  {
    if (current->next->socket == socket)
    {
      /* remove */
      temp = current->next;
      current->next = temp->next;
      free(temp->receivebuffer);
      free(temp); /* don't forget to free memory */
      return;
    }
    else
    {
      current = current->next;
    }
  }
}

/* create the data structure associated with a connected socket */
void add(struct node *head, int socket, struct sockaddr_in addr)
{
  struct node *new_node;
  new_node = (struct node *)malloc(sizeof(struct node));
  new_node->socket = socket;
  new_node->client_addr = addr;
  new_node->receivebuffer = (char *)malloc(65535);
  new_node->BUFFER_LEN = 0;
  new_node->pending_data = 0;
  new_node->next = head->next;
  new_node->count = 0;
  new_node->size = 0;
  head->next = new_node;
}

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
/*****************************************/
/* main program                          */
/*****************************************/

/* simple server, takes one parameter, the server port number */
int main(int argc, char **argv)
{

  /* socket and option variables */
  int sock, new_sock, max;
  int optval = 1;

  /* server socket address variables */
  struct sockaddr_in sin, addr;
  unsigned short server_port = atoi(argv[1]);
  if (server_port < 18000 || server_port > 18200)
  {
    perror("Invalid server port");
    abort();
  }
  /* socket address variables for a connected client */
  socklen_t addr_len = sizeof(struct sockaddr_in);

  /* maximum number of pending connection requests */
  int BACKLOG = 5;

  /* variables for select */
  fd_set read_set, write_set;
  struct timeval time_out;
  int select_retval;

  /* a silly message */
  // char *message = "Welcome! COMP/ELEC 556 Students!\n";
  struct message Receive;

  /* number of bytes sent/received */
  int count;
  int cc = 0;
  /* numeric value received */
  // int num;

  /* linked list for keeping track of connected sockets */
  struct node head;
  struct node *current, *next;

  /* a buffer to read data */
  // char *receivebuffer;
  char *sendbuffer;
  int BUFFER_LEN = 65535;

  // receivebuffer = (char *)malloc(BUF_LEN);
  sendbuffer = (char *)malloc(BUFFER_LEN);
  /* initialize dummy head node of linked list */
  head.socket = -1;
  head.next = 0;

  /* create a server socket to listen for TCP connection requests */
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    perror("opening TCP socket");
    abort();
  }

  /* set option so we can reuse the port number quickly after a restart */
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setting TCP socket option");
    abort();
  }

  /* fill in the address of the server socket */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(server_port);

  /* bind server socket to the address */
  if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    perror("binding socket to address");
    abort();
  }

  /* put the server socket in listen mode */
  if (listen(sock, BACKLOG) < 0)
  {
    perror("listen on socket failed");
    abort();
  }

  /* now we keep waiting for incoming connections,
     check for incoming data to receive,
     check for ready socket to send more data */
  while (1)
  {

    /* set up the file descriptor bit map that select should be watching */
    FD_ZERO(&read_set);  /* clear everything */
    FD_ZERO(&write_set); /* clear everything */

    FD_SET(sock, &read_set); /* put the listening socket in */
    max = sock;              /* initialize max */

    /* put connected sockets into the read and write sets to monitor them */
    for (current = head.next; current; current = current->next)
    {
      FD_SET(current->socket, &read_set);

      if (current->pending_data)
      {
        /* there is data pending to be sent, monitor the socket
                 in the write set so we know when it is ready to take more
                 data */
        FD_SET(current->socket, &write_set);
      }

      if (current->socket > max)
      {
        /* update max if necessary */
        max = current->socket;
      }
    }

    time_out.tv_usec = 100000; /* 1-tenth of a second timeout */
    time_out.tv_sec = 0;

    /* invoke select, make sure to pass max+1 !!! */
    select_retval = select(max + 1, &read_set, &write_set, NULL, &time_out);
    if (select_retval < 0)
    {
      perror("select failed");
      abort();
    }

    if (select_retval == 0)
    {
      /* no descriptor ready, timeout happened */
      continue;
    }

    if (select_retval > 0) /* at least one file descriptor is ready */
    {
      if (FD_ISSET(sock, &read_set)) /* check the server socket */
      {
        /* there is an incoming connection, try to accept it */
        new_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);

        if (new_sock < 0)
        {
          perror("error accepting connection");
          abort();
        }

        /* make the socket non-blocking so send and recv will
                 return immediately if the socket is not ready.
                 this is important to ensure the server does not get
                 stuck when trying to send data to a socket that
                 has too much data to send already.
               */
        if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0)
        {
          perror("making socket non-blocking");
          abort();
        }

        /* the connection is made, everything is ready */
        /* let's see who's connecting to us */
        printf("Accepted connection. Client IP address is: %s\n",
               inet_ntoa(addr.sin_addr));

        /* remember this client connection in our linked list */
        add(&head, new_sock, addr);
      }

      /* check other connected sockets, see if there is
               anything to read or some socket is ready to send
               more pending data */
      for (current = head.next; current; current = next)
      {
        next = current->next;
        /* see if we can now do some previously unsuccessful writes */
        if (FD_ISSET(current->socket, &read_set))
        {
          /* we have data from a client */
          printf("====================================================================\n");
          current->count += recv(current->socket, current->receivebuffer + current->count, BUFFER_LEN, 0);
          int size_thistime = 0;
          if (current->count <= 0)
          {
            /* something is wrong */
            if (current->count == 0)
            {
              printf("Client closed connection. Client IP address is: %s\n", inet_ntoa(current->client_addr.sin_addr));
            }
            else
            {
              perror("error receiving from a client");
            }

            /* connection is closed, clean up */
            close(current->socket);
            dump(&head, current->socket);
          }
          else
          {
            // First, we have to get the size and the time to determine the size of this message,
            // then we know when the message will end.
            while (current->count < 2)
            {
              size_thistime = recv(current->socket, current->receivebuffer + current->count, BUFFER_LEN - current->count, 0);
              current->count += size_thistime;
            }
            Receive.size = (unsigned short)be16toh(*(unsigned short *)current->receivebuffer);
            Receive.sec = (long)be64toh(*(long *)(current->receivebuffer + 2));
            Receive.usec = (long)be64toh(*(long *)(current->receivebuffer + 10));
            memcpy(Receive.data, current->receivebuffer + 18, Receive.size - 18);

            current->size = (unsigned short)be16toh(*(unsigned short *)current->receivebuffer);
            while (Receive.size != count)
            {
              size_thistime = recv(current->socket, current->receivebuffer + count, BUFFER_LEN - count, 0);
              count += size_thistime;
            }
            /*Receive the whole message*/
            printf("Received ping message from %s  count %d size %d\n", inet_ntoa(current->client_addr.sin_addr),current->count,current->size);
            // printf("size is: %d  %d\n", (unsigned short)be16toh(*(unsigned short *)receivebuffer), count);
            *(unsigned short *)sendbuffer = (unsigned short)htobe16(Receive.size);
            *(long *)(sendbuffer + 2) = (long)htobe64(Receive.sec);
            *(long *)(sendbuffer + 10) = (long)htobe64(Receive.usec);
            memcpy(sendbuffer + 18, Receive.data, Receive.size - 18);
            if (current->count == current->size)
            {
              while (current->count != 0)  {
                size_thistime = send(current->socket, current->receivebuffer + current->size - current->count, current->count, 0);
                current->count -= size_thistime;
              }
              printf("send total times %d  size %d\n",++cc,size_thistime);
            }
          }
        }
      }
    }
  }
}
