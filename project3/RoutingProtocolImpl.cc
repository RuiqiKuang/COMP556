#include "RoutingProtocolImpl.h"

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n)
{
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl()
{
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type)
{
  // add your own code
}

void RoutingProtocolImpl::handle_alarm(void *data)
{
  // add your own code
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size)
{
  // add your own code
}

// add more of your own code
void RoutingProtocolImpl::send_ping(unsigned short port)
{
  char *ping_msg = (char *)malloc(this->ping_pong_msg_size);
  // 0-1:Packet Type; 1-2:Reserved;  2-4:Packet Size; 4-6:Source ID; 6-8:Destination ID(unused in PING); 8-12:current time
  *ping_msg = (char)PING;
  *(unsigned short *)(ping_msg + 2) = (unsigned short)htons(this->ping_pong_msg_size);
  *(unsigned short *)(ping_msg + 4) = (unsigned short)htons(this->router_id);
  unsigned int current_time = (unsigned int)htonl(sys->time());
  *(unsigned int *)(ping_msg + 8) = current_time;
  sys->send(port, ping_msg, this->ping_pong_msg_size);
}

void RoutingProtocolImpl::recv_ping_send_pong(unsigned short port, char *msg)
{
  char *pong_msg = (char *)malloc(this->ping_pong_msg_size);
  *pong_msg = (char)PONG;
  *(unsigned short *)(pong_msg + 2) = (unsigned short)htons(this->ping_pong_msg_size);
  *(unsigned short *)(pong_msg + 4) = (unsigned short)htons(this->router_id);
  unsigned short dst_id = (unsigned short)ntohs(*(unsigned short *)(packet + 4));
  //*(unsigned short *)(pong_msg + 6) = *(unsigned short*)(msg + 4);
  *(unsigned short *)(pong_msg + 6) = (unsigned short)htons(dst_id);
  unsigned int timestamp = (unsigned int)ntohl(*(unsigned int *)(msg + 8));
  *(unsigned int *)(pong_msg + 8) = (unsigned int)htonl(timestamp);
  free(msg);
  sys->send(port, pong_msg, this->ping_pong_msg_size);
}

void recv_pong(unsigned short port, char *msg)
{
  if (this->router_id != (unsigned short)ntohs(*(unsigned short *)(msg + 6)))
  {
    free(msg);
    return;
  }
  unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short *)(msg + 4));
  unsigned short RTT = (short)(sys->time() - (unsigned int)ntohl(*(unsigned int *)(msg + 8)));
  bool update = false;
  if (neighbor_table.find(neighbor_router_id) != neighbor_table.end())
  {
    // neighbor's router ID: <cost, port, timeout>
    unsigned short prev_cost, prev_port;
    tie(prev_cost, prev_port, std::ignore) = neighbor_table[neighbor_router_id];
    if (prev_cost != RTT)
    {
      update = true;
    }
    neighbor_table[neighbor_router_id] = make_tuple(RTT, port, sys->time() + this->ping_pong_timeout);
    if (port != prev_port)
    {
      port_table.erase(prev_port);
      port_table[port] = neighbor_router_id;
      updated = true;
    }
  }
  else
  {
    neighbor_table[neighbor_router_id] = make_tuple(RTT, port, sys->time() + PONG_TIMEOUT);
    port_table[port] = neighbor_router_id;
    update = true;
  }
  free(msg);
  if (update == true)
  {
    // TODO: update with the specific protocol
  }
}