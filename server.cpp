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
    int expected_seq_num = 0;
    int seq_num = 0;
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

    while (true){ //valread != -1
        expected_seq_num++;
        // std::cout << expected_seq_num << std::endl;
        valread = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &server_addr, (socklen_t *)(sizeof(server_addr)));
        //std::cout << "here " << buffer.payload << std::endl;
        // if(valread == -1){
        //     std::cout << "no packet received" << std::endl;
        //     continue;
        // }
        // std::cout << "packet received" << std::endl;
        if(buffer.seqnum != expected_seq_num){ //out of order packet; reject for now
            //check if receiving a pkt we already ACKed
            if(buffer.seqnum == lastSeqnumAcked){
                //then ACK didnt send so resend it
                std::cout << "resending ACK for pkt with seq num: " << buffer.seqnum << std::endl;
                ack = 1;
                build_packet(&ack_pkt, seq_num, buffer.seqnum, 0, ack, 0, NULL);
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
            }
            // std::cout << "PACKET DID NOT HAVE EXPECTED SEQ NUM!" << std::endl;
            // std::cout << "expected: " << expected_seq_num << " but read: " << buffer.seqnum << std::endl;
            //send ack with ack=0?
            
            ack = 0;
            build_packet(&ack_pkt, seq_num, buffer.seqnum, 0, ack, 0, NULL);
            valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));;
            expected_seq_num--;
            continue;
        }
        else{
            
            //seq_num++;
            //ACK the packet
            ack = 1;
            build_packet(&ack_pkt, seq_num, buffer.seqnum, 0, ack, 0, NULL);
            valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
            lastSeqnumAcked = buffer.seqnum;
        }
        if(buffer.last){
            std::cout << "here2" << std::endl;
            break;
        }
        //std::cout << buffer.payload;
        printRecv(&buffer);
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
