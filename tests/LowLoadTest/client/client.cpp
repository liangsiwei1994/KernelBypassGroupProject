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
#include <assert.h>

#define PORT     8080
#define MAX_LENGTH 1448
#define MIDDLEBOX_IP "10.1.0.2"
#define BURST_SIZE 1
#define BURST_NUMBER 1

#define MEMCACHED_REQ_ID_SIZE 2
#define MEMCACHED_HEADER "\0\0\0\1\0\0"
#define MEMCACHED_HEADER_SIZE 6
#define MEMCACHED_TAIL "\r\n"
#define MEMCACHED_TAIL_SIZE 2

struct app_payload {
  char *req_id;
  char *hdr;
  char *payload;
  uint16_t msg_len;
  char *tail;
  uint16_t total_len;
} global_ap;

struct app_payload *init_send_app_payload(char *app_buf, const char *message,
					  int req_id_len, char *hdr, int hdr_len, char *tail, int tail_len);

using namespace std;
   
// Driver code
int main(int argc, char** argv) {

    if (argc != 7) {
      printf("Invalid format. Please use: ./client <interface name> <source I.P.> <key 1> <exp. val 1> <key 2> <exp. val 2>\n");
      return 1;
    }

    const char *interface = argv[1];
    const char *client_ip = argv[2];
    const char *key1 = argv[3];
    const char *val1 = argv[4];
    const char *key2 = argv[5];
    const char *val2 = argv[6];

    //Construct packets
    char msg[512];
    strcpy(msg, "gets");
    strcat(msg, " ");
    strcat(msg, key1);
    strcat(msg, " ");
    strcat(msg, key2);

    //Prevent double client conflict
    srand(time(NULL));

    char hdr[] = MEMCACHED_HEADER;
    char tail[] = MEMCACHED_TAIL;
    char full_msg[MAX_LENGTH];
    
    int sockfd;
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

    struct app_payload *ap = init_send_app_payload(full_msg, msg, MEMCACHED_REQ_ID_SIZE, hdr,
                    MEMCACHED_HEADER_SIZE, tail, MEMCACHED_TAIL_SIZE);
    if(!ap) {
      cout << "Message too long!" << endl;
      return 0;
    }
    
    sendto(sockfd, (const char *)full_msg, ap->total_len,
          MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
          sizeof(servaddr));
    cout << '\r';
    cout << "Message sent: " << msg << endl;


    bool val1_rcvd = false;
    bool val2_rcvd = false;
    
    for (int k = 0; k < 2; k++) {
      char buffer[MAX_LENGTH];
      int n = -1;
      socklen_t len = sizeof(servaddr);
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

      string msg_str(message);
      if (msg_str.find(string(val1)) != string::npos) {
        val1_rcvd = true;
      }
      else if (msg_str.find(string(val2)) != string::npos) {
        val2_rcvd = true;
      }
    }

    assert(val1_rcvd && val2_rcvd);
    cout << '\r';
    cout << "Test succesful!" << endl;

    close(sockfd);
    return 0;
}

struct app_payload *init_send_app_payload(char *app_buf, const char *message,
					  int req_id_len, char *hdr, int hdr_len, char *tail, int tail_len) {

  if(strlen(message) > (unsigned int) MAX_LENGTH - hdr_len - tail_len) {
    printf("Failed to send message - message too long!");
    return NULL;
  }

  //Random request ID
  uint16_t req_id = 42;
  uint16_t temp = htons(req_id);
  
  struct app_payload *ap = &global_ap;
  ap->req_id = app_buf;
  ap->hdr = ap->req_id + req_id_len;
  ap->payload = ap->hdr + hdr_len;
  ap->msg_len = strlen(message);
  ap->tail = ap->payload + ap->msg_len;
  ap->total_len = req_id_len + hdr_len + ap->msg_len + tail_len;
  
  memcpy(ap->req_id, &temp, req_id_len);
  memcpy(ap->hdr, hdr, hdr_len);
  memcpy(ap->payload, message, ap->msg_len);
  memcpy(ap->tail, tail, tail_len);

  return ap;
}
