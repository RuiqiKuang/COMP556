//
// Created by 陈镜泽 on 2023/4/6.
//
#include "DVProImp.h"
void DVProImp :: init (unordered_map<unsigned short, tuple<unsigned short, unsigned short, unsigned int>> *neighbor_table,unordered_map<unsigned short, pair<unsigned short, unsigned short>> *routing_table,unordered_map<unsigned short, unsigned short> *port_table,
                      unsigned short router_id,unsigned short num_ports, Node *sys) {
    this->router_id = router_id;
    this->neighbor_table = neighbor_table;
    this->routing_table = routing_table;
    this->port_table = port_table;
    this->num_ports = num_ports;
    this->sys = sys;
    IsUpdated = true;
}

DVProImp :: DVProImp () {

}

void DVProImp ::recv(unsigned short port, char *msg, unsigned short size) {
    unsigned short source_id = ntohs(*(unsigned short *) (msg + 4));

    if (neighbor_table->count(source_id) == 0) {
        free(msg);
        return;
    }

    unsigned short id,cost;
    unordered_map<unsigned short, unsigned short> DV_table;

    for(int i = 8; i < size; i += 4) {
        id = ntohs(*(unsigned short *)(msg + i));
        cost = ntohs(*(unsigned short *)(msg + i + 2));
        DV_table[id] = cost;
    }
    free(msg);

    vector<unsigned short > eraseNodes;
    for(auto DVentry : DV_table) {
        if (DVentry.first == this->router_id)
            continue;
        if (DVentry.second == INFINITY_COST) {
//            if (routing_table.count(DVentry.first) != 0) {
//                auto nextHop = routing_table[DVentry.first].second;
//                if(nextHop == source_id) {
//                    eraseNodes.push_back(DVentry.first);
//                    DV_time.erase(DVentry.first);
//                    IsUpdated = false;
//                }
//            }
            continue;
        }

        DV_time[DVentry.first] = sys->time() + DV_LS_TIMEOUT;

        unsigned short totalCost = DVentry.second + get<0>(neighbor_table->find((*port_table)[port])->second);

        if (routing_table->count(DVentry.first) == 0) {
            (*routing_table)[DVentry.first] = make_pair(totalCost, source_id);
            IsUpdated = false;
        }else {
            if ((*routing_table)[DVentry.first].second == source_id || (*routing_table)[DVentry.first].first > totalCost) {
                (*routing_table)[DVentry.first] = make_pair(totalCost, source_id);
                IsUpdated = false;
            }
        }
    }

    if (!IsUpdated) {
        send();
        IsUpdated = true;
    }
    for (unsigned short id : eraseNodes) {
        routing_table->erase(id);
    }
}

void DVProImp ::send() {
    
    for (auto neighbor : *neighbor_table) {
        send_DV(get<1>(neighbor.second), neighbor.first);
    }
}




void DVProImp::send_DV(unsigned short port_id, unsigned short dest_id) {
    unsigned short num_entries = routing_table->size();
    unsigned short size = 8 + num_entries * 4;
    char *dv_packet = (char *) malloc(size);
    // Write header
    *dv_packet = (char) DV;
    *(unsigned short *) (dv_packet + 2) = (unsigned short) htons(size);
    *(unsigned short *) (dv_packet + 4) = (unsigned short) htons(this->router_id);
    *(unsigned short *) (dv_packet + 6) = (unsigned short) htons(dest_id);
    // Write table <node ID, cost>
    int i = 0;
    for (auto line: *routing_table) {
        unsigned short node_id = line.first;
        auto cost_hop = line.second;
        unsigned short cost = cost_hop.first;
        unsigned short hop = cost_hop.second;
        // Poison reverse
        if (dest_id == hop) {
            cost = INFINITY_COST;
        }
        *(unsigned short *) (dv_packet + 8 + i * 4) = (unsigned short) htons(node_id);
        *(unsigned short *) (dv_packet + 10 + i * 4) = (unsigned short) htons(cost);
        i += 1;
    }
    sys->send(port_id, dv_packet, size);
}

void DVProImp ::update(unsigned short id, unsigned short RTT) {

    if (routing_table->count(id) == 0) {
        (*routing_table)[id] = make_pair(RTT,id);
        DV_time[id] = sys->time() +DV_LS_TIMEOUT;
    }else {
        unsigned short nextHop = (*routing_table)[id].second;
        if (nextHop == id) {
            (*routing_table)[id].first = RTT;
            DV_time[id] = sys->time() +DV_LS_TIMEOUT;
        }else if (RTT < (*routing_table)[id].first) {
            (*routing_table)[id].first = RTT;
            (*routing_table)[id].second = id;
            DV_time[id] = sys->time() +DV_LS_TIMEOUT;
        }
    }
}

bool DVProImp ::refresh() {
    for (auto DvEntry = DV_time.begin(); DvEntry != DV_time.end(); ) {
        auto nextD = next(DvEntry);
        auto it_in_routing_table = routing_table->find(DvEntry->first);
        if (sys->time() >= DvEntry->second) {
            routing_table->erase(it_in_routing_table);
            DV_time.erase(DvEntry);
            IsUpdated = false;
        }
        DvEntry = nextD;
    }
    if (!IsUpdated) {
        send();
        IsUpdated = true;
    }
    return IsUpdated;
}