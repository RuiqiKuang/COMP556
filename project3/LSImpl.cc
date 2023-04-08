#include "RoutingProtocolImpl.h"
#include <queue>
#include <cstring>
#include <unordered_set>

void RoutingProtocolImpl::recv_LS(unsigned short port, char *msg, unsigned short size) {
    // get source id and sequence number
    unsigned short source_id = ntohs(*(unsigned short *) (msg + 4));
    unsigned long long sequence_num = ntohs(*(unsigned short *) (msg + 8));

    // an older LSP with a sequence number previously seen is discarded
    if (max_sequence_num.find(source_id) != max_sequence_num.end() 
        && max_sequence_num[source_id] >= sequence_num) {
        free(msg);
        return;
    }
    
    // update maximum sequence number for the source node
    max_sequence_num[source_id] = sequence_num;

    // get neighbor ids and costs of the source node and update the graph
    del_node(source_id);
    unsigned short source_neighbor_id;
    unsigned short cost;
    for (int i = 12; i < size; i += 4) {
        source_neighbor_id = ntohs(*(unsigned short *) (msg + i));
        cost = ntohs(*(unsigned short *) (msg + i + 2));
        add_edge(source_id, source_neighbor_id, cost);
    }

    update_forwarding_table();

    send_LS(msg, size, port);
}

void RoutingProtocolImpl::send_LS(char *msg, unsigned short size, unsigned short port) {
    // flood newer LSP to all neighbors except the neighbor where the LSP comes from
    for (auto &link: neighbor_table) {
        auto ltuple = link.second;
        unsigned short port_num = get<1>(ltuple);
        if (port_num == port) continue;
        char *packet_copy = (char *)malloc(size);
        memcpy(packet_copy, msg, size); // all the packets will be freed when received 
        sys->send(port_num, packet_copy, size);
    }
    free(msg);
}

void RoutingProtocolImpl::update_LS() {
    // update the graph
    del_node(this->router_id);
    for (auto &link: neighbor_table) {
        unsigned short neighbor_id = link.first;
        unsigned short cost = get<0>(link.second);
        add_edge(this->router_id, neighbor_id, cost);
    }

    // generate packet message
    unsigned short size = 12 + neighbor_table.size() * 4;
    char *msg = (char *) malloc(size);
    // write header (packet type, size, source id, sequence number)
    *msg = (char) LS;
    *(unsigned short *) (msg + 2) = (unsigned short) htons(size);
    *(unsigned short *) (msg + 4) = (unsigned short) htons(this->router_id);
    *(unsigned long long *) (msg + 8) = (unsigned long long) htons(sequence_num);
    sequence_num++;
    // write table (neighbor id - cost)
    for (int i = 12; i < size; i += 4) {
        for (auto &link: neighbor_table) {
            unsigned short neighbor_id = link.first;
            unsigned short cost = get<0>(link.second);
            *(unsigned short *) (msg + i) = (unsigned short) htons(neighbor_id);
            *(unsigned short *) (msg + i + 2) = (unsigned short) htons(cost);
        }
    }

    send_LS(msg, size, SPECIAL_PORT);
}

void RoutingProtocolImpl::update_forwarding_table() {
    unsigned short u, v, w;
    // Dijkstra with priority queue
    // key: dest_ID. value: cost/distance
    unordered_map<unsigned short, unsigned short> distance;
    // key: dest_ID. value: next hop
    unordered_map<unsigned short, unsigned short> next_hop;
    // key: dest_ID. value: if the least cost path is definitively known
    unordered_map<unsigned short, bool> visited;

    typedef pair<unsigned short, unsigned short> dis_dest;
    priority_queue<dis_dest, vector<dis_dest>, greater<dis_dest>> minq;
    
    // initializaton
    distance[this->router_id] = 0;
    next_hop[this->router_id] = this->router_id;
    visited[this->router_id] = true;
    for (auto &link: neighbor_table) {
        auto v = link.first;
        auto w = get<0>(link.second);
        distance[v] = w;
        next_hop[v] = port_table[get<1>(link.second)];
        minq.push(make_pair(distance[v], v));
    }

    // loop until all the least cost paths to every destination are known
    while (!minq.empty()) {
        u = minq.top().second;
        minq.pop();
        if (visited[u]) continue;
        visited[u] = true;
        for (auto edge : graph[u]) {
            v = edge.first;
            w = edge.second.first;
            if (!distance.count(v) || distance[v] > distance[u] + w) {
                distance[v] = distance[u] + w;
                next_hop[v] = next_hop[u];
                minq.push(make_pair(distance[v], v));
            }
        }
    }

    // update the routing table
    routing_table.clear();
    for (auto dest_dis : distance) {
        if (dest_dis.first == this->router_id) continue;
        unsigned short dest_id = dest_dis.first;
        unsigned short cur_dis = dest_dis.second;
        unsigned short cur_hop = next_hop[dest_id];
        routing_table[dest_id] = {cur_dis, cur_hop};
    }

}

void RoutingProtocolImpl::del_node(unsigned short node_id) {
    for (auto &it_in_graph : graph) {
        auto &edges = it_in_graph.second;
        for (auto edge = edges.begin(); edge != edges.end();) {
            auto next_edge = next(edge);
            if (edge.first == node_id) {
                edges.erase(edge);
            }
            edge = next_edge;
        }
    }
    graph.erase(node_id);
}

void RoutingProtocolImpl::add_edge(unsigned short u, unsigned short v, unsigned short weight) {
    cout << "add_edge " << u << " " << v << " " << weight << " " << sys->time() + DV_LS_TIMEOUT << endl;
    graph[u][v] = make_pair(weight, sys->time() + DV_LS_TIMEOUT);
    graph[v][u] = make_pair(weight, sys->time() + DV_LS_TIMEOUT);
}
