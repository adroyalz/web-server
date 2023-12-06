#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include <string>
#include <vector>
#include <iostream>

void fillBuffer(int lastIndex, char *buffer){
    if(lastIndex < PAYLOAD_SIZE-1){
        for(int j=lastIndex; j<PAYLOAD_SIZE; j++){
            buffer[j] = '>';
        }
    }
}


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv_start, tv_now;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    std::string file_content = "";

    int valsent = 0;
    int valread = 0;
    int bytecount = 0;
    int lastIndex = 0;
    
    packet windowBuffer[WINDOW_SIZE];
    int ackedPkts[WINDOW_SIZE];
    for(int i=0; i<WINDOW_SIZE; i++){
        ackedPkts[i] = 1;
    }
    int expected_ack_num = 1;
    int last_seq_num = 0;
    bool closeCon = false;
    bool full = false;
    int useThisIndex = 0;
    int tempSeqNum = 0; //+1

    while(valsent != -1 && !closeCon){
        //add packets to window until we fill the window
        while(!full && !last){
            for(int i=0; i<WINDOW_SIZE+1; i++){
                if(i == WINDOW_SIZE){ //did not find a spot to put the packet in
                   // std::cout << "FULL; " << std::endl;
                    full = true;
                }
                if(ackedPkts[i] == 1){  //if that packet has been acked, replace it with a new one
                    useThisIndex = i;
                    full = false;
                    break;
                }
            }
            if(full){
                break;
            }
            for(int i=0; i<PAYLOAD_SIZE; i++){
                int c = getc(fp);
                lastIndex = i; //TODO: still need to use this --> maybe make the elements after index i in buffer some known empty value?
                if(c == EOF){
                    last = '1';
                    break;
                }
                buffer[i] = c;
            }
            fillBuffer(lastIndex, buffer);
            ack=0;
            ack_num = 0;
            seq_num++;
            build_packet(&pkt, seq_num, ack_num, last, ack, sizeof(buffer)/sizeof(char), buffer);
            windowBuffer[useThisIndex] = pkt;
            ackedPkts[useThisIndex] = 0; //change this to 1 when the respective windowbuffer packet is acked

            valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
            std::cout << "sent pkt: " << seq_num << std::endl;
            bytecount += valsent;
            if(last){
                last_seq_num = seq_num;
            }
            std::cout << "new window buffer: " << std::endl;
            for(int j=0; j<WINDOW_SIZE; j++){
                std::cout << windowBuffer[j].seqnum << " " << std::endl;
            }
        }
        //send packets in window that have not yet been sent (upon receiving ACK?)
        // windowBuffer[tempSeqNum].ack = 1;
        // ackedPkts[tempSeqNum] = 1;
        // full = false;
        // tempSeqNum++;
        // if(tempSeqNum >= WINDOW_SIZE){
        //     tempSeqNum = 0;
        // }

        std::cout << "listening for acks" << std::endl;
        valread = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *) &client_addr, (socklen_t *)sizeof(client_addr));
        if(ack_pkt.ack == 0){ //check ack bit
            std::cout << "ack==0 ack pkt received" << std::endl;
        }
        else if(ack_pkt.acknum == last_seq_num){ //received ACK for last packet --> maybe change this later to received ACK for tcp close down
            closeCon = true;
        }
        else if(ack_pkt.acknum >= expected_ack_num){ //cumulative ACK
            std::cout << "received ACK pkt (cumulatively) acking seqnum: " << ack_pkt.acknum << std::endl; //if expecting ack5 but get ack7, we know ack5, 6 were lost but server got packets 5,6 because server will only ack7 after it has sent ack5 and 6
            expected_ack_num=ack_pkt.acknum+1;
            for(int i=0; i<WINDOW_SIZE; i++){ //find which packets in window were ACKed and mark them as acked
                if(windowBuffer[i].seqnum <= ack_pkt.acknum){
                    std::cout << "acking seq num " << windowBuffer[i].seqnum << std::endl;
                    windowBuffer[i].ack = 1;
                    ackedPkts[i] = 1;
                    full = false;
                }
            }
        }
        else if(ack_pkt.acknum < expected_ack_num ){ 
            std::cout << "------received ACK pkt with acknum: " << ack_pkt.acknum << " but expected acknum: " << expected_ack_num << std::endl;
            //so resend that packet
            for(int h=0; h<WINDOW_SIZE; h++){
                if(windowBuffer[h].seqnum == expected_ack_num){ //find packet in window that needs to be resent
                    std::cout << "resending packet with seqnum: " << windowBuffer[h].seqnum << std::endl;
                    valsent = sendto(send_sockfd, &windowBuffer[h], sizeof(windowBuffer[h]), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                }
            }
        }
        else{
            std::cout << "Something went wrong with packet ACKing seq num: " << ack_pkt.acknum << " under expected_ack_num: " << expected_ack_num << std::endl;
        }        
    }
    std::cout << "\nwrote: " << std::to_string(bytecount) << " bytes" << std::endl;
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

