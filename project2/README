# COMP 556：Project 2

<p align="center">Ruiqi Kuang (rk85) , Jingze Chen (jc208) ,</p> 
<p align="center">Ziang Tang (zt23) ,  Ziyi Zhao (zz94) </p>

## 

## Project Outline

In this project, we write our own reliable transport code based on SOCK DGRAM, the UDP protocol with the stop-and-wait method.

On the sending end, based on the four parameters received, the program opens and reads the file, divides it into blocks of size DATA_SIZE, constructs the UDP packet, adds CRC checksum and sends it to the receiver. The program also waits for the reception confirmation packet and retransmits the packet within the timeout period, if it reaches the end of the file, the program stops sending the packet.

On the receiving end, the received data is checked, incorrect packets and previously received packets are ignored, and an acknowledgement packet is sent after the correct received packet, until the sequence number of the received packet is -1. When receiving packets, a timeout of 100 seconds is set, and if no packet is received within 100 seconds, the receiving end exits the program.

## Packet Formats

The packet consists of three main parts, a header (64 bytes), including: 00000000 + Sequence number (1 byte) + data length (2 bytes) + sub-direction name (40 bytes) + filename (20 bytes), data (DATA_SIZE = 20000 bytes) and a CRC check code (CRC_SIZE = 4 bytes). Besides, the ack packet is also defined. (ACK_SIZE = 3)

## Timeout mechanism

The timeout mechanism is implemented by an adaptive algorithm. We set the SO_RCVTIMEO socket option and initialize the timeout to 10s and offset to 1s. When the program sends a packet, a timer is started to record the time it took to send the packet. If an ACK is received within the timeout period, the timeout is updated and the new timeout value is set to the RTT plus offset. The program increases seq_num by 1 after receiving the ACK and then starts sending the next packet. If no ACK is received within the timeout period, the program adds offset to the timeout, indicating a longer timeout for the next packet to be sent. 

## Testing

We tested our code on cai with all network disturbances, including Delay, Drop, Reorder, Mangle and Duplicate. We set several different values for each of the different disturbance conditions to test, and the results show that our code produces the correct output under any of the above conditions, i.e., the receiver side can receive the packet correctly. In terms of actual performance, when we set the interference to 20% and transmit 30M packets, we often take a long time (about half an hour) to receive all packets correctly.
