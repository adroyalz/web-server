#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include <iostream>

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 1;
    int seq_num = 1;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);
    

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    std::cout << "reading " << std::endl;
    int valread = 0;
    int valsent = 0;
    int ack=0;
    int lastSeqnumAcked = 0;
    packet windowBuffer[WINDOW_SIZE]; //make this max window size? 
    bool outOfOrder = false;
    int numToAck = 0;

    while (true){ //valread != -1
        valread = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &server_addr, (socklen_t *)(sizeof(server_addr)));
        if(buffer.seqnum != expected_seq_num){ //out of order packet; keep ACKing last packet we already ACKed
            buffer.payload[buffer.length] = '\0';
            if(buffer.last){
                for(int ind=0; ind<buffer.length; ind++){
                    if(buffer.payload[ind] == '>'){
                        buffer.payload[ind] = '\0';
                        break;
                    }
                }
            }
            windowBuffer[buffer.seqnum % WINDOW_SIZE] = buffer;
            outOfOrder = true;
            ack = 1;
            build_packet(&ack_pkt, seq_num, lastSeqnumAcked, 0, ack, 0, NULL);
            valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));;
            std::cout << "OUT OF ORDER PACKET! (seq num: " << buffer.seqnum << "), instead ACKing seq num: " << lastSeqnumAcked << std::endl;
            continue;
        }
        else if(buffer.seqnum == expected_seq_num){ //in-order packet: accept and ACK
            buffer.payload[buffer.length] = '\0';
            if(buffer.last){
                for(int ind=0; ind<buffer.length; ind++){
                    if(buffer.payload[ind] == '>'){
                        buffer.payload[ind] = '\0';
                        break;
                    }
                }
            }
            if(outOfOrder){ //if (out of order) packets are buffered, print them after
                fprintf(fp, buffer.payload);
                numToAck = buffer.seqnum;
                for(int j=0; j<WINDOW_SIZE; j++){
                    std::cout << "in window buffer, packet: " << windowBuffer[j].seqnum << " last acked packet: " << lastSeqnumAcked << std::endl;
                    if(windowBuffer[j].seqnum > buffer.seqnum){
                        std::cout << "printing buffered packet: " << windowBuffer[j].seqnum << std::endl;
                        fprintf(fp, buffer.payload);
                        numToAck = windowBuffer[j].seqnum;
                    }
                }
                outOfOrder = false;
                //cumulatively ACK the highest buffered packet
                ack = 1;
                build_packet(&ack_pkt, seq_num, numToAck, 0, ack, 0, NULL);
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
                lastSeqnumAcked = numToAck;
                expected_seq_num = numToAck+1; //change to just numToAck if following ack convention (ack7 on pkt6)
                seq_num++; //is this right? does it even matter for ack pkts?
                std::cout << "cumulatively ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;
            }
            else{
                fprintf(fp, buffer.payload);
                std::cout << "received packet: " << buffer.seqnum << std::endl;
                //ACK the packet
                ack = 1;
                build_packet(&ack_pkt, seq_num, buffer.seqnum, 0, ack, 0, NULL);
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
                lastSeqnumAcked = buffer.seqnum;
                expected_seq_num++;
                seq_num++;
                std::cout << "ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;
            }
        }
        //printRecv(&buffer);
        if(buffer.last){
            std::cout << "received last packet" << std::endl;
            break;
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
