#include <middlebox.h>
#include <api.h>

static char payload_array[GROUP_SIZE][MAX_LENGTH];
static int payload_size[GROUP_SIZE];
static int payload_counter = 0;
static uint32_t ip_array[GROUP_SIZE];

static char ukey[GROUP_SIZE][MAX_KEY_LENGTH];
static int ukey_len = 0;
static uint16_t request_id = 0;

struct app_payload {
  char *req_id;
  char *hdr;
  char *payload;
  uint16_t msg_len;
  char *tail;
  uint16_t total_len;
} global_ap;

//Internal helper function which captures relevant positions along the packet for easy manipulation.
struct net_sge *init_net_sge(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph) {
  
  struct net_sge *e = rte_pktmbuf_mtod_offset(pkt_buf, struct net_sge *,
					      sizeof(struct ip_tuple));
  e->len = rte_be_to_cpu_16(udph->dgram_len) - sizeof(struct rte_udp_hdr);
  e->payload = (void *)((unsigned char *)udph + sizeof(struct rte_udp_hdr));
  e->handle = pkt_buf;
  return e;
}

//Internal helper function which fills app payload.
struct app_payload *init_send_app_payload(char *app_buf, char *message, uint16_t* req_id,
				     int req_id_len, char *hdr, int hdr_len, char *tail, int tail_len) {

  if(strlen(message) > MAX_LENGTH - hdr_len - tail_len) {
    printf("Failed to send message - message too long!");
    return NULL;
  }

  struct app_payload *ap = &global_ap;
  ap->req_id = app_buf;
  ap->hdr = ap->req_id + req_id_len;
  ap->payload = ap->hdr + hdr_len;
  ap->msg_len = strlen(message);
  ap->tail = ap->payload + ap->msg_len;
  ap->total_len = req_id_len + hdr_len + ap->msg_len + tail_len;

  uint16_t temp = htons(*req_id); // convert to network byte order

  rte_memcpy(ap->req_id, &temp, req_id_len);
  rte_memcpy(ap->hdr, hdr, hdr_len);
  rte_memcpy(ap->payload, message, ap->msg_len);
  rte_memcpy(ap->tail, tail, tail_len);

  return ap;
}

//Internal helper function which extracts application-level data.
struct app_payload *init_rcv_app_payload(char *app_buf, struct rte_mbuf *pkt_buf,
					 struct rte_udp_hdr *udph, int req_id_len, char *hdr,
					 int hdr_len, char *tail, int tail_len) {

  struct net_sge *e = init_net_sge(pkt_buf, udph);
  

  if(e->len > MAX_LENGTH) {
    printf("Received packet payload too long!");
    return NULL;
  }
  struct app_payload *ap = &global_ap;
  ap->req_id = app_buf;
  ap->hdr = ap->req_id + req_id_len;
  ap->payload = ap->hdr + hdr_len;
  ap->total_len = e->len;
  ap->msg_len = ap->total_len - tail_len - hdr_len - req_id_len;
  ap->tail = ap->req_id + ap->msg_len;
  
  rte_memcpy(ap->req_id, e->payload, ap->total_len);
  
  return ap;
}

//Internal helper function which sends a packet containing message and request ID req_id to destination_ip.
void app_send(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph,
	      char *message, uint16_t req_id, uint32_t destination_ip) {

  char full_msg[MAX_LENGTH];
  struct net_sge *e = init_net_sge(pkt_buf, udph);
  struct app_payload *ap = init_send_app_payload(full_msg, message, &req_id, MEMCACHED_REQ_ID_SIZE, MEMCACHED_HEADER,
					    MEMCACHED_HEADER_SIZE, MEMCACHED_TAIL, MEMCACHED_TAIL_SIZE);
  
  //Message too long
  if(!ap) {
    rte_pktmbuf_free(e->handle);
    return;
  }
    
  e->len = ap->total_len;
  rte_memcpy(e->payload, full_msg, e->len);
  udp_send(e, destination_ip);
  return;
};

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
  printf("key:%s\n",key);
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
void construct_keys_packet(char keys[MAX_KEY_PER_PACKET][MAX_KEY_LENGTH], int keys_len, char* payload)
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
}

void app_server_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph){

  char server_payload[MAX_LENGTH];
  struct app_payload *ap = init_rcv_app_payload(server_payload, pkt_buf, udph, MEMCACHED_REQ_ID_SIZE, MEMCACHED_HEADER,
						MEMCACHED_HEADER_SIZE, MEMCACHED_TAIL, MEMCACHED_TAIL_SIZE);
  //Incoming payload too large
  if(!ap) {
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  //If you didn't get anything back, try using
  //ip route add 10.1.0.2 via 10.1.0.3 dev veth2
  //on the command line

  uint16_t _req_id;
  rte_memcpy(&_req_id,ap->req_id,2);
  _req_id = htons(_req_id);

  printf("MEMCACHED REPLY RECEIVED FOR ID: %u, ", _req_id);
  // printf("MEMCACHED REPLY: %.*s\n", ap->msg_len, ap->payload);

  // strip the key and value
  char key[MAX_KEY_LENGTH];
  if(!strip_key_value(key, ap->payload, ap->msg_len))
  {
    printf("END\n");
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  char reply[MAX_LENGTH];
  rte_memcpy(reply, ap->payload, ap->msg_len);
  reply[ap->msg_len] = '\0';

  // getting list of clients requesting this value
  struct Array* client_data = get_from_map(key, _req_id);
  rte_mbuf_refcnt_update(pkt_buf,client_data->used-1);
  for (int i = 0; i < client_data->used; i++)
  {
    app_send(pkt_buf, udph, reply, (client_data->array[i]).req_id, (client_data->array[i]).src_ip);
  }
};

void app_client_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph, uint32_t client_address) {

  char client_payload[MAX_LENGTH];
  struct app_payload *ap = init_rcv_app_payload(client_payload, pkt_buf, udph, MEMCACHED_REQ_ID_SIZE, MEMCACHED_HEADER,
						MEMCACHED_HEADER_SIZE, MEMCACHED_TAIL, MEMCACHED_TAIL_SIZE);
  //Incoming payload too large
  if(!ap) {
    rte_pktmbuf_free(pkt_buf);
    return;
  }

  printf("APP RECEIVED FROM CLIENT: %.*s\n", ap->msg_len, ap->payload);

  // Initialize hashmap for client request mapping
  initialize_map();

  //TODO: Strip client packet to req id and key(s) (make sure to put '\0' at end of each key)
  char keys[MAX_KEY_PER_PACKET][MAX_KEY_LENGTH];
  int keys_len = 0;
  strip_keys(keys, &keys_len, ap->payload, ap->msg_len);

  uint16_t _req_id;
  memcpy(&_req_id,ap->req_id,2);
  _req_id = htons(_req_id);
  // printf("Req_id: %u\n",_req_id);

  // Initialize client_info struct
  struct client_info client_data;
  client_data.req_id = _req_id;
  client_data.src_ip = client_address;
  client_data.src_port = rte_be_to_cpu_16(udph->dst_port);
  
  // Register client info in hashmap 
  for (int i = 0; i < keys_len; i++)
  {
    // insert_client_info_to_map(keys[i], client_data);
    insert_client_info_to_map(keys[i], request_id, client_data);
    insert_ukey_group(keys[i]);

    // check if unique key group is full
    if (ukey_len >= GROUP_SIZE)
    {
      char payload[MAX_LENGTH];
      construct_keys_packet(ukey, ukey_len, payload);
      rte_mbuf_refcnt_update(pkt_buf,1);
      app_send(pkt_buf, udph, payload, request_id, ip_str_to_int(TARGET_IP));
      request_id = (request_id + 1)%MAX_REQ_ID;
      ukey_len = 0;
    }
  }
  rte_pktmbuf_free(pkt_buf);
};
