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
    std::vector<char> tempResponse;
    while((c=getc(fp)) != EOF){
        file_content += c;
    }
    for(int i=0; i<file_content.size(); i++){
        tempResponse.push_back(file_content[i]);
    }
    //char response[tempResponse.size() + 1];
    char response[256];
    //for(int i=0; i<tempResponse.size(); i++){
    for(int i=0; i<255; i++){
        response[i] = tempResponse[i];
        std::cout << response[i];
    }
    //response[tempResponse.size()] = '\0';
    response[255] = '\0';


    // build_packet(&ack_pkt, 1, 2, 'e', '1', 15, "hello world");
    // std::cout << ack_pkt.length << " " << sizeof(ack_pkt)/sizeof(char) << std::endl;

    std::cout << "writing, to port: " << htons(server_addr_to.sin_port) << std::endl;
    int valsent = sendto(send_sockfd, response, sizeof(response), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
    
    if(valsent<0){
        std::cout << "cout: error writing" << std::endl;
    }
    std::cout << "wrote: " << std::to_string(sizeof(response)) << " bytes" << std::endl;

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

