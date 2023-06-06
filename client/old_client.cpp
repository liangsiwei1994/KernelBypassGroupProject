// Client side implementation of UDP client-middlebox communication
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <cstring>
   
#define PORT     8080
#define MAXLINE 1024
#define MIDDLEBOX_IP "10.1.0.2"
#define BURST_SIZE 4
#define BURST_NUMBER 50
#define IDENTIFIER "-"

using namespace std;
   
// Driver code
int main(int argc, char** argv) {

    if (argc != 4) {
      printf("Invalid format. Please use: ./client <interface name> <source I.P.> <message> \n");
      return 1;
    }

    const char *interface = argv[1];
    const char *client_ip = argv[2];
    const char *msg = argv[3];
    
    /*
    string msgString = IDENTIFIER + string(argv[3]);
    char msg[msgString.length() + 1];
    int i;
    int msgLength = msgString.length();
    for(i=0; i<msgLength; i++) {
      msg[i] = msgString[i];
    }
    msg[i] = '\0';
    */

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;

    //Get rid of junk
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, MIDDLEBOX_IP, &servaddr.sin_addr.s_addr);

    // Filling client information
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, client_ip, &cliaddr.sin_addr.s_addr);
   
    // Creating socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
       
    int n = -1;
    socklen_t len = sizeof(servaddr);
    int burst_ctr(0);

    //Bind socket to interface to send traffic through
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, sizeof(interface)) != 0) {  
      perror("bind to interface failed");
      exit(EXIT_FAILURE);
    }
    
    //Bind own IP address to socket
    if (bind(sockfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    //We will send requests and receive replies in batches (bursts)
    while (burst_ctr<BURST_NUMBER) {

      int msg_ctr(0);
      
      //Send out messages
      while (msg_ctr<BURST_SIZE) {
	sendto(sockfd, (const char *)msg, strlen(msg),
	       MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
	       sizeof(servaddr));
	//const char* print_msg = msg;
	cout << "Message sent: " << msg << endl;
	//sleep(1);
	msg_ctr++;
      }
    
      //Return results from middlebox
      while(msg_ctr > 0) {
	n = recvfrom(sockfd, (char *)buffer, MAXLINE, 
		     MSG_WAITALL, (struct sockaddr *) &servaddr,
		     &len);
        buffer[n] = '\0';
        cout << "Server: " << buffer << endl;
        msg_ctr--;
        if (n == -1) {
          fprintf(stderr, "recvfrom n = -1 with error message: %s (%d)\n", strerror(errno), errno);
        }
      }

      burst_ctr++;
    }
      
    close(sockfd);
    return 0;
}
