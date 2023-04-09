#include "RoutingProtocolImpl.h"

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
    sys = n;
    // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
    // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
    // add your own code
    this->num_ports = num_ports;
    this->protocol_type = protocol_type;
    this->router_id = router_id;

    Dv = DVProImp(neighbor_table,routing_table,port_table,router_id,num_ports,sys);

    for(unsigned int p; p < num_ports; p++){
        send_ping(p);
    }

    sys->set_alarm(this, PING_DURATION, (void *)this->Ping_alarm);

    sys->set_alarm(this, TIMEOUT_DURATION, (void *)this->Port_alarm);
    // 2. update DV duration set 30000, update LS duration set 30000
    if (protocol_type == P_DV) {
        sys->set_alarm(this, DV_DURATION, (void *)this->DV_alarm);

    } else if (protocol_type == P_LS)	{
        sequence_num = 0;
		sys->set_alarm(this, LS_DURATION, (void *)this->LS_alarm);
    }

}

void RoutingProtocolImpl::handle_alarm(void *data) {
    // handle periodic event: send PingPongMessage, send LS_update, send DV_update
    // check alarm message and setting the consequence alarm.

    // if the alarm is ping_update_alarm
    char *alarm_type = (char *)data;
    if (strcmp(alarm_type,this->Ping_alarm) == 0) {
        // add code...send_ping_packet() and set the next alarm
        for(unsigned int p; p < num_ports; p++) {
            send_ping(p);
        }
        sys->set_alarm(this, PING_DURATION, (void *)this->Ping_alarm);
    }
    else if (strcmp(alarm_type,this->DV_alarm) == 0) {
        // add code...update_DV() and set the next alarm
        Dv.send();
        sys->set_alarm(this, DV_DURATION, (void *)this->DV_alarm);
    }
    else if (strcmp(alarm_type,this->LS_alarm) == 0) {
        // add code...update_LS() and set the next alarm
        update_LS();
        sys->set_alarm(this, LS_DURATION, (void *)this->LS_alarm);
    }
    else if (strcmp(alarm_type,this->Port_alarm) == 0) {
        update_timeout();
        sys->set_alarm(this, TIMEOUT_DURATION, (void *)Port_alarm);
    }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
    // add your own code
    char *packet_as_cstring = (char*) packet;
    switch(packet_as_cstring[0]) {
        case PING:
            recv_ping_send_pong(port, packet_as_cstring);
            break;

        case PONG:
            recv_pong(port, packet_as_cstring);
            break;

        case DV:
            Dv.recv(port, packet_as_cstring, size);
            break;

        case LS:
            recv_LS(port, packet_as_cstring, size);
            break;

        case DATA:
            recv_data(port, packet_as_cstring, size);
            break;

        default:
            cout<<"Error: Unknown Packet Type!"<<endl;
            break;
    }
}

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
    unsigned short dst_id = (unsigned short)ntohs(*(unsigned short *)(msg + 4));
    //*(unsigned short *)(pong_msg + 6) = *(unsigned short*)(msg + 4);
    *(unsigned short *)(pong_msg + 6) = (unsigned short)htons(dst_id);
    unsigned int timestamp = (unsigned int)ntohl(*(unsigned int *)(msg + 8));
    *(unsigned int *)(pong_msg + 8) = (unsigned int)htonl(timestamp);
    free(msg);
    sys->send(port, pong_msg, this->ping_pong_msg_size);
}

void RoutingProtocolImpl::recv_pong(unsigned short port, char *msg)
{
    if (this->router_id != (unsigned short)ntohs(*(unsigned short *)(msg + 6)))
    {
        free(msg);
        return;
    }
    unsigned short neighbor_router_id = (unsigned short)ntohs(*(unsigned short *)(msg + 4));
    unsigned short RTT = (short)(sys->time() - (unsigned int)ntohl(*(unsigned int *)(msg + 8)));
    bool update = false;
    unsigned oldID = port_table[port];
    if (port_table.find(port) != port_table.end())
    {
        // neighbor's router ID: <cost, port, timeout>
        unsigned short prev_cost, prev_id;
        prev_id = port_table[port];
        prev_cost = get<0>(neighbor_table[prev_id]);

        if (prev_cost != RTT)
        {
            update = true;
        }
        neighbor_table[neighbor_router_id] = make_tuple(RTT, port, sys->time() + this->ping_pong_timeout);
        if (neighbor_router_id != prev_id)
        {
            port_table[port] = neighbor_router_id;
            neighbor_table.erase(prev_id);
            update = true;
        }
    }
    else
    {
        neighbor_table[neighbor_router_id] = make_tuple(RTT, port, sys->time() + ping_pong_timeout);
        port_table[port] = neighbor_router_id;
        update = true;
    }
    free(msg);
    if (update == true)
    {
        switch(protocol_type){
            case P_LS:
                update_LS();
                break;
            case P_DV:
                Dv.update(neighbor_router_id,RTT,oldID);
                Dv.send();
                break;
        }
    }
}

void RoutingProtocolImpl::recv_data(unsigned short port, char* packet, unsigned short size) {
    unsigned short dest = (unsigned short)ntohs(*(unsigned short *)(packet+6)); // package destination
    // check if the destination is the node: if is the node, then stop free the package. else keep sending
    if(dest == router_id){
        free(packet);
        return;
    }
    // print_routing_table();

    auto it_in_routing_table = routing_table.find(dest);
    // if this node can reach the destintion with this node
    if(it_in_routing_table != routing_table.end()){
        // find the port to send: find the destination is map to which port the node is going to send
        unsigned short next_hop = it_in_routing_table->second.second;
        auto it_in_neighbor_table = neighbor_table.find(next_hop);
        if(it_in_neighbor_table != neighbor_table.end()){
            auto cost_and_port = it_in_neighbor_table->second;
            sys->send(get<1>(cost_and_port), packet, size);
        }
    }
}

void RoutingProtocolImpl::update_timeout() {
    // check port states
    unsigned short cost, port;
    unsigned int timeout;
    bool changed = false;
    for (auto it = neighbor_table.begin(); it != neighbor_table.end();) {
        auto next_it = next(it);
        tie(cost, port, timeout) = it->second;
        if(sys->time() >= timeout) {
            neighbor_table.erase(it);
            port_table.erase(port);
            changed = true;
        }
        it = next_it;
    }

    // cout << "my protocol: " << protocol << endl;
    if (protocol_type == P_DV) {

        Dv.refresh();
        // cout << "DV done" << endl;
    } else {
        for (auto &it_in_graph : graph) {
			auto &edges = it_in_graph.second;
			for (auto edge = edges.begin(); edge != edges.end();) {
				auto next_edge = next(edge);
				if (sys->time() >= edge->second.second) {
					edges.erase(edge);
					changed = true;
				}
				edge = next_edge;
			}
		}
		if (changed) {
			update_LS();
		}
    }
}


// add more of your own code
