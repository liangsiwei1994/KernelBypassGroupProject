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
#include <fstream>
#include <assert.h>
   
#define PORT     8080
#define MAX_LENGTH 1024
#define MIDDLEBOX_IP "10.1.0.3"

#define MEMCACHED_REQ_ID_SIZE 2
#define MEMCACHED_HEADER "\0\0\0\1\0\0"
#define MEMCACHED_HEADER_SIZE 6
#define MEMCACHED_TAIL "\r\n"
#define MEMCACHED_TAIL_SIZE 2
#define NUM_KEYS_PER_PACKET 4

#define CSV_KEY_NUMS 200
#define MAX_KEY_LENGTH 32

using namespace std;
   
// Driver code
int main(int argc, char** argv) {

    if (argc != 3) {
      printf("Invalid format. Please use: ./server <interface name> <source I.P.>\n");
      return 1;
    }

    const char *interface = argv[1];
    const char *server_ip = argv[2];

    //Prevent double server conflict
    srand(time(NULL));
    
    int sockfd;
    char buffer[MAX_LENGTH];
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
    inet_pton(AF_INET, server_ip, &cliaddr.sin_addr.s_addr);
   
    // Creating socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
       
    int n = -1;
    socklen_t len = sizeof(servaddr);

    //Bind socket to interface to send traffic through
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, sizeof(interface)) != 0) {  
      perror("bind to interface failed");
      exit(EXIT_FAILURE);
    }

    struct timeval timeout;      
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    //Set socket timeout
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed\n");
    }
    
    //Bind own IP address to socket
    if (bind(sockfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }

    n = recvfrom(sockfd, (char *)buffer, MAX_LENGTH, 
          MSG_WAITALL, (struct sockaddr *) &servaddr,
          &len);

    buffer[n] = '\0';

    int msg_len = n - MEMCACHED_REQ_ID_SIZE - MEMCACHED_HEADER_SIZE - MEMCACHED_TAIL_SIZE;

    //Strip app-level headers
    char message[MAX_LENGTH];
    memcpy(message, buffer + MEMCACHED_REQ_ID_SIZE + MEMCACHED_HEADER_SIZE, msg_len);
    message[msg_len] = '\0';
    cout << '\r';
    cout << "Received: " << message << endl;

    close(sockfd);

    string msg_str(message);
    assert(msg_str.find("key1") != string::npos);
    assert(msg_str.find("key2") != string::npos);
    assert(msg_str.find("key3") != string::npos);
    assert(msg_str.find("key4") != string::npos);
    cout << '\r';
    cout << "Test succesful!" << endl;
    
    return 0;
}
