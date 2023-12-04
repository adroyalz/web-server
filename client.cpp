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
                //std::cout << last << " " << PAYLOAD_SIZE-1 << " " << sizeof(buffer)/sizeof(char) << std::endl;
                for(int j=lastIndex; j<PAYLOAD_SIZE; j++){
                    buffer[j] = '>';
                }
            }
            ack=0;
            ack_num = 0;
            seq_num++;
            // if(seq_num == 1000){
            //     seq_num++;
            // }
            build_packet(&pkt, seq_num, ack_num, last, ack, sizeof(buffer), buffer);
            std::cout << pkt.payload;
            valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
            std::cout << "sent pkt" << std::endl;
            bytecount += valsent;
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
                    noneed = true;
                    break;
                }
            }
        }
        if(noneed){
            noneed = false;
            continue;
        }
        std::cout << "listening for ack" << std::endl;
        valread = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *) &client_addr, (socklen_t *)sizeof(client_addr));
        std::cout << "lsitened!! for ack" << std::endl;
        if(valread == -1){
            std::cout << "no ack received" << std::endl;
            resending = true;
        }
        else if(ack_pkt.ack == 0){
            resending = true;
        }
        else{
            printRecv(&ack_pkt);
        }
        
    }
    std::cout << "\nwrote: " << std::to_string(bytecount) << " bytes" << std::endl;
    // build_packet(&pkt, 1, 1, 0, 'y', 0, NULL);
    // valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

