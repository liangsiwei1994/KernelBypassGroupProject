#include <api.h>

static char ukey[MAP_ENTRIES][MAX_KEY_LENGTH];
static uint16_t ukey_len = 0;
static uint16_t total_ukey_size =0; 
static uint16_t request_id = 0;
static uint16_t outstanding_req = 0;

static char key_hold[MAX_KEY_LENGTH];
static char last_key[MAX_KEY_LENGTH];
static uint16_t pkt_counts[MAP_ENTRIES];
static struct rte_hash* client_pkts;
static uint16_t keys_sent[MAX_REQ_ID] = {0};
static int16_t keys_rcvd[MAX_REQ_ID] = {0};
static uint16_t last_seq_id = 0;
static clock_t last_send_clock = 0;

static char LRU_queue[MAP_ENTRIES][MAX_KEY_LENGTH];
static int LRU_ptr = 0;

static struct rte_mbuf* memcached_pkt = NULL;

static clock_t program_start_time = 0;

struct app_payload {
  char *req_id;
  char *hdr;
  char *payload;
  uint16_t msg_len;
  uint16_t total_len_no_id;
  char *tail;
  uint16_t total_len;
} global_ap;

/* Initialize memcached packet for sending*/
void init_memcached_pkt(struct rte_mbuf* pkt_buf);

/* Hashmap creation */
void init_hashmap();

/* Get information from packet */
void get_iph_udph_from_buf(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr **iph_out, struct rte_udp_hdr** udph_out);
struct net_sge *init_net_sge(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph);
uint16_t extract_info(char* pos, int len);

/* Key extraction and concatenation */
bool strip_key_value(char* key, char* payload, uint16_t payload_len);
void strip_keys(char keys[MAX_KEY_PER_PACKET][MAX_KEY_LENGTH], int* keys_len, char* payload, uint16_t payload_len);
void construct_keys_packet(char keys[MAP_ENTRIES][MAX_KEY_LENGTH], int keys_len, char* payload);
void insert_ukey_group(const char* key);

/* Creating, reading, and sending application-level payloads in the correct format */
struct app_payload *init_send_app_payload(struct rte_udp_hdr *udph, const char *message, uint16_t* req_id,
					  bool to_server);
struct app_payload *init_rcv_app_payload(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph);
void app_send(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph,
	      char *msg, uint16_t req_id, uint32_t destination_ip, bool to_server);

/***** Current Working Area *****/

//Constructs a packet with all keys in ukey and sends it to the server
void construct_pkt_and_send_to_server(bool full_pkt) {

  //If we have no keys in ukey, there is nothing to send
  if(ukey_len == 0)
    return;

  struct rte_udp_hdr* memcached_udph;
  struct rte_ipv4_hdr* memcached_iph;
  get_iph_udph_from_buf(memcached_pkt, &memcached_iph, &memcached_udph);
  
  char payload[MAX_PAYLOAD_LEN];
  
  if(full_pkt) {

    //If what we have in ukey brings us over the max payload, set the retained last key as the new first key
    construct_keys_packet(ukey, ukey_len - 1, payload);
    keys_sent[request_id] = ukey_len - 1; //Store number of keys sent to server
    strcpy(ukey[0], ukey[ukey_len - 1]);
    ukey_len = 1;
    total_ukey_size = strlen(ukey[0]); 
  } else {

    //If forced to send a partially-filled packet, we can send all keys in ukey
    construct_keys_packet(ukey, ukey_len, payload);
    keys_sent[request_id] = ukey_len; 
    ukey_len = 0;
    total_ukey_size = 0;
  }

  outstanding_req++; //Memcached will now be dealing with 1 more outstanding request
  keys_rcvd[request_id] = 0; //Refresh received key count
  
  rte_mbuf_refcnt_update(memcached_pkt,1);
  app_send(memcached_pkt, memcached_udph, payload, request_id, ip_str_to_int(TARGET_IP), true);
  request_id = (request_id + 1)%MAX_REQ_ID;
  last_send_clock = clock();
}

//To implement timeout clearing
void refresh_backlog() {

  // check if program has timed out
  if (PROGRAM_TIMEOUT >= 0) {
    if (program_start_time == 0) program_start_time = clock();

    clock_t dt = clock() - program_start_time;
    double dt_ms = ((double)dt)/CLOCKS_PER_SEC*1000; //ms
    if (dt_ms >= PROGRAM_TIMEOUT) {
      printf("Middlebox runtime finished!\n");
      exit(0);
    }
  }

  // initialize last send clock
  if (last_send_clock == 0) last_send_clock = clock();

  if (ukey_len == 0) return; // do nothing if no backlog

  clock_t dt = clock() - last_send_clock;
  double dt_ms = ((double)dt)/CLOCKS_PER_SEC*1000; //ms
  if (dt_ms >= TIMEOUT) {

    // remove all outstanding_req
    outstanding_req = 0;

    // remove all keys_rcvd count to -1 and 
    for (int i = 0; i < MAX_REQ_ID; i++)
      keys_rcvd[i] = -1;
    
    construct_pkt_and_send_to_server(false);
    return;
  }
}

//User-defined function to steer replies from server to correct clients.
void app_server_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph) {
  // clock_t t = clock();

  struct app_payload *ap = init_rcv_app_payload(pkt_buf, udph);
  //Incoming payload too large
  if(!ap) {
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  //If you didn't get anything back, try using
  //sudo ip route add 10.1.0.2 via 10.1.0.3 dev veth2
  //on the command line

  //Extract request ID, position of pkt in sequence, and expected number of datagrams
  uint16_t _req_id = extract_info(ap->req_id, 2);
  uint16_t _seq_id = extract_info(ap->hdr, 2);
  uint16_t _datagram_exp = extract_info(ap->hdr + 2, 2);

  // printf("MEMCACHED REPLY RECEIVED FOR ID: %u\n", _req_id);

  //Strip key and value if present. Memcached does not include
  //key name in multi-packet replies after the first packet.
  //Thus, we assume that if no key is present, the previous key currently
  //stored in last_key still applies.
  char key[MAX_KEY_LENGTH];
  struct rte_mbuf* head = NULL;
  int position;

  if(strip_key_value(key, ap->payload, ap->msg_len)) {
    
    //Obtain consistent signature key for lookup
    memset(key_hold, '\0', sizeof(key_hold));
    sprintf(key_hold,"%u-%s",_req_id,key);

    //Copy into last_key if we expect more than one packet for a reply
    if (_datagram_exp > 1)
      rte_memcpy(last_key, key_hold, sizeof(key_hold));

    //Setting counter value and get the chain head
    position = rte_hash_lookup_data(client_pkts, key_hold, (void **) &head);

  } else //Anything without a key name is a non-initial reply packet
    position = rte_hash_lookup_data(client_pkts, last_key, (void **) &head);

  //Unable to figure out where this packet came from
  if (position < 0) {
    // printf("Unable to find client to send packet to!\n");
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  // printf("APP RECEIVED FROM SERVER: %.*s\n", ap->msg_len, ap->payload);
  
  //Traverse the chain and send packet to clients
  struct rte_mbuf* ptr = head;
  while(ptr) {

    struct rte_udp_hdr* curr_udph;
    struct rte_ipv4_hdr* curr_iph;
    get_iph_udph_from_buf(ptr, &curr_iph, &curr_udph);

    struct app_payload *ap_ptr = init_rcv_app_payload(ptr, curr_udph);

    //Extract client request ID for sending
    uint16_t client_req_id = extract_info(ap_ptr->req_id, 2);
    //rte_memcpy(&client_req_id,ap_ptr->req_id,2);
    //client_req_id = htons(client_req_id);

    rte_mbuf_refcnt_update(pkt_buf,1);

    app_send(pkt_buf, udph, "", client_req_id, rte_be_to_cpu_32(curr_iph->src_addr), false);

    ptr = ptr->next;

  }
  rte_pktmbuf_free(pkt_buf);

  //We assume that there is no packet duplication and values from different keys
  //are not interleaved - it would be impossible to resolve them otherwise.
  //If a request has been marked as fulfilled (multiplied by -1), stop tracking it to save time.
  if(keys_rcvd[_req_id] >= 0 && _seq_id <= last_seq_id) {

    keys_rcvd[_req_id]++;

    //Requests are deemed fulfilled (no longer outstanding)if Memcached
    //has replied to more than FULFILL_THRESHOLD of the keys requested.
    //This accounts for packet loss.
    if (keys_rcvd[_req_id] >
	      (int) floor(keys_sent[_req_id] * FULFILL_THRESHOLD)) {
    
      keys_rcvd[_req_id] = -1;
      outstanding_req--;
      //If we happen to have space now, send whatever we have
      if (outstanding_req < MIN_OUTSTANDING_REQ){
	        construct_pkt_and_send_to_server(false);
      }
    }
  }

  //Track sequence IDs for all packets; we detect if we have reached
  //a new key by comparing to the sequence ID of the packet before
  last_seq_id = _seq_id;

  //Check if all packets are received from server
  if (pkt_counts[position] < _datagram_exp) 
    pkt_counts[position]++;
  else {
    // Clear the hashmap
    rte_hash_del_key(client_pkts, key_hold);
    rte_pktmbuf_free(head);
  }

  // t = clock() - t;
  // double time_taken = ((double)t)/CLOCKS_PER_SEC*1000;
  // printf("server_in time: %f ms\n",time_taken);
};

//User-defined function to merge incoming requests from client.
void app_client_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph, uint32_t client_address) {
  // clock_t t = clock();

  struct app_payload *ap = init_rcv_app_payload(pkt_buf, udph);
  //Incoming payload too large
  if(!ap) {
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  //printf("APP RECEIVED FROM CLIENT: %.*s\n", ap->msg_len, ap->payload);

  init_memcached_pkt(pkt_buf);

  //Strip client packet to obtain key(s)
  char keys[MAX_KEY_PER_PACKET][MAX_KEY_LENGTH];
  int keys_len = 0;
  strip_keys(keys, &keys_len, ap->payload, ap->msg_len);

  struct rte_mbuf* curr_buf = pkt_buf;
  
  // Register client info in hashmap 
  for (int i = 0; i < keys_len; i++)
  {
    //Remove duplicates and ensure consistent key signature for hashing
    insert_ukey_group(keys[i]);

    memset(key_hold, '\0', sizeof(key_hold));
    sprintf(key_hold,"%u-%s",request_id,keys[i]);

    // if client issues more than one key in a packet -> allocate new smaller mbuf(s) for the chaining
    if (i > 0) {
      curr_buf = rte_pktmbuf_copy(pkt_buf, small_mbuf_pool, 0, UINT32_MAX);
    }

    struct rte_mbuf* first_buf = NULL;

    // find req_id-key in hashtable
    int position = rte_hash_lookup_data(client_pkts, key_hold, (void**)&first_buf);

    // if key is not found on the hashtable, update LRU queue and drop least recently pending item
    if (position < 0) {
      struct rte_mbuf* LRU_buf = NULL;
      int pos_LRU = rte_hash_lookup_data(client_pkts, LRU_queue[LRU_ptr], (void**)&LRU_buf);
      if (pos_LRU >= 0) {
        rte_hash_del_key(client_pkts, LRU_queue[LRU_ptr]);
        rte_pktmbuf_free(LRU_buf);
      }
      rte_memcpy(LRU_queue[LRU_ptr], key_hold, sizeof(key_hold));
      LRU_ptr = (LRU_ptr + 1)%MAP_ENTRIES; // circular traversal
    }

    // add or update the current value for current key in the hashmap
    rte_hash_add_key_data(client_pkts, key_hold, curr_buf);

    // if key is previously found on map, add it to the key's chain
    if (position >= 0) {
      curr_buf->next = first_buf;
    }

    // reset expected number of packet for newly inserted key
    pkt_counts[rte_hash_lookup(client_pkts, key_hold)] = 1;

    //If we exceed maximum payload size, send all keys except the most recent one (over the limit)
    if (total_ukey_size >= MAX_PAYLOAD_LEN - GETS_LEN - ukey_len + 1)
      construct_pkt_and_send_to_server(true); 
  }

  //However, if we have space at memcached, send client requests without aggregating across clients
  if (outstanding_req < MIN_OUTSTANDING_REQ){
    construct_pkt_and_send_to_server(false);
  }

  // t = clock() - t;
  // double time_taken = ((double)t)/CLOCKS_PER_SEC*1000;
  // printf("client_in time: %f ms\n",time_taken);
};

/***** Internal helper functions with confirmed behaviour *****/

//Helper function to initialise hash table. Only for use by main().
void init_hashmap() {

  struct rte_hash_parameters params = { 0 };
  params.entries = MAP_ENTRIES;
  params.key_len = MAX_KEY_LENGTH;
  params.hash_func = rte_jhash;
  params.hash_func_init_val = 0;
  params.socket_id = rte_socket_id();

  client_pkts = rte_hash_create(&params);
}

//Internal helper function to initialize memcached packet from first client packet received
void init_memcached_pkt(struct rte_mbuf* pkt_buf) {
  if (!memcached_pkt) {
    printf("Initializing memcached_pkt!\n");
    memcached_pkt = rte_pktmbuf_copy(pkt_buf, mbuf_pool, 0, UINT32_MAX);
  }
}

//Internal helper function to extract ip and udp headers from a packet
void get_iph_udph_from_buf(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr **iph_out, struct rte_udp_hdr** udph_out) {
  unsigned char *payload = rte_pktmbuf_mtod(pkt_buf, unsigned char *);
  struct rte_ipv4_hdr *iph = (struct rte_ipv4_hdr *)(payload + (sizeof(struct rte_ether_hdr)));
  int hdrlen = (iph->version_ihl & RTE_IPV4_HDR_IHL_MASK) * RTE_IPV4_IHL_MULTIPLIER;
  struct rte_udp_hdr *udph = (struct rte_udp_hdr *)((unsigned char *)iph + hdrlen);
  *iph_out = iph;
  *udph_out = udph;
  return;
}

//Internal helper function which captures relevant positions along the packet for easy manipulation.
struct net_sge *init_net_sge(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph) {
  
  struct net_sge *e = rte_pktmbuf_mtod_offset(pkt_buf, struct net_sge *,
					      sizeof(struct ip_tuple));
  e->len = rte_be_to_cpu_16(udph->dgram_len) - sizeof(struct rte_udp_hdr);
  e->payload = (void *)((unsigned char *)udph + sizeof(struct rte_udp_hdr));
  e->handle = pkt_buf;
  return e;
}

//Internal helper function to extract information (e.g., sequence number) from a received packet.  
uint16_t extract_info(char* pos, int len) {
  
  uint16_t num;
  rte_memcpy(&num, pos, len);
  return ntohs(num); //Convert to host byte order first
}

//Internal helper function which extracts a key from the server reply.
bool strip_key_value(char* key, char* payload, uint16_t payload_len)
{
  if (!strstr(payload,"VALUE"))
    return false; // No key found

  int i = 6; // 6 is start of key
  int j = 0;
  while (i < payload_len && payload[i] != ' ')
  {
    key[j] = payload[i++];
    key[++j] = '\0';
  }
  return true;
}

//Internal helper function which extracts keys from a client request.
void strip_keys(char keys[MAX_KEY_PER_PACKET][MAX_KEY_LENGTH], int* keys_len, char* payload, uint16_t payload_len)
{
  int i = 0;
  // move index to the first key
  while (i < payload_len && payload[i] != ' ')
    i++;
  i++;
  
  // copy keys
  *keys_len = 0;
  char buffer[MAX_KEY_LENGTH];
  int j = 0;
  int k = 0;
  for (; i < payload_len; i++)
  {
    if (payload[i] == ' ')
    {
      strcpy(keys[k],buffer);
      j = 0;
      k++;
      (*keys_len)++;
      continue;
    }
    buffer[j] = payload[i];
    buffer[++j] = '\0';
  }
  strcpy(keys[k],buffer);
  (*keys_len)++;
}

//Internal helper function which merges client requests.
void construct_keys_packet(char keys[MAP_ENTRIES][MAX_KEY_LENGTH], int keys_len, char* payload)
{
  // adding gets statement
  strcpy(payload, "gets");
  // adding keys
  for (int i = 0; i < keys_len; i++)
  {
    strcat(payload, " ");
    strcat(payload, keys[i]);
  }
}

//Internal helper function which ensures no duplicate keys are added.
void insert_ukey_group(const char* key)
{
    // check if key already exists
    for (int i = 0; i < ukey_len; i++)
        if (strcmp(key,ukey[i]) == 0)
            return;

    // add key
    strcpy(ukey[ukey_len],key);
    ukey_len++;
    total_ukey_size += strlen(key); 
}

//Internal helper function which fills app payload.
struct app_payload *init_send_app_payload(struct rte_udp_hdr *udph, const char *message, uint16_t* req_id,
					  bool to_server) {

  int req_id_len = MEMCACHED_REQ_ID_SIZE;
  char* hdr = MEMCACHED_HEADER;
  int hdr_len = MEMCACHED_HEADER_SIZE; 
  char* tail = MEMCACHED_TAIL;
  int tail_len = MEMCACHED_TAIL_SIZE;

  int message_length = strlen(message);
  char* payload_ptr = (char *)((unsigned char *)udph + sizeof(struct rte_udp_hdr));
  uint32_t payload_len = rte_be_to_cpu_16(udph->dgram_len) - sizeof(struct rte_udp_hdr);

  if(message_length > MAX_PAYLOAD_LEN) {
    printf("Failed to send message - message too long!");
    return NULL;
  }

  struct app_payload *ap = &global_ap;
  ap->req_id = payload_ptr;
  ap->hdr = ap->req_id + req_id_len;
  ap->payload = ap->hdr + hdr_len;
  ap->total_len = payload_len;
  ap->msg_len = ap->total_len - tail_len - hdr_len - req_id_len;

  // if sending to memcached server:
  if(to_server) {
    // copy req_id
    uint16_t temp = htons(*req_id); // convert to network byte order
    rte_memcpy(ap->req_id, &temp, req_id_len);

    // copy the rest of server header
    rte_memcpy(ap->hdr, hdr, hdr_len);

    // copy the message
    rte_memcpy(ap->payload, message, message_length);

    // update length
    ap->msg_len = message_length;
    ap->total_len = ap->msg_len + tail_len + hdr_len + req_id_len;

    // copy the tail
    ap->tail = ap->payload + ap->msg_len;
    rte_memcpy(ap->tail, tail, tail_len);
  }

  // if forwarding to client:
  else {
    //copy client req_id
    uint16_t temp = htons(*req_id); // convert to network byte order
    rte_memcpy(ap->req_id, &temp, req_id_len);
  }

  return ap;
}

//Internal helper function which extracts application-level data.
struct app_payload *init_rcv_app_payload(struct rte_mbuf *pkt_buf,
					 struct rte_udp_hdr *udph) {

  int req_id_len = MEMCACHED_REQ_ID_SIZE;
  int hdr_len = MEMCACHED_HEADER_SIZE;
  int tail_len = MEMCACHED_TAIL_SIZE;

  char* payload_ptr = (char *)((unsigned char *)udph + sizeof(struct rte_udp_hdr));
  uint32_t payload_len = rte_be_to_cpu_16(udph->dgram_len) - sizeof(struct rte_udp_hdr);
  
  if(payload_len > UDP_MAX_LEN) {
    printf("Received packet payload too long!");
    return NULL;
  }
  struct app_payload *ap = &global_ap;
  ap->req_id = payload_ptr;
  ap->hdr = ap->req_id + req_id_len;
  ap->payload = ap->hdr + hdr_len;
  ap->total_len = payload_len;
  ap->msg_len = ap->total_len - tail_len - hdr_len - req_id_len;
  ap->tail = ap->payload + ap->msg_len;

  return ap;
}

//Internal helper function which sends a packet containing message and request ID req_id to destination_ip.
void app_send(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph,
	      char *msg, uint16_t req_id, uint32_t destination_ip, bool to_server) {

  struct app_payload *ap = init_send_app_payload(udph, msg, &req_id, to_server);

  //Message too long
  if(!ap) {
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  struct net_sge *e = init_net_sge(pkt_buf, udph);

  // update e->len if dest is server
  if (to_server) {
    e->len = ap->total_len;
  }

  udp_send(e, destination_ip);
  return;
};
