#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include <iostream>
#include <vector>

using namespace std;

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
    int windowSize = SSTHRESH;
    vector<packet> windowBuffer;
    int validWindowIndex[windowSize] = {0};
    bool outOfOrder = false;
    int numToAck = 0;
    bool closeCon = false;
    int finalSeqNum = 0;
    bool stillOutOfOrder = false;
    char tempload[PAYLOAD_SIZE];

    while (!closeCon){ //valread != -1
        valread = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) &server_addr, (socklen_t *)(sizeof(server_addr)));
        ack_pkt.seqnum = buffer.seqnum;
        cout << "received packet " << buffer.seqnum << endl;
        if(buffer.seqnum > 2000){
            cout << "contents: " << buffer.payload << endl;
        }
        
        //ack_pkt.ack = 1; //add this in? shouldnt change anything

        if(buffer.seqnum > expected_seq_num){ //out of order packet; keep ACKing last packet we already ACKed (!= or >?)
            //only store pkt in buffer if > expected seq num
            //std::cout << "stored seq num: " << buffer.seqnum << " in buffer, expected seq num: " << expected_seq_num << std::endl;

            //sort windowBuffer by seqnum
            // sort(windowBuffer, validWindowIndex, windowSize);
            lastSeqnumAcked = buffer.seqnum;
            windowBuffer.push_back(buffer);
            outOfOrder = true;
            //std::cout << "OUT OF ORDER PACKET! (seq num: " << buffer.seqnum << "), instead ACKing seq num: " << lastSeqnumAcked << std::endl;
            //continue;
        }
        else if(buffer.seqnum == expected_seq_num){ //in-order packet: accept and ACK
            // buffer.payload[buffer.length] = '\0';
            if(buffer.last){
                build_packet(&ack_pkt, buffer.seqnum, buffer.seqnum, 0, 0, 0, "");
                valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
                closeCon = true;
                std::cout << "received last packet" << std::endl;
                continue;
            }
            //NEW: remove this line eventually?
            outOfOrder = windowBuffer.size();
            if(outOfOrder){ //if (out of order) packets are buffered, print them after
                //std::cout << "expected seqnum: " << expected_seq_num << " and printing packet: " << buffer.seqnum << std::endl;
                fwrite(buffer.payload, buffer.length, 1, fp);

               // std::cout << "\n\n-----------received pkt payload: " << buffer.payload << std::endl;
                numToAck = buffer.seqnum;
                expected_seq_num++;

                while(outOfOrder){
                    if(windowBuffer.begin()->seqnum > expected_seq_num){
                        //we have a gap between received pkt and buffered packets, dont print the buffered ones yet
                        break;
                    }
                    else if(windowBuffer.begin()->seqnum < expected_seq_num){
                        //still storing an old packet, remove it from buffer
                        windowBuffer.erase(windowBuffer.begin());
                        continue;
                    }
                    else{
                        if(windowBuffer.begin()->last){ //last packet => mark closeCon as true  TODO
                            ack_pkt.acknum = windowBuffer.begin()->seqnum; // should be == expected_seq_num 
                            sendto(send_sockfd, (void *)&ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
                            fclose(fp);
                            close(listen_sockfd);
                            close(send_sockfd);
                            return 0;
                        }
                        fwrite(windowBuffer.begin()->payload, windowBuffer.begin()->length, 1, fp);
                        expected_seq_num = windowBuffer.begin()->seqnum + 1;
                        windowBuffer.erase(windowBuffer.begin());
                    }
                    outOfOrder = windowBuffer.size();
                }
                ack_pkt.acknum = expected_seq_num-1;
                lastSeqnumAcked = ack_pkt.acknum;
                // outOfOrder = stillOutOfOrder;
                //std::cout << "expected_seq_num: " << expected_seq_num << std::endl;
                //outOfOrder = false;
                //cumulatively ACK the highest buffered packet (or last packet we got without a gap before it)
                // lastSeqnumAcked = numToAck;

                // seq_num++; //is this right? does it even matter for ack pkts?
                //std::cout << "cumulatively ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;

                //if the final packet was buffered and printed, then mark closeCon as true
                //under an ideal pipeline the final pkt would have come in order and closeCon would have been made true at the top of this else if
                // if(lastSeqnumAcked == finalSeqNum){
                //     closeCon = true;
                // }
            }
            else{ //nothing is buffered
                //std::cout << "in order, printing packet: " << buffer.seqnum << std::endl;
                fwrite(buffer.payload, buffer.length, 1, fp);
                //std::cout << "received packet: " << buffer.seqnum << std::endl;
                ack_pkt.acknum = buffer.seqnum;
                lastSeqnumAcked = ack_pkt.acknum;
                expected_seq_num++;
                //std::cout << "ACKed: " << lastSeqnumAcked << ", new expected_seq_num: " << expected_seq_num << std::endl;
            }
        }
        std::cout << "sending ACK for packet: " << ack_pkt.acknum << std::endl;
        valsent = sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &client_addr_to, sizeof(client_addr_to));
        //printRecv(&buffer);
        // if(buffer.last){
        //     std::cout << "received last packet" << std::endl;
        //     break;
        // }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
