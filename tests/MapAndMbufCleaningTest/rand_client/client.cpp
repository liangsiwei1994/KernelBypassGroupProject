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
#include <thread>
   
#define PORT     8080
#define MAX_LENGTH 1024
#define MIDDLEBOX_IP "10.1.0.2"
#define BURST_SIZE 1
#define BURST_NUMBER 1000

#define MEMCACHED_REQ_ID_SIZE 2
#define MEMCACHED_HEADER "\0\0\0\1\0\0"
#define MEMCACHED_HEADER_SIZE 6
#define MEMCACHED_TAIL "\r\n"
#define MEMCACHED_TAIL_SIZE 2
#define NUM_KEYS_PER_PACKET 4

#define CSV_KEY_NUMS 200
#define MAX_KEY_LENGTH 32

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

void create_rand_msg(char* msg, char** key_list, int key_num);

void get_list_of_keys(char key_list[CSV_KEY_NUMS][MAX_KEY_LENGTH], int &key_num);

void create_rand_msg(char* msg, char key_list[CSV_KEY_NUMS][MAX_KEY_LENGTH], int key_num);

using namespace std;
   
// Driver code
int main(int argc, char** argv) {

    if (argc != 3) {
      printf("Invalid format. Please use: ./client <interface name> <source I.P.>\n");
      return 1;
    }

    const char *interface = argv[1];
    const char *client_ip = argv[2];

    //Prevent double client conflict
    srand(time(NULL));

    char hdr[] = MEMCACHED_HEADER;
    char tail[] = MEMCACHED_TAIL;
    char full_msg[MAX_LENGTH];
    
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

    //Get list of keys
    char key_list[CSV_KEY_NUMS][MAX_KEY_LENGTH];
    int key_num;
    get_list_of_keys(key_list, key_num);

    //We will send requests and receive replies in batches (bursts)
    while (burst_ctr<BURST_NUMBER) {

      int msg_ctr(0);
      
      //Send out messages
      while (msg_ctr<BURST_SIZE) {

        // get random msg:
        char msg[512];
        create_rand_msg(msg, key_list, key_num);

        struct app_payload *ap = init_send_app_payload(full_msg, msg, MEMCACHED_REQ_ID_SIZE, hdr,
                    MEMCACHED_HEADER_SIZE, tail, MEMCACHED_TAIL_SIZE);
        if(!ap) {
          cout << "Message too long!" << endl;
          return 0;
        }
        
        sendto(sockfd, (const char *)full_msg, ap->total_len,
              MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
              sizeof(servaddr));
        cout << "Message sent: " << msg << endl;
        msg_ctr++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      burst_ctr++;
    }

    struct app_payload *ap = init_send_app_payload(full_msg, "get complete", MEMCACHED_REQ_ID_SIZE, hdr,
                    MEMCACHED_HEADER_SIZE, tail, MEMCACHED_TAIL_SIZE);
    sendto(sockfd, (const char *)full_msg, ap->total_len,
          MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
          sizeof(servaddr));
    cout << "Message sent: get complete" << endl;
      
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
  uint16_t req_id = 24;
  uint16_t temp = htons(req_id + rand() % 65535);
  
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

void get_list_of_keys(char key_list[CSV_KEY_NUMS][MAX_KEY_LENGTH], int &key_num)
{
  ifstream file;
  file.open("config-files/key.csv");

  if (!file) 
    file.open("../config-files/key.csv");

  if (!file) 
    cout << "key.csv cannot be found!" << endl;

  int index = 0;
  char buffer[512];
  while (file >> buffer && strcmp(buffer,"\n") != 0)
  {
    strcpy(key_list[index], buffer);
    index++;
  }
  key_num = index;
  file.close();
}

void create_rand_msg(char* msg, char key_list[CSV_KEY_NUMS][MAX_KEY_LENGTH], int key_num)
{
  strcpy(msg, "gets");
  for (int i = 0; i < NUM_KEYS_PER_PACKET; i++)
  {
    int rand_i = rand() % key_num;
    strcat(msg, " ");
    strcat(msg, key_list[rand_i]);
  }
}
