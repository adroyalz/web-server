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

using namespace std;

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

    tv = {1, 500000};
    if(setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
        perror("error setting timeout");
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
    
    vector<packet> windowBuffer;
    int expected_ack_num = 1;
    int last_seq_num = 0;
    bool closeCon = false;
    bool full = false;
    int useThisIndex = 0;
    int tempSeqNum = 0; //+1
    int indMostRecentPacketSent = 0;
    int read = 0;
    int lastPacketACKed = 0; //seqnum of that packet
    double cwnd = WINDOW_SIZE;

    // for(int i=0; i<cwnd; i++){
    //     read = fread(buffer, 1, sizeof(buffer), fp);
    //     if(read <= 0){
    //         //done sending, send close con packet
    //     }
    //     else{
    //         //put it into a packet payload and put the packet into the window
    //         build_packet(&pkt, seq_num, 0, 0, 0, read, buffer);
    //         windowBuffer.push_back(pkt);
    //         sendto(send_sockfd, &pkt, sizeof(pkt),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
    //         seq_num++;
    //     }

    // }

    while(!closeCon){  //valsent != -1 && 
        //add packets to window till we reach limit
        if(windowBuffer.size() <= cwnd){
            cout << "\ncreating and sending packets: " << endl;
        }
        while(windowBuffer.size() <= cwnd){
            read = fread(buffer, 1, sizeof(buffer), fp);
            if(read <= 0){
                //done reasing
                last = '1';
                seq_num++;
                ack_num = 0;
                ack=0;
                // //build_packet(&pkt, seq_num, ack_num, last, ack, read, buffer);
                build_packet(&pkt, seq_num, ack_num, last, ack, 0, "");
                cout << "last packet built, setting last_seq_num to: " << seq_num << endl;
                last_seq_num = seq_num;
                windowBuffer.push_back(pkt);
                //send the packet
                valsent = sendto(send_sockfd, &pkt, sizeof(pkt),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //cout << "valsent: " << valsent << endl;
                indMostRecentPacketSent = pkt.seqnum;
                break;
            }
            //put it into a packet payload and put the packet into the window
            seq_num++;
            build_packet(&pkt, seq_num, 0, 0, 0, read, buffer);
            windowBuffer.push_back(pkt);
            //send the packet
            valsent = sendto(send_sockfd, &pkt, sizeof(pkt),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
            //cout << "valsent: " << valsent << endl;
            indMostRecentPacketSent = pkt.seqnum;
            

            //FOR TESTING
            bytecount += valsent; 
            cout << pkt.seqnum << ", " << endl;
        }    
        //TEMPORARY    
        cout << "\nnew window buffer: " << endl;
        for(int j=0; j<windowBuffer.size(); j++){
            cout << windowBuffer[j].seqnum << " " << endl;
        }
        //TEMPORARY
        if(last){
            break;
        }
        // else{
        //     //clear buffer and continue
        //     while(windowBuffer.size()){
        //         windowBuffer.erase(windowBuffer.begin());
        //     }
        //     continue;
        // }

        std::cout << "listening for acks" << std::endl;
        valread = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, NULL, NULL);
        if(valread == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                //std::cout << "ack_pkt ACKed seq num: " << ack_pkt.acknum << std::endl;
                //resend last sent packet --> maybe remove this later? and just rely on server also timing out and resending ACK
                // for(int j=0; j<WINDOW_SIZE; j++){
                //     if(windowBuffer[j].seqnum == indMostRecentPacketSent){
                //         valsent = sendto(send_sockfd, &windowBuffer[j], sizeof(windowBuffer[j]), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //     }
                // }
                //std::cout << "buffer: " << ack_pkt.acknum << std::endl;
                std::cout << "timed out, last sent pkt: " << indMostRecentPacketSent << std::endl;
                // valsent = sendto(send_sockfd, &windowBuffer.begin(), sizeof(windowBuffer.begin()),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                // std::cout << "resent packet " << windowBuffer.begin()->seqnum << std::endl;
                // for(vector<packet>::iterator i=windowBuffer.begin(); i != windowBuffer.end(); i++){
                //     if(i->seqnum == lastPacketACKed+1){
                //         std::cout << "resent packet " << i->seqnum << std::endl;
                //          std::cout << "contents: " << i->payload << std::endl;
                //         valsent = sendto(send_sockfd, &i, sizeof(i),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //     }   
                // }
                for (int i = 0; i < windowBuffer.size(); i++)
                {
                    if (windowBuffer[i].seqnum == lastPacketACKed+1)
                    {
                        printSend(&windowBuffer[i], 1);
                        sendto(send_sockfd, (void *)&windowBuffer[i], sizeof(windowBuffer[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    }
                }
                continue;
            }
        }
        else{ //received a packet (an ACK)
            std::cout << "received ACK pkt acking seqnum: " << ack_pkt.acknum << std::endl;
            if(ack_pkt.acknum >= expected_ack_num){ //cumulative ACK
                std::cout << "received ACK pkt (cumulatively) acking seqnum: " << ack_pkt.acknum << std::endl; //if expecting ack5 but get ack7, we know ack5, 6 were lost but server got packets 5,6 because server will only ack7 after it has sent ack5 and 6
                expected_ack_num=ack_pkt.acknum+1;
                vector<packet>::iterator i = windowBuffer.begin();
                while(i!=windowBuffer.end()){
                    if(i->seqnum <= ack_pkt.acknum){
                        std::cout << "marking as ACKED - seq num " << i->seqnum << std::endl;
                        lastPacketACKed = i->seqnum;
                        windowBuffer.erase(i);
                    }
                    else{
                        i++;
                    }
                }
            }
            else if(ack_pkt.acknum < expected_ack_num ){ 
                std::cout << "------received ACK pkt with acknum: " << ack_pkt.acknum << " but expected acknum: " << expected_ack_num << std::endl;
                //so resend that packet
                // for(vector<packet>::iterator i=windowBuffer.begin(); i != windowBuffer.end(); i++){
                //     if(i->seqnum == lastPacketACKed+1){
                //         std::cout << "resent packet " << i->seqnum << std::endl;
                //         //std::cout << "for reference, lastPacketACKed+1: " << lastPacketACKed+1 << " expected ack num: " << expected_ack_num << std::endl;
                //         valsent = sendto(send_sockfd, &i, sizeof(i),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //         break;
                //     }   
                // }
                for (int i = 0; i < windowBuffer.size(); i++)
                {
                    if (windowBuffer[i].seqnum == lastPacketACKed+1)
                    {
                        printSend(&windowBuffer[i], 1);
                        sendto(send_sockfd, (void *)&windowBuffer[i], sizeof(windowBuffer[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    }
                }
                // for(int h=0; h<windowBuffer.size(); h++){
                //     if(windowBuffer[h].seqnum == expected_ack_num){ //find packet in window that needs to be resent
                //         std::cout << "resending packet with seqnum: " << windowBuffer[h].seqnum << std::endl;
                //         valsent = sendto(send_sockfd, &windowBuffer[h], sizeof(windowBuffer[h]), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                //     }
                // }
            }
            else{
                std::cout << "Something went wrong with packet ACKing seq num: " << ack_pkt.acknum << " under expected_ack_num: " << expected_ack_num << std::endl;
            }     
            if(ack_pkt.acknum == last_seq_num){ //received ACK for last packet --> maybe change this later to received ACK for tcp close down
                closeCon = true;
            }   
        }
    }

    //send last packet

    std::cout << "\nwrote: " << std::to_string(bytecount) << " bytes" << std::endl;
    valsent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
                    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

