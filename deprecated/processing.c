#include <middlebox.h>
#include <map.h>

static hashmap* client_info_map = NULL;
static hashmap* counter_map = NULL;

// static uint16_t mbox_req_id = 64000;
static char ukey[GROUP_SIZE][MAX_KEY_LENGTH];
static int ukey_len = 0;
static char normalized_key[MAX_KEY_LENGTH];

void normalize_key(char* key)
{
    // function to ensure keys are normalized before hashed
    memset(normalized_key, sizeof(normalized_key),0);
    strcpy(normalized_key,key);
}

void insert_client_info_to_map(char* key, int request_id, struct client_info client_data)
{
    // combine current req_id with key
    char mbox_req_id_key[MAX_KEY_LENGTH];
    // sprintf(mbox_req_id_key,"%u-%s",mbox_req_id,key);
    sprintf(mbox_req_id_key,"%u-%s",request_id,key);

    normalize_key(mbox_req_id_key);

    uintptr_t result;

    // check if the req_id key pair already exists on the map
    if (hashmap_get(client_info_map, hashmap_str_lit(normalized_key), &result))
    {
        // printf("key: %s is already inside the map, adding item to map table directly\n",normalized_key);

        struct Array* table_ptr = (struct Array*) result;

        // printf("inserting %u\n",client_data.src_ip);

        insertArray(table_ptr, client_data);

        // printf("back of table: %u\n",(table_ptr->array[table_ptr->used-1]).src_ip);

        return;
    }

    // if the key doesn't yet exists on the map
    // printf("key: %s creating table\n",normalized_key);
    struct Array* table_ptr = malloc(sizeof(struct Array));
    initArray(table_ptr, 1);
    hashmap_set(client_info_map, hashmap_str_lit(normalized_key), (uintptr_t)table_ptr);
    // hashmap_set(counter_map, hashmap_str_lit(normalized_key), 0);
    // printf("inserting %u\n",client_data.src_ip);
    insertArray(table_ptr, client_data);
    // printf("back of table: %u\n",(table_ptr->array[table_ptr->used-1]).src_ip);
}

struct Array* get_from_map(char* key, uint16_t request_id)
{
    // combine current req_id with key
    char mbox_req_id_key[MAX_KEY_LENGTH];
    // sprintf(mbox_req_id_key,"%u-%s",mbox_req_id,key);
    sprintf(mbox_req_id_key,"%u-%s",request_id,key);

    normalize_key(mbox_req_id_key);

    // printf("getting value for key: %s \n", normalized_key);
    uintptr_t result;
    if (!hashmap_get(client_info_map, hashmap_str_lit(normalized_key), &result))
        printf("hashmap get failed (key not found!)\n");
    return (struct Array*) result;
}

void remove_key_from_map(char* key, uint16_t request_id)
{
    // combine current req_id with key
    char mbox_req_id_key[MAX_KEY_LENGTH];
    // sprintf(mbox_req_id_key,"%u-%s",mbox_req_id,key);
    sprintf(mbox_req_id_key,"%u-%s",request_id,key);

    normalize_key(mbox_req_id_key);

    struct Array* table_ptr = get_from_map(key, request_id);
    freeArray(table_ptr);
    hashmap_remove(client_info_map, hashmap_str_lit(normalized_key));
}

void initialize_map()
{
    // if global hashmap hasn't been initialized, initialize it
    if (!client_info_map)
        client_info_map = hashmap_create();
    if (!counter_map)
        counter_map = hashmap_create();
}
