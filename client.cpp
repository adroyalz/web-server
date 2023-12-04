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
    struct timeval tv;
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
    int c;
    std::string file_content = "";

    // build_packet(&ack_pkt, 1, 2, 'e', '1', 15, "hello world");
    // std::cout << ack_pkt.length << " " << sizeof(ack_pkt)/sizeof(char) << std::endl;

    //std::cout << "writing, to port: " << htons(server_addr_to.sin_port) << std::endl;
    int valsent = 0;
    int bytecount = 0;
    bool reachedEnd = false;
    int responseLength = 0;

    while(valsent != -1 && !reachedEnd){
        char response[256] = {};
        for(int i=0; i<sizeof(response); i++){
            int c = getc(fp);
            responseLength = i;
            if(c == EOF){
                reachedEnd = true;
                break;
            }
            response[i] = c;
        }
        build_packet(&pkt, 1, 1, NULL, 'n', sizeof(response), response);
        std::cout << pkt.payload;
        valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
        //valsent = sendto(send_sockfd, response, sizeof(response), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
        bytecount += valsent;
    }
    std::cout << "\nwrote: " << std::to_string(bytecount) << " bytes" << std::endl;
    build_packet(&pkt, 1, 1, NULL, 'y', 0, NULL);
    valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

