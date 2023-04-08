//
// Created by 陈镜泽 on 2023/4/4.
//

#ifndef COMP556_DVPROIMP_H
#define COMP556_DVPROIMP_H

#include "Node.h"
#include "string.h"
#include <arpa/inet.h>

#define DV_LS_TIMEOUT 45000

#define PING_DURATION 10000
#define DV_DURATION 30000
#define LS_DURATION 30000
#define TIMEOUT_DURATION 1000

class DVProImp {
public :
    DVProImp(unordered_map<unsigned short, tuple<unsigned short, unsigned short, unsigned int>> &neighbor_table,unordered_map<unsigned short, pair<unsigned short, unsigned short>> &routing_table, unordered_map<unsigned short, unsigned short> &port_table,
             unsigned short router_id,unsigned short num_ports, Node *sys);
    DVProImp();

    void recv(unsigned short port, char *msg, unsigned short size);

    void update(unsigned short id, unsigned short RTT,unsigned short oldPort);

    void send();

    void sendDV(unsigned short ID, unsigned short port,char *commonPart);

    char* generateDVMsg();

    void send_DV(unsigned short port_id, unsigned short dest_id);

    bool refresh();
private:
    unordered_map<unsigned short, tuple<unsigned short, unsigned short, unsigned int>> neighbor_table;
    unordered_map<unsigned short, pair<unsigned short, unsigned short>> routing_table;
    unordered_map<unsigned short, unsigned short> port_table;
    unordered_map<unsigned short, unsigned int> DV_time;
    unsigned short router_id;
    unsigned short num_ports;
    bool IsUpdated;
    Node * sys;
};

#endif //COMP556_DVPROIMP_H
