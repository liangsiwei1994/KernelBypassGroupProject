// Server side implementation of UDP client-server model
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <string>

using namespace std;

#define SERVER_IP "10.1.0.3"
#define SERVER_PORT 8080
#define DPDK_IP "10.1.0.2"
#define DPDK_PORT 8080
#define MAXLINE 1024
#define INTERFACE "veth2"

// Driver code
int main() {
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
       
    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    char interface[MAXLINE];
    strcpy(interface, INTERFACE);

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr.s_addr);

    // Filling client information
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(DPDK_PORT);
    inet_pton(AF_INET, DPDK_IP, &cliaddr.sin_addr.s_addr); // DPDK App IP
       
    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, 
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //Bind socket to interface to send traffic through
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, sizeof(interface)) != 0) {  
      perror("bind to interface failed");
      exit(EXIT_FAILURE);
    }
       
    socklen_t len;
    int n;
   
    len = sizeof(cliaddr);  //len is value/result
    
    while(true)
    {
        // Receive from dpdk app
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, 
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    &len);
	    
        buffer[n] = '\0';

        // Split merge packets and answers queries
        string received = buffer;
        string delimiter = "|";
        string send;
        size_t found = received.find(delimiter);

        while (found != string::npos) {
            send += received.substr(0, found);
            send += "a|";
            received.erase(0, found+1);
            found = received.find(delimiter);
        }

        printf("DPDK message : %s\n", buffer);

        // Reply from server
        char send_buf[MAXLINE*2];
        unsigned int idx;
        for (idx=0; idx < send.length(); idx++) {
            send_buf[idx] = send[idx];
        }
        send_buf[idx] = '\0';
	    printf("Sending out: %s\n", send_buf);
        sendto(sockfd, (const char *)send_buf, strlen(send_buf),
	     MSG_CONFIRM, (const struct sockaddr *) &cliaddr, 
	     sizeof(cliaddr));
    }
    return 0;
}
