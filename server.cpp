#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include <iostream>

void sort(packet windowBuffer[], int validWindowIndex[], int arraySize){
    for(int a=0; a<arraySize; a++){
        if(validWindowIndex[a] == 0){
            continue;
        }
        int smallest = a;
        for(int b=a; b<arraySize; b++){
            if(validWindowIndex[b] == 0){
                continue;
            }
            if(windowBuffer[b].seqnum < windowBuffer[smallest].seqnum){
                //std::cout << windowBuffer[b].seqnum << " is smaller than (" << b << ", " << smallest << ") " << windowBuffer[smallest].seqnum << std::endl;
                smallest = b;
            } 
        }
        //std::cout << "switching: " << std::endl;
        packet temp = windowBuffer[smallest];
        windowBuffer[smallest] = windowBuffer[a];
        windowBuffer[a] = temp;
    }
    //std::cout << "sorted array: " << std::endl;
    // for(int c=0; c<arraySize; c++){
    //     if(validWindowIndex[c] == 1){
    //         std::cout << windowBuffer[c].seqnum << std::endl;
    //     }
    // }
}

void cleanBuffer(packet buf){
    std::cout << "examining payload: " << buf.payload << std::endl;
    for(int i=0; i<buf.length; i++){
        if(buf.payload[i] < 32 || buf.payload[i] > 126){
            std::cout << "\n\ndetected non alphanum char: " << buf.payload[i] << std::endl;
            buf.payload[i] = 0;
            // for(int j=i-10; j<buf.length && j<i+15; j++){
            //     std::cout << buf.payload[j];
            // }
        }
    }
}

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
    int lastSeqnumAcked = 0; //deal with pkt1 dropped => so ack pkt 0
    packet windowBuffer[WINDOW_SIZE]; //make this max window size? 
    int validWindowIndex[WINDOW_SIZE] = {0};
    bool outOfOrder = false;
    int numToAck = 0;
    bool closeCon = false;
    int finalSeqNum = 0;
    bool stillOutOfOrder = false;
    char tempload[PAYLOAD_SIZE];

    while (!closeCon){ //valread != -1
        valread = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &server_addr, (socklen_t *)(sizeof(server_addr)));

        cleanBuffer(buffer);

        if(buffer.seqnum != expected_seq_num){ //out of order packet; keep ACKing last packet we already ACKed (!= or >?)
            //only store pkt in buffer if > expected seq num
            if(buffer.seqnum > expected_seq_num){
                buffer.payload[buffer.length] = '\0';
                if(buffer.last){
                    finalSeqNum = buffer.seqnum;
                    for(int ind=0; ind<buffer.length; ind++){
                        if(buffer.payload[ind] == '>'){
                            buffer.payload[ind] = '\0';
                            break;
                        }
                    }
                }
                //keep windowBuffer ordered by seq num?
                int tempInd = -1; //buffer.seqnum - (expected_seq_num+1);
                for(int c=0; c<WINDOW_SIZE; c++){
                    if(validWindowIndex[c] == 0){
                        tempInd = c;
                    }
                }
                if(tempInd == -1){
                    std::cout << "COULD NOT FIND SPACE TO BUFFER PACKET " << buffer.seqnum << std::endl;
                }
                else{
                    windowBuffer[tempInd] = buffer;
                    validWindowIndex[tempInd] = 1;
                    
                    outOfOrder = true;
                    //std::cout << "stored seq num: " << buffer.seqnum << " in buffer, expected seq num: " << expected_seq_num << std::endl;

                    //sort windowBuffer by seqnum (lol change this to mergesort?)
                    sort(windowBuffer, validWindowIndex, WINDOW_SIZE);
                    
                }
            }
            ack = 1;
            build_packet(&ack_pkt, seq_num, lastSeqnumAcked, 0, ack, 0, NULL);
            valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));;
            //std::cout << "OUT OF ORDER PACKET! (seq num: " << buffer.seqnum << "), instead ACKing seq num: " << lastSeqnumAcked << std::endl;
            continue;
        }
        else if(buffer.seqnum == expected_seq_num){ //in-order packet: accept and ACK
            buffer.payload[buffer.length] = '\0';
            //buffer.payload[buffer.length-1] = '$';
            // buffer.payload[buffer.length-2] = '=';
            if(buffer.last){
                for(int ind=0; ind<buffer.length; ind++){
                    if(buffer.payload[ind] == '>'){
                        buffer.payload[ind] = '\0';
                        break;
                    }
                }
                closeCon = true;
            }
            if(outOfOrder){ //if (out of order) packets are buffered, print them after
                //std::cout << "expected seqnum: " << expected_seq_num << " and printing packet: " << buffer.seqnum << std::endl;
                fprintf(fp, buffer.payload);

               // std::cout << "\n\n-----------received pkt payload: " << buffer.payload << std::endl;

                numToAck = buffer.seqnum;
                expected_seq_num++;

                //TEMP FOR TESTING
                // std::cout << "in window buffer: " << std::endl;
                // for(int j=0; j<WINDOW_SIZE; j++){ 
                //     if(validWindowIndex[j] == 0){ //do not print buffered packets if they are old (already printed or NULL)
                //         continue;
                //     }
                //     std::cout << "packet: " << windowBuffer[j].seqnum << std::endl;
                //     std::cout << "\n\n payload: " << windowBuffer[j].payload << std::endl;
                // }


                for(int j=0; j<WINDOW_SIZE; j++){
                    if(validWindowIndex[j] == 0){ //do not print buffered packets if they are old (already printed or NULL)
                        continue;
                    }
                    if(windowBuffer[j].seqnum == expected_seq_num){
                        //std::cout << "\n\nprinting buffered packet: " << windowBuffer[j].seqnum << std::endl;
                        fprintf(fp, windowBuffer[j].payload);
                        //std::cout << "-----------buffered pkt payload: " << windowBuffer[j].payload << std::endl;
                        validWindowIndex[j] = 0;
                        numToAck = windowBuffer[j].seqnum;
                        expected_seq_num++;
                        stillOutOfOrder = false; //if every packet in buffer was in order, then stillOutOfOrder is false at the end of the for loop and outOfOrder is set to false
                    }
                    else{ //we are missing another packet that we thought was buffered (note: this works since window is sorted)
                        if(windowBuffer[j].seqnum != expected_seq_num){
                            //do not continue printing other buffered packets (because there is a gap between packets that needs to be fixed)
                            stillOutOfOrder = true;
                            break;
                        }
                    }
                }
                outOfOrder = stillOutOfOrder;
                //std::cout << "expected_seq_num: " << expected_seq_num << std::endl;
                //outOfOrder = false;
                //cumulatively ACK the highest buffered packet (or last packet we got without a gap before it)
                ack = 1;
                build_packet(&ack_pkt, seq_num, numToAck, 0, ack, 0, NULL);
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
                lastSeqnumAcked = numToAck;

                seq_num++; //is this right? does it even matter for ack pkts?
                //std::cout << "cumulatively ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;

                //if the final packet was buffered and printed, then mark closeCon as true
                //under an ideal pipeline the final pkt would have come in order and closeCon would have been made true at the top of this else if
                if(lastSeqnumAcked == finalSeqNum){
                    closeCon = true;
                }
            }
            else{
                //std::cout << "in order, printing packet: " << buffer.seqnum << std::endl;
                fprintf(fp, buffer.payload);
                //std::cout << "received packet: " << buffer.seqnum << std::endl;
                //ACK the packet
                ack = 1;
                build_packet(&ack_pkt, seq_num, buffer.seqnum, 0, ack, 0, NULL);
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
                lastSeqnumAcked = buffer.seqnum;
                expected_seq_num++;
                seq_num++;
                //std::cout << "ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;
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
