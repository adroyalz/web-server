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
    int ssthresh = SSTHRESH;
    int dupCount = 0;
    bool inFastRecovery = false;
    int fakeEnd = 0;

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
    //fakeEnd--; //after packet loss and cwnd reset, cwnd might be < current windowBuffer size
    while(!closeCon){  //valsent != -1 && 
        //add packets to window till we reach limit
        if(fakeEnd <= cwnd){
            cout << "\ncreating and sending packets: " << endl;
        }
        int tempcwnd = (int)cwnd;
        while(windowBuffer.size() <= tempcwnd && !last){ //while(fakeEnd <= tempcwnd && !last){
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
            fakeEnd++;
            //send the packet
            valsent = sendto(send_sockfd, &pkt, sizeof(pkt),  0, (const struct sockaddr *) &server_addr_to, sizeof(server_addr_to));
            //cout << "valsent: " << valsent << endl;
            indMostRecentPacketSent = pkt.seqnum;
            

            //FOR TESTING
            bytecount += valsent; 
            cout << pkt.seqnum << ", " << endl;
        }    
        if(cwnd <= ssthresh){
            cwnd++;
        }
        //TEMPORARY    
        cout << "\nnew window buffer: " << endl;
        for(int j=0; j<windowBuffer.size(); j++){ //for(int j=0; j<fakeEnd; j++){
            cout << windowBuffer[j].seqnum << " " << endl;
        }
        //TEMPORARY
        // if(last){
        //     break;
        // }
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
                std::cout << "timed out, last sent pkt: " << indMostRecentPacketSent << std::endl;
                for (int i = 0; i < windowBuffer.size(); i++){ //for (int i = 0; i < fakeEnd; i++){
                    if (windowBuffer[i].seqnum == lastPacketACKed+1){
                        printSend(&windowBuffer[i], 1);
                        sendto(send_sockfd, (void *)&windowBuffer[i], sizeof(windowBuffer[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    }
                }
                //timed out; reset cwnd and ssthresh
                ssthresh = max(cwnd/2, 2.0);
                cwnd = 1;
                //fakeEnd = 1;

                continue;
            }
        }
        else{ //received a packet (an ACK)
            std::cout << "received ACK pkt acking seqnum: " << ack_pkt.acknum << std::endl;
            if(ack_pkt.acknum >= expected_ack_num){ //cumulative ACK
                std::cout << "received ACK pkt (cumulatively) acking seqnum: " << ack_pkt.acknum << std::endl; //if expecting ack5 but get ack7, we know ack5, 6 were lost but server got packets 5,6 because server will only ack7 after it has sent ack5 and 6
                expected_ack_num=ack_pkt.acknum+1;
                if(inFastRecovery){
                    cwnd = ssthresh;
                    //fakeEnd = cwnd;
                    inFastRecovery = false;
                }
                if(cwnd <= ssthresh){
                    cwnd++;
                    std::cout << "new cwnd size: " << cwnd << std::endl;
                }
                else{ //cwnd>ssthresh, congestion avoidance
                int floor = (int)cwnd;
                double temp = (double)floor;
                    cwnd += 1/temp;
                }
                dupCount = 0;
                vector<packet>::iterator i = windowBuffer.begin();
                while(i!=windowBuffer.end() && fakeEnd != 0){// && fakeEnd != -1){
                    if(i->seqnum <= ack_pkt.acknum){
                        std::cout << "marking as ACKED - seq num " << i->seqnum << std::endl;
                        lastPacketACKed = i->seqnum;
                        i = windowBuffer.erase(i); //windowBuffer.erase(i);
                        fakeEnd--;
                    }
                    else{
                        i++;
                    }
                }
            }
            else if(ack_pkt.acknum < expected_ack_num ){ 
                std::cout << "------received ACK pkt with acknum: " << ack_pkt.acknum << " but expected acknum: " << expected_ack_num << std::endl;
                //so resend that packet
                for (int i = 0; i < windowBuffer.size(); i++) //for (int i = 0; i < fakeEnd; i++)
                {
                    if (windowBuffer[i].seqnum == lastPacketACKed+1)
                    {
                        printSend(&windowBuffer[i], 1);
                        sendto(send_sockfd, (void *)&windowBuffer[i], sizeof(windowBuffer[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    }
                }
                //getting to this else if means we lost a packet
                if(ack_pkt.acknum == lastPacketACKed){
                    dupCount++;
                }
                else{
                    std::cout << "impossible else, ack_pkt.acknum: " << ack_pkt.acknum << " and lastPacketACKed: " << lastPacketACKed  << std::endl;
                    dupCount = 0;
                }
                if(dupCount == 3){ //fast retransmit
                    std::cout << "received 3rd dup ACK, new cwnd size: " << cwnd << std::endl;
                    ssthresh = max(cwnd/2, 2.0);
                    cwnd = ssthresh + 3;
                    for (int i = 0; i < windowBuffer.size(); i++){ // for (int i = 0; i < fakeEnd; i++){
                        if (windowBuffer[i].seqnum == lastPacketACKed+1){
                            printSend(&windowBuffer[i], 1);
                            sendto(send_sockfd, (void *)&windowBuffer[i], sizeof(windowBuffer[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                        }
                    }
                }
                else if(dupCount > 3){ //fast recovery
                    inFastRecovery = true;
                    cwnd++;
                }
                else{
                    std::cout << "received " << dupCount << " dup ACK" << std::endl;
                }
                
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

