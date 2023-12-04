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
    bool reachedEnd = false;
    int lastIndex = 0;
    bool resending = false;
    bool noneed = false;
    packet windowBuffer[WINDOW_SIZE];
    int ackedPkts[WINDOW_SIZE];
    for(int i=0; i<WINDOW_SIZE; i++){
        ackedPkts[i] = 0;
    }
    int expected_ack_num = 1;

    while(valsent != -1 && !reachedEnd){
        if(!resending){
            for(int i=0; i<PAYLOAD_SIZE; i++){
                int c = getc(fp);
                lastIndex = i; //TODO: still need to use this --> maybe make the elements after index i in buffer some known empty value?
                if(c == EOF){
                    last = '1';
                    reachedEnd = true;
                    break;
                }
                buffer[i] = c;
            }
            if(lastIndex < PAYLOAD_SIZE-1){
                for(int j=lastIndex; j<PAYLOAD_SIZE; j++){
                    buffer[j] = '>';
                }
            }
            ack=0;
            ack_num = 0;
            seq_num++;
            //expected_ack_num++; //only increase this after getting an ack for a packet?
            // if(seq_num == 1000){
            //     seq_num++;
            // }
            build_packet(&pkt, seq_num, ack_num, last, ack, sizeof(buffer)/sizeof(char), buffer);
            valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
            std::cout << "sent pkt" << std::endl;
            bytecount += valsent;

            windowBuffer[(seq_num%WINDOW_SIZE + (WINDOW_SIZE-1))%WINDOW_SIZE] = pkt;
            if(seq_num==10){
                for(int z=0; z<WINDOW_SIZE; z++){
                    std::cout << "Packet " << windowBuffer[z].seqnum << " in window buffer" << std::endl;    
                }
                return 0;
            }
        }
        else{ //resend packet (until get ack?)  
            while(valread == -1){
                //{ //do this every x seconds?
                    std::cout << "resending packet with seqnum: " << pkt.seqnum << std::endl;
                    valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //}
                valread = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *) &client_addr, (socklen_t *)sizeof(client_addr));
                
                if(ack_pkt.ack==1 && ack_pkt.acknum == pkt.seqnum){ //if we read something, check its an ACK with correct params
                    std::cout << "received pkt ACKing seqnum: " << ack_pkt.acknum << std::endl;
                    std::cout << "successfully resent packet" << std::endl;
                    resending = false;
                    noneed = true; //dont re listen for this ack at the end of the function
                    break;
                }
            }
        }
        if(noneed){
            noneed = false;
            continue;
        }
        if(seq_num%WINDOW_SIZE != 0){ //only check for acks after sending the entire window (gobackn n=window_size)
            continue;
        }
        std::cout << "listening for acks" << std::endl;
        bool windowIsGood = false;
        while(!windowIsGood){
            valread = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *) &client_addr, (socklen_t *)sizeof(client_addr));
            if(ack_pkt.ack == 0){
                std::cout << "ack==0 ack pkt received" << std::endl;
                resending = true;
            }
            else if(ack_pkt.acknum == expected_ack_num){
                std::cout << "received ACK pkt acking seqnum: " << ack_pkt.acknum << std::endl;
                expected_ack_num++;
                ackedPkts[(ack_pkt.acknum%WINDOW_SIZE + (WINDOW_SIZE-1))%WINDOW_SIZE] = 1;
                //can move window over by 1?
                //TEMPORARY: if received ack for last packet in window, reset ackedPkts (and then the window moves right 5 at once?)
                if(ack_pkt.acknum%WINDOW_SIZE == 0){
                    for(int i=0; i<WINDOW_SIZE; i++){
                        ackedPkts[i] = 0;
                        windowIsGood = true;
                    }
                }
            }
            else if(ack_pkt.acknum != expected_ack_num && ackedPkts[(ack_pkt.acknum%WINDOW_SIZE + (WINDOW_SIZE-1))%WINDOW_SIZE]==0){ //note: second condition skips this loop if the ack num was not expected, but was previously received when it was expected (ie duplicate ACK)
                std::cout << "received ACK pkt with acknum: " << ack_pkt.acknum << " but expected acknum: " << expected_ack_num << std::endl;
                //for now ignoring the case that received acknum > expected_ack_num, is this even a possible case?
                //assuming ack_pkt.acknum < expected_ack_num, that means packets>ack_pkt.acknum in window were not received correctly
                //so resend the entire window
                for(int h=0; h<WINDOW_SIZE; h++){
                    std::cout << "resending packet with seqnum: " << windowBuffer[h].seqnum << std::endl;
                    valsent = sendto(send_sockfd, &windowBuffer[h], sizeof(windowBuffer[h]), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                }
            }
        }
        
    }
    std::cout << "\nwrote: " << std::to_string(bytecount) << " bytes" << std::endl;
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

