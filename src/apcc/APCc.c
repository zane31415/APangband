#include "APCc.h"

// For testing sp functions
bool sp_testing = false;

// Helper funcs
void json_print(const char* key, json_t* j)
{
    char* jdump = json_dumps(j, JSON_DECODE_ANY);
    printf("%s:\n%s\n", key, jdump);
    free(jdump);
}

const char* jtype_to_string(json_t* j) 
{
    json_type jt = json_typeof(j);
    switch (jt) {
    case JSON_OBJECT:
        return "JSON_OBJECT";
    case JSON_ARRAY:
        return "JSON_ARRAY";
    case JSON_STRING:
        return "JSON_STRING";
    case JSON_INTEGER:
        return "JSON_INTEGER";
    case JSON_TRUE:
        return "JSON_BOOL";
    case JSON_FALSE:
        return "JSON_BOOL";
    case JSON_REAL:
        return "JSON_REAL";
    case JSON_NULL:
        return "JSON_NULL";
    default:
        return "Unknown JSON type";
    }
}

gboolean g_array_append_unique_gstring(GArray* array, GString* str) {
    for (guint i = 0; i < array->len; i++) {
        GString* existing = g_array_index(array, GString*, i);
        if (g_string_equal(existing, str)) {
            return FALSE;
        }
    }

    GString* str_copy = g_string_new(str->str);
    g_array_append_val(array, str_copy);
    return TRUE;
}

// Struct funcs

struct AP_NetworkVersion* AP_NetworkVersion_new(int major, int minor, int build) {
    struct AP_NetworkVersion* newVersion = (struct AP_NetworkVersion*)malloc(sizeof(struct AP_NetworkVersion));
    if (newVersion != NULL) {
        newVersion->major = major;
        newVersion->minor = minor;
        newVersion->build = build;
    }
    return newVersion;
};

void AP_NetworkVersion_free(struct AP_NetworkVersion* version) {
    if (version != NULL) {
        free(version);
    }
};

struct AP_NetworkItem* AP_NetworkItem_new(uint64_t item, uint64_t location, int player, int flags, const char* itemName, const char* locationName, const char* playerName) {
    struct AP_NetworkItem* newItem = (struct AP_NetworkItem*)malloc(sizeof(struct AP_NetworkItem));
    if (newItem != NULL) {
        newItem->item = item;
        newItem->location = location;
        newItem->player = player;
        newItem->flags = flags;
        newItem->itemName = _strdup(itemName);
        newItem->locationName = _strdup(locationName);
        newItem->playerName = _strdup(playerName);
    }
    return newItem;
};

void AP_NetworkItem_free(struct AP_NetworkItem* item) {
    if (item != NULL) {
        free(item->itemName);
        free(item->locationName);
        free(item->playerName);
        free(item);
    }
};

struct AP_NetworkPlayer* AP_NetworkPlayer_new(int team, int slot, const char* name, const char* alias, const char* game) {
    struct AP_NetworkPlayer* newPlayer = (struct AP_NetworkPlayer*)malloc(sizeof(struct AP_NetworkPlayer));
    if (newPlayer != NULL) {
        newPlayer->team = team;
        newPlayer->slot = slot;
        newPlayer->name = _strdup(name);
        newPlayer->alias = _strdup(alias);
        newPlayer->game = _strdup(game);
    }
    return newPlayer;
};

void AP_NetworkPlayer_free(struct AP_NetworkPlayer* player) {
    if (player != NULL) {
        free(player->name);
        free(player->alias);
        free(player->game);
        free(player);
    }
};

struct AP_MessagePart* AP_MessagePart_new(const char* text, AP_MessagePartType type) {
    struct AP_MessagePart* newPart = (struct AP_MessagePart*)malloc(sizeof(struct AP_MessagePart));
    if (newPart != NULL) {
        newPart->text = _strdup(text);
        newPart->type = type;
    }
    return newPart;
};

void AP_MessagePart_free(struct AP_MessagePart* part) {
    if (part != NULL) {
        free(part->text);
        free(part);
    }
};

struct AP_Message* AP_Message_new(AP_MessageType type, const char* text, GArray* messageParts) {
    struct AP_Message* newMessage = (struct AP_Message*)malloc(sizeof(struct AP_Message));
    if (newMessage != NULL) {
        newMessage->type = type;
        newMessage->text = _strdup(text);
        newMessage->messageParts = messageParts;
    }
    return newMessage;
};

void AP_Message_free(struct AP_Message* message) {
    if (message != NULL) {
        free(message->text);
        if (message->messageParts != NULL) {
            g_array_free(message->messageParts, TRUE);
        }
        free(message);
    }
};

struct AP_ItemSendMessage* AP_ItemSendMessage_new(const char* item, const char* recvPlayer, AP_MessageType type, const char* text, GArray* messageParts) {
    struct AP_ItemSendMessage* newItemSendMessage = (struct AP_ItemSendMessage*)malloc(sizeof(struct AP_ItemSendMessage));
    if (newItemSendMessage != NULL) {
        newItemSendMessage->base.type = type;
        newItemSendMessage->base.text = _strdup(text);
        newItemSendMessage->base.messageParts = messageParts;
        newItemSendMessage->item = _strdup(item);
        newItemSendMessage->recvPlayer = _strdup(recvPlayer);
    }
    return newItemSendMessage;
};

void AP_ItemSendMessage_free(struct AP_ItemSendMessage* itemSendMessage) {
    if (itemSendMessage != NULL) {
        free(itemSendMessage->base.text);
        if (itemSendMessage->base.messageParts != NULL) {
            g_array_free(itemSendMessage->base.messageParts, TRUE);
        }
        free(itemSendMessage->item);
        free(itemSendMessage->recvPlayer);
        free(itemSendMessage);
    }
};

struct AP_ItemRecvMessage* AP_ItemRecvMessage_new(const char* item, const char* sendPlayer, AP_MessageType type, const char* text, GArray* messageParts) {
    struct AP_ItemRecvMessage* newItemRecvMessage = (struct AP_ItemRecvMessage*)malloc(sizeof(struct AP_ItemRecvMessage));
    if (newItemRecvMessage != NULL) {
        newItemRecvMessage->base.type = type;
        newItemRecvMessage->base.text = _strdup(text);
        newItemRecvMessage->base.messageParts = messageParts;
        newItemRecvMessage->item = _strdup(item);
        newItemRecvMessage->sendPlayer = _strdup(sendPlayer);
    }
    return newItemRecvMessage;
};

void AP_ItemRecvMessage_free(struct AP_ItemRecvMessage* itemRecvMessage) {
    if (itemRecvMessage != NULL) {
        free(itemRecvMessage->base.text);
        if (itemRecvMessage->base.messageParts != NULL) {
            g_array_free(itemRecvMessage->base.messageParts, TRUE);
        }
        free(itemRecvMessage->item);
        free(itemRecvMessage->sendPlayer);
        free(itemRecvMessage);
    }
};

struct AP_HintMessage* AP_HintMessage_new(const char* item, const char* sendPlayer, const char* recvPlayer, const char* location, int checked, AP_MessageType type, const char* text, GArray* messageParts) {
    struct AP_HintMessage* newHintMessage = (struct AP_HintMessage*)malloc(sizeof(struct AP_HintMessage));
    if (newHintMessage != NULL) {
        newHintMessage->base.type = type;
        newHintMessage->base.text = _strdup(text);
        newHintMessage->base.messageParts = messageParts;
        newHintMessage->item = _strdup(item);
        newHintMessage->sendPlayer = _strdup(sendPlayer);
        newHintMessage->recvPlayer = _strdup(recvPlayer);
        newHintMessage->location = _strdup(location);
        newHintMessage->checked = checked;
    }
    return newHintMessage;
}

void AP_HintMessage_free(struct AP_HintMessage* hintMessage) {
    if (hintMessage != NULL) {
        free(hintMessage->base.text);
        if (hintMessage->base.messageParts != NULL) {
            g_array_free(hintMessage->base.messageParts, TRUE);
        }
        free(hintMessage->item);
        free(hintMessage->sendPlayer);
        free(hintMessage->recvPlayer);
        free(hintMessage->location);
        free(hintMessage);
    }
}

struct AP_CountdownMessage* AP_CountdownMessage_new(int timer, AP_MessageType type, const char* text, GArray* messageParts) {
    struct AP_CountdownMessage* newCountdownMessage = (struct AP_CountdownMessage*)malloc(sizeof(struct AP_CountdownMessage));
    if (newCountdownMessage != NULL) {
        newCountdownMessage->base.type = type;
        newCountdownMessage->base.text = _strdup(text);
        newCountdownMessage->base.messageParts = messageParts;
        newCountdownMessage->timer = timer;
    }
    return newCountdownMessage;
}

void AP_CountdownMessage_free(struct AP_CountdownMessage* countdownMessage) {
    if (countdownMessage != NULL) {
        free(countdownMessage);
    }
}

struct AP_SetServerDataRequest* AP_SetServerDataRequest_new(AP_RequestStatus status, const char* key, GArray* operations, void* default_value, AP_DataType type, bool want_reply) {
    struct AP_SetServerDataRequest* request = (struct AP_SetServerDataRequest*)malloc(sizeof(struct AP_SetServerDataRequest));
    if (request != NULL) {
        request->status = status;
        request->key = _strdup(key);
        request->operations = operations;
        request->default_value = default_value;
        request->type = type;
        request->want_reply = want_reply;
    }
    return request;
}


void AP_SetServerDataRequest_free(struct AP_SetServerDataRequest* request) {
    if (request != NULL) {
        free(request->key);
        // Free each element in the GArray if necessary
        if (request->operations != NULL) {
            g_array_free(request->operations, TRUE);
        }
        free(request);
    }
}

struct AP_GetServerDataRequest* AP_GetServerDataRequest_new(AP_RequestStatus status, const char* key, void* value, AP_DataType type) {
    struct AP_GetServerDataRequest* request = (struct AP_GetServerDataRequest*)malloc(sizeof(struct AP_GetServerDataRequest));
    if (request != NULL) {
        request->status = status;
        request->key = _strdup(key);
        request->value = value;
        request->type = type;
    }
    return request;
}

void AP_GetServerDataRequest_free(struct AP_GetServerDataRequest* request) {
    if (request != NULL) {
        free(request->key);
        free(request);
    }
}

struct AP_SetReply* AP_SetReply_new(const char* key, void* original_value, void* value) {
    struct AP_SetReply* reply = (struct AP_SetReply*)malloc(sizeof(struct AP_SetReply));
    if (reply != NULL) {
        reply->key = _strdup(key);
        reply->original_value = original_value;
        reply->value = value;
    }
    return reply;
}

void AP_SetReply_free(struct AP_SetReply* reply) {
    if (reply != NULL) {
        free(reply->key);
        free(reply);
    }
}

struct AP_GameData* AP_GameData_new(GHashTable* item_data, GHashTable* location_data) {
    struct AP_GameData* game_data = (struct AP_GameData*)malloc(sizeof(struct AP_GameData));
    if (game_data != NULL) {
        game_data->item_data = item_data;
        game_data->location_data = location_data;
    }
    return game_data;
}

void AP_GameData_free(struct AP_GameData* game_data) {
    if (game_data != NULL) {
        if (game_data->item_data) g_hash_table_destroy(game_data->item_data);
        if (game_data->location_data) g_hash_table_destroy(game_data->location_data);
        free(game_data);
    }
}

struct AP_DataStorageOperation* AP_DataStorageOperation_new(const char* operation, void* value) {
    struct AP_DataStorageOperation* operation_struct = (struct AP_DataStorageOperation*)malloc(sizeof(struct AP_DataStorageOperation));
    if (operation_struct != NULL) {
        operation_struct->operation = _strdup(operation);
        operation_struct->value = value;
    }
    return operation_struct;
}

void AP_DataStorageOperation_free(struct AP_DataStorageOperation* operation_struct) {
    if (operation_struct != NULL) {
        free(operation_struct->operation);
        free(operation_struct);
    }
}

// Function to create a new AP_RoomInfo instance
struct AP_RoomInfo* AP_RoomInfo_new(struct AP_NetworkVersion version, GArray* tags, bool password_required, GHashTable* permissions, int hint_cost, int location_check_points, GArray* games, GHashTable* datapackage_checksums, const char* seed_name, double time) {
    struct AP_RoomInfo* room_info = (struct AP_RoomInfo*)malloc(sizeof(struct AP_RoomInfo));
    if (room_info != NULL) {
    room_info->version = version;
    room_info->tags = tags;
    room_info->password_required = password_required; 
    room_info->permissions = permissions;
    room_info->hint_cost = hint_cost; 
    room_info->location_check_points = location_check_points;
    room_info->games = games;
    room_info->datapackage_checksums = datapackage_checksums;
    room_info->seed_name = _strdup(seed_name);
    room_info->time = time;
    }
    return room_info;
}

// Function to free memory allocated for AP_RoomInfo instance
void AP_RoomInfo_free(struct AP_RoomInfo* room_info) {
    if (room_info) {
        if (room_info->tags)
            g_array_free(room_info->tags, TRUE);
        if (room_info->permissions)
            g_hash_table_destroy(room_info->permissions);
        if (room_info->games)
            g_array_free(room_info->games, TRUE);
        if (room_info->datapackage_checksums)
            g_hash_table_destroy(room_info->datapackage_checksums);
        g_free(room_info->seed_name);
        g_free(room_info);
    }
}

struct pair_char_uint64_t* pair_char_int64_t_new(const char* name, uint64_t id) {
    struct pair_char_uint64_t* newPair = (struct pair_char_uint64_t*)malloc(sizeof(struct pair_char_uint64_t));
    if (newPair != NULL) {
        newPair->name = _strdup(name);
        newPair->id = id;
    }
    return newPair;
}

void pair_char_uint64_t_free(struct pair_char_uint64_t* pair) {
    if (pair != NULL) {
        free(pair->name);
        free(pair);
    }
}

#define AP_OFFLINE_SLOT 1404
#define AP_OFFLINE_NAME "You"

//Setup Stuff
bool init = false;
bool auth = false;
bool refused = false;
bool connected = false;
bool multiworld = true;
bool isSSL = true;
bool ssl_success = false;
bool data_synced = false;
bool ap_ready = false;
int ap_player_id;
const char* ap_player_name;
const char* ap_ip;
int ap_port;
const char* ap_game;
const char* ap_passwd;
uint64_t ap_uuid = 0;
GRand* seeded_rand;


struct AP_NetworkVersion* client_version; // Default for compatibility reasons

//Deathlink Stuff
bool deathlinkstat = false;
bool deathlinksupported = false;
bool enable_deathlink = false;
int deathlink_amnesty = 0;
int cur_deathlink_amnesty = 0;

// Message System
//std::deque<AP_Message*> messageQueue;
GQueue* messageQueue; //Constr: g_queue_new();
bool queueitemrecvmsg = true;

GString* message_buffer = NULL;
uint64_t remaining_prev = 0;
GQueue* outgoing_queue = NULL;
json_t* request = NULL;
json_error_t jerror;

// Data Maps
//std::map<int, AP_NetworkPlayer> map_players;
GArray* map_players = NULL;
GHashTable* map_game_to_data = NULL;

// Callback function pointers
void (*resetItemValues)();
void (*getitemfunc)(uint64_t, int, bool);
void (*checklocfunc)(uint64_t);
void (*locinfofunc)(GArray*) = NULL;
void (*recvdeath)() = NULL;
void (*setreplyfunc)(struct AP_SetReply*) = NULL;

// Serverdata Management
GHashTable* map_serverdata_typemanage = NULL;
struct AP_GetServerDataRequest* resync_serverdata_request;
uint64_t last_item_idx = 0;

// Singleplayer Seed Info
char* sp_save_path;
//Json::Value sp_save_root;
json_t* sp_save_root;

//Misc Data for Clients
struct AP_RoomInfo lib_room_info;

//Server Data Stuff
GHashTable* map_server_data = NULL;
//std::map<std::string, AP_GetServerDataRequest*> map_server_data;

//Slot Data Stuff
GHashTable* map_slotdata_callback_int = NULL; //std::map<std::string, void (*)(int)> map_slotdata_callback_int;
GHashTable* map_slotdata_callback_raw = NULL; //std::map<std::string, void (*)(std::string)> map_slotdata_callback_raw;
GHashTable* map_slotdata_callback_intarray = NULL; //std::map<std::string, void (*)(std::map<int, int>)> map_slotdata_callback_intarray;
GArray* slotdata_strings = NULL; //std::vector<std::string> slotdata_strings;
GHashTableIter it;

// Caching
const char* datapkg_cache_path = "APCc_datapkg.cache";
json_t* datapkg_cache;


static struct lws* web_socket = NULL;

json_t* sp_ap_root;

// PRIV Func Declarations Start
void AP_Init_Generic();
bool parse_response(json_t* root);
//void APSend(char* req);
char* getItemName(const char* gamename, uint64_t id);
char* getLocationName(const char* gamename, uint64_t id);
void parseDataPkg(json_t* new_datapkg);
void parseDataPkgCache();
struct AP_NetworkPlayer* getPlayer(int team, int slot);
void localSetServerData(json_t* req);
GArray* messageParts = NULL; 
char* messagePartsToPlainText(GArray* messageParts);
// PRIV Func Declarations End

// websocket
static struct json_t* rec_in;
struct lws_context* context;

enum protocols
{
    PROTOCOL_AP = 0,
    PROTOCOL_COUNT
};

struct per_session_data {
    GString* message_buffer;
    bool is_processing;
};

#define EXAMPLE_TX_BUFFER_BYTES 10
int recv_count = 0;
static int lws_callbacks(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    struct per_session_data* psd = (struct per_session_data*)user;

    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        if (!psd) {
            lwsl_err("No user session data\n");
            return -1;
        }
        psd->message_buffer = NULL;
        psd->is_processing = false;
        connected = true;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        recv_count++;
        if (!in || len == 0) {
            lwsl_warn("Empty receive buffer\n");
            return 0;
        }
        if (!psd) {
            lwsl_err("No user session data\n");
            return -1;
        }

        size_t remaining = lws_remaining_packet_payload(wsi);

        if (lws_is_first_fragment(wsi)) {

            if (len + remaining > MAX_PAYLOAD_SIZE) {
                //lwsl_warn("Large message incoming: %zu bytes\n", len + remaining);
            }
            psd->message_buffer = g_string_new_len(NULL, len + remaining);
            psd->is_processing = true;
        }

        if (psd->message_buffer) {
            g_string_append_len(psd->message_buffer, (const gchar*)in, len);
        }

        // Process when complete
        if (lws_is_final_fragment(wsi))
        {
            if (psd->message_buffer) {
                char* current_pos = psd->message_buffer->str;
                char* end_pos = psd->message_buffer->str + psd->message_buffer->len;

                while (current_pos < end_pos) {
                    json_t* json = json_loads(current_pos, JSON_DISABLE_EOF_CHECK, &jerror);

                    if (!json) {
                        if (jerror.text != NULL) {
                            lwsl_warn("JSON parse error: %s at position %zd. Trying to recover.\n", jerror.text, jerror.position);
                            if (jerror.position > 0 && jerror.position < psd->message_buffer->len) {
                                current_pos += jerror.position;
                                while (current_pos < end_pos && isspace(*current_pos)) {
                                    current_pos++;
                                }
                                if (current_pos < end_pos && *current_pos != '[') { // Try finding the next  '['
                                    while (current_pos < end_pos && *current_pos != '[') {
                                        current_pos++;
                                    }
                                }
                                if (current_pos >= end_pos) {
                                    break;
                                }
                                continue;
                            }
                            else {
                                lwsl_err("Unrecoverable JSON parse error: %s.\n", jerror.text);
                                break;
                            }
                        }
                        else {
                            lwsl_err("Unknown JSON parse error.\n");
                            break;
                        }
                    }
                    else {
                        printf("in: %s \n\n\n", current_pos);
                        parse_response(json);
                        json_decref(json);
                        current_pos += jerror.position; // Advance using the error.position
                        while (current_pos < end_pos && isspace(*current_pos)) {
                            current_pos++; // Skip whitespace
                        }
                    }
                }

                g_string_free(psd->message_buffer, TRUE);
                psd->message_buffer = NULL;
                psd->is_processing = false;
            }
        }
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        unsigned char* buf = malloc(LWS_PRE + WRITE_BUFFER_SIZE);
        if (!buf) {
            lwsl_err("Buffer allocation failed\n");
            return -1;
        }
        unsigned char* write_buf = &buf[LWS_PRE];

        while (!g_queue_is_empty(outgoing_queue)) {
            json_t* json_out = g_queue_peek_head(outgoing_queue);
            if (!json_out) {
                continue;
            }

            char* msg_out = json_dumps(json_out, JSON_COMPACT);
            if (!msg_out) {
                lwsl_err("JSON serialization failed\n");
                free(msg_out);
                continue;
            }

            size_t msg_len = strlen(msg_out);
            if (msg_len > WRITE_BUFFER_SIZE) {
                lwsl_err("Message too large for buffer: %zu bytes\n", msg_len);
                free(msg_out);
                continue;
            }

            memcpy(write_buf, msg_out, msg_len);
            int written = lws_write(wsi, write_buf, msg_len, LWS_WRITE_TEXT);
            printf("out: %s \n\n\n",msg_out);
            free(msg_out);
            json_decref(json_out);
            g_queue_pop_head(outgoing_queue);
            if (written < msg_len) {
                lwsl_err("Write failed: %d/%zu\n", written, msg_len);
                continue;
            }
            // TODO: Is this necessary?
            // Check if we need more write events
            if (!g_queue_is_empty(outgoing_queue)) {
                lws_callback_on_writable(wsi);
            }
        }
        free(buf);
        break;
    case LWS_CALLBACK_CLOSED:
        auth = false;
        connected = false;
        web_socket = NULL;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        if (psd && psd->message_buffer) {
            g_string_free(psd->message_buffer, TRUE);
            psd->message_buffer = NULL;
        }
        auth = false;
        connected = false;
        web_socket = NULL;
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        if (psd && psd->message_buffer) {
            g_string_free(psd->message_buffer, TRUE);
            psd->message_buffer = NULL;
        }
        auth = false;
        connected = false;
        web_socket = NULL;
        isSSL = !isSSL;
        break;

    default:
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] =
{
    {
        .name = "archipelago-protocol",     /* Protocol name*/
        .callback = lws_callbacks,      /* Protocol callback */
        .per_session_data_size = sizeof(struct per_session_data),     /* Protocol callback 'userdata' size */
        .rx_buffer_size = 0,            /* Receive buffer size (0 = no restriction) */
        .id = 0,                        /* Protocol Id (version) (optional) */
        .user = NULL,                   /* 'User data' ptr, to access in 'protocol callback */
        .tx_packet_size = 0              /* Transmission buffer size restriction (0 = no restriction) */
    },
    LWS_PROTOCOL_LIST_TERM /* terminator */
};

static const struct lws_extension extensions[] = {
        {
                "permessage-deflate", lws_extension_callback_pm_deflate,
                "permessage-deflate" "; client_no_context_takeover"
                 "; client_max_window_bits"
        },
        { NULL, NULL, NULL /* terminator */ }
};

// SUL Websocket Callback
struct sul_s {
    lws_sorted_usec_list_t sul;
    struct lws_context* context;
    int counter;
    int64_t timeout_us;
};
static struct sul_s sul_data;

// Callback function that will be called by sul
static void sul_cb(lws_sorted_usec_list_t* sul)
{
    struct sul_s* sulstruct = lws_container_of(sul, struct sul_s, sul);
    lws_sul_schedule(sulstruct->context, 0, &sulstruct->sul, sul_cb, sulstruct->timeout_us);
}

int AP_WebsocketSulInit(uint32_t timeout_ms)
{
    memset(&sul_data, 0, sizeof(sul_data));

    sul_data.context = context;
    sul_data.counter = 0;

    sul_data.timeout_us = (int64_t)timeout_ms * 1000;

    lws_sul_schedule(context, 0, &sul_data.sul, sul_cb, sul_data.timeout_us);

    return TRUE;
}

void AP_WebsocketSulCleanup()
{
    // Cancel any pending sul callback
    lws_sul_schedule(context, 0, &sul_data.sul, NULL, LWS_SET_TIMER_USEC_CANCEL);
}

// Main service loop - required for SUL to work
void service_loop()
{
    while (context) {
        // Service any pending events (including SUL callbacks)
        AP_WebService();
        // Small sleep to prevent CPU spinning
        Sleep(1);
    }
}

struct send_data_sul {
    lws_sorted_usec_list_t sul;
    struct lws_context* context;
    struct lws* wsi;
    const char* data;
    size_t len;
};

static void send_data_cb(lws_sorted_usec_list_t* sul)
{
    struct send_data_sul* send_data = lws_container_of(sul, struct send_data_sul, sul);

    if (send_data->wsi) {
        // Request a writable callback
        lws_callback_on_writable(send_data->wsi);

        // Reschedule for next send
        // Example: Send every 100ms (100000 microseconds)
        lws_sul_schedule(send_data->context, 0, &send_data->sul, send_data_cb, 100000);
    }
}

// Initialize periodic data sending
void start_periodic_send(struct lws* wsi, const char* data, size_t len)
{
    static struct send_data_sul send_data;

    memset(&send_data, 0, sizeof(send_data));
    send_data.context = context;
    send_data.wsi = wsi;
    send_data.data = data;
    send_data.len = len;

    // Schedule first send
    lws_sul_schedule(context, 0, &send_data.sul, send_data_cb, 100000);
}

void AP_SetClientVersion(struct AP_NetworkVersion* version) {
    if (!client_version) { client_version = AP_NetworkVersion_new(0, 2, 6); }
    client_version->major = version->major;
    client_version->minor = version->minor;
    client_version->build = version->build;
}

void AP_SendWeb()
{
    if (web_socket) lws_callback_on_writable(web_socket);
}

//TODO: Implement SP
void AP_SendItem(uint64_t idx) {
    if (multiworld) {
        json_t* req_t = json_object();
        json_t* req_array = json_array();
        json_t* req_locations_array = json_array();
        json_object_set_new(req_t, "cmd", json_string("LocationChecks"));
        json_array_append_new(req_locations_array, json_integer(idx));
        json_object_set_new(req_t, "locations", req_locations_array);
        json_array_append_new(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else {
        /*
        for (auto itr : sp_save_root["checked_locations"]) {
            if (itr.asInt64() == idx) {
                return;
            }
        }
        int64_t recv_item_id = sp_ap_root["location_to_item"].get(std::to_string(idx), 0).asInt64();
        if (recv_item_id == 0) return;
        Json::Value fake_msg;
        fake_msg[0]["cmd"] = "ReceivedItems";
        fake_msg[0]["index"] = last_item_idx + 1;
        fake_msg[0]["items"][0]["item"] = recv_item_id;
        fake_msg[0]["items"][0]["location"] = idx;
        fake_msg[0]["items"][0]["player"] = ap_player_id;
        std::string req;
        parse_response(writer.write(fake_msg), req);
        sp_save_root["checked_locations"].append(idx);
        WriteFileJSON(sp_save_root, sp_save_path);
        fake_msg.clear();
        fake_msg[0]["cmd"] = "RoomUpdate";
        fake_msg[0]["checked_locations"][0] = idx;
        parse_response(writer.write(fake_msg), req);*/
    }
}

void AP_SendMsg(char* msg_in) {
    if (multiworld) {
        json_t* req_t = json_object();
        json_t* req_array = json_array();
        json_object_set_new(req_t, "cmd", json_string("Say"));
        json_object_set_new(req_t, "text", json_string(msg_in));
        json_array_append_new(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else {
        // TODO: Implement SP
    }
}

//TODO: Implement SP
void AP_SendLocationScouts(GArray* locations, int create_as_hint) {
    if (multiworld) {
        json_t* req_t = json_object();
        json_t* req_array = json_array();
        json_t* req_locations_array = json_array();
        json_object_set_new(req_t, "cmd", json_string("LocationScouts"));
        for (guint i = 0; i < locations->len; i++)
        {
            json_array_append_new(req_locations_array, json_integer(g_array_index(locations, uint64_t, i)));
        }
        
        json_object_set_new(req_t, "locations", req_locations_array);
        json_object_set_new(req_t, "create_as_hint", json_integer(create_as_hint));
        json_array_append_new(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else {
        /*
        Json::Value fake_msg;
        fake_msg[0]["cmd"] = "LocationInfo";
        fake_msg[0]["locations"] = Json::arrayValue;
        for (int64_t loc : locations) {
            Json::Value netitem;
            netitem["item"] = sp_ap_root["location_to_item"].get(std::to_string(loc), 0).asInt64();
            netitem["location"] = loc;
            netitem["player"] = ap_player_id;
            netitem["flags"] = 0b001; // Hardcoded for SP seeds.
            fake_msg[0]["locations"].append(netitem);
        }*/
    }
}

void AP_SendLocationScout(uint64_t location, int create_as_hint) {
    if (multiworld) {
        json_t* req_t = json_object();
        json_t* req_array = json_array();
        json_t* req_locations_array = json_array();
        json_object_set_new(req_t, "cmd", json_string("LocationScouts"));
        json_array_append_new(req_locations_array, json_integer(location));
        json_object_set_new(req_t, "locations", req_locations_array);
        json_object_set_new(req_t, "create_as_hint", json_integer(create_as_hint));
        json_array_append_new(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else {
        /*
        Json::Value fake_msg;
        fake_msg[0]["cmd"] = "LocationInfo";
        fake_msg[0]["locations"] = Json::arrayValue;
        for (int64_t loc : locations) {
            Json::Value netitem;
            netitem["item"] = sp_ap_root["location_to_item"].get(std::to_string(loc), 0).asInt64();
            netitem["location"] = loc;
            netitem["player"] = ap_player_id;
            netitem["flags"] = 0b001; // Hardcoded for SP seeds.
            fake_msg[0]["locations"].append(netitem);
        }*/
    }
}

void AP_StoryComplete() {
    if (!multiworld) return;
    json_t* req_t = json_object();
    json_t* req_array = json_array();
    json_object_set_new(req_t, "cmd", json_string("StatusUpdate"));
    json_object_set_new(req_t, "status", json_integer(30));
    json_array_append_new(req_array, req_t);
    g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
    AP_SendWeb();
}

void AP_DeathLinkSend() {
    if (!enable_deathlink || !multiworld) return;
    if (cur_deathlink_amnesty > 0) {
        cur_deathlink_amnesty--;
        return;
    }
    cur_deathlink_amnesty = deathlink_amnesty;
    gint64 timestamp_sec = g_get_real_time() / G_USEC_PER_SEC;
    json_t* req_t = json_object();
    json_t* req_array = json_array();
    json_t* time_obj = json_object();
    json_t* source_obj = json_object();
    json_t* req_data_obj = json_object();
    json_t* req_tags_array = json_array();
    json_object_set_new(req_t, "cmd", json_string("Bounce"));
    json_object_set_new(req_data_obj, "time", json_integer(timestamp_sec));
    json_object_set_new(req_data_obj, "source", json_string(ap_player_name));
    json_object_set_new(req_t, "data", req_data_obj);
    json_array_append_new(req_tags_array, json_string("DeathLink"));
    json_object_set_new(req_t, "tags", req_tags_array);
    json_array_append_new(req_array, req_t);
    g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
    AP_SendWeb();
}

//TODO: Implement SP
void AP_Init_SP(const char* filename) {
    multiworld = false;
    char sp_save_buf[sizeof(".save") + sizeof(filename)];
    sprintf(sp_save_buf, "%s%s", filename, ".save");
    sp_ap_root = json_load_file(filename, 0, &jerror);
    sp_save_path = sp_save_buf;
    sp_save_root = json_load_file(sp_save_path, 0, &jerror);
    json_dump_file(sp_save_root, sp_save_path, 0);
    ap_player_name = AP_OFFLINE_NAME;
    AP_Init_Generic();
}

bool AP_IsInit() {
    return init;
}

void AP_WebService() {
    /* Connect if we are not connected to the server. */
    if (!web_socket)
    {
        struct lws_client_connect_info ccinfo;
        memset(&ccinfo, 0, sizeof(ccinfo));
        ccinfo.context = context;
        ccinfo.port = ap_port;
        ccinfo.path = "/";
        ccinfo.host = lws_canonical_hostname(context);
        ccinfo.address = ap_ip;
        ccinfo.origin = "origin";
        ccinfo.protocol = protocols[PROTOCOL_AP].name;
        if (isSSL) ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        else ccinfo.ssl_connection = 0;
        web_socket = lws_client_connect_via_info(&ccinfo);
        lws_service(context, 0);
    }
    else if (web_socket)
    {
        lws_service(context, 0);
    }
}

////TODO: Implement SP, currently has no use in MP
void AP_Start() {
    init = true;
    ap_ready = true;
    if (multiworld) {
        // Websocket is handled by AP_Init
        //webSocket.start();
    }
    else {
        /*
        if (!sp_save_root.get("init", false).asBool()) {
            sp_save_root["init"] = true;
            sp_save_root["checked_locations"] = Json::arrayValue;
            sp_save_root["store"] = Json::objectValue;
        }
        // Seed for savegame names etc
        lib_room_info.seed_name = sp_ap_root["seed"].asString();
        Json::Value fake_msg;
        fake_msg[0]["cmd"] = "Connected";
        fake_msg[0]["slot"] = AP_OFFLINE_SLOT;
        fake_msg[0]["players"] = Json::arrayValue;
        fake_msg[0]["players"][0]["team"] = 0;
        fake_msg[0]["players"][0]["slot"] = AP_OFFLINE_SLOT;
        fake_msg[0]["players"][0]["alias"] = AP_OFFLINE_NAME;
        fake_msg[0]["players"][0]["name"] = AP_OFFLINE_NAME;
        fake_msg[0]["checked_locations"] = sp_save_root["checked_locations"];
        fake_msg[0]["slot_data"] = sp_ap_root["slot_data"];
        std::string req;
        parse_response(writer.write(fake_msg), req);
        fake_msg.clear();
        fake_msg[0]["cmd"] = "DataPackage";
        fake_msg[0]["data"] = sp_ap_root["data_package"]["data"];
        parse_response(writer.write(fake_msg), req);
        fake_msg.clear();
        fake_msg[0]["cmd"] = "ReceivedItems";
        fake_msg[0]["index"] = 0;
        fake_msg[0]["items"] = Json::arrayValue;
        for (unsigned int i = 0; i < sp_ap_root["start_inventory"].size(); i++) {
            Json::Value item;
            item["item"] = sp_ap_root["start_inventory"][i].asInt64();
            item["location"] = 0;
            item["player"] = ap_player_id;
            fake_msg[0]["items"].append(item);
        }
        for (unsigned int i = 0; i < sp_save_root["checked_locations"].size(); i++) {
            Json::Value item;
            item["item"] = sp_ap_root["location_to_item"][sp_save_root["checked_locations"][i].asString()].asInt64();
            item["location"] = 0;
            item["player"] = ap_player_id;
            fake_msg[0]["items"].append(item);
        }
        parse_response(writer.write(fake_msg), req);*/
    }
}

void AP_Init(const char* ip, int port, const char* game, const char* player_name, const char* passwd)
{
   // lws_set_log_level(LLL_DEBUG | LLL_INFO | LLL_NOTICE | LLL_WARN | LLL_ERR, NULL);
    multiworld = true;
    if (!slotdata_strings) { slotdata_strings = g_array_new(true, true, sizeof(GString*)); }
    if (!sp_save_root) { sp_save_root = json_object(); }
    if (!messageQueue) { messageQueue = g_queue_new(); }
    if (!outgoing_queue) { outgoing_queue = g_queue_new(); }

    g_random_set_seed((guint32)time(NULL));
    seeded_rand = g_rand_new();
    
    
    map_slotdata_callback_int = g_hash_table_new(g_string_hash, g_string_equal);
    map_slotdata_callback_raw = g_hash_table_new(g_string_hash, g_string_equal);
    map_slotdata_callback_intarray = g_hash_table_new(g_string_hash, g_string_equal);
    
    ap_ip = ip;
    ap_port = port;
    ap_game = game;
    ap_player_name = player_name;
    ap_passwd = passwd;

    //Connect to server

    // Connection info
    struct lws_context_creation_info lws_info;
    memset(&lws_info, 0, sizeof(lws_info));

    lws_info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    lws_info.protocols = protocols;
    lws_info.gid = -1;
    lws_info.uid = -1;
    lws_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    lws_info.extensions = extensions;

    context = lws_create_context(&lws_info);

    struct AP_NetworkPlayer* archipelago = AP_NetworkPlayer_new(-1, 0, "Archipelago", "Archipelago", "__Server");
    map_players = g_array_new(true, true, sizeof(struct AP_NetworkPlayer*));
    g_array_append_val(map_players, archipelago);
    AP_Init_Generic();

    
}

void AP_Init_Generic() {
    datapkg_cache = json_load_file(datapkg_cache_path, 0, &jerror);
    if (!datapkg_cache) {
        printf("Parsing error on file %s: %s", datapkg_cache_path, jerror.text);
        return;
    }
    return;
}

bool parse_response(json_t* root)
{
    request = json_array();
    // Received a valid json
    for (int i = 0; i < json_array_size(root); i++)
    {
        json_t* obj = json_array_get(root, i);
        char* cmd = NULL;
        json_t* temp_obj = NULL;
        temp_obj = json_object_get(obj,"cmd");
        if (temp_obj) { cmd = _strdup(json_string_value(temp_obj)); } else { printf("Error retreiving array element %i of incoming request.", i); return 0; }
        if (cmd && !strcmp(cmd, "RoomInfo"))
        {
            if ((temp_obj = json_object_get(obj, "password"))) { lib_room_info.password_required = json_integer_value(obj); }
            if ((temp_obj = json_object_get(obj, "games"))) 
            {
                GArray* serv_games = g_array_new(true, true, sizeof(char*));
                size_t j;
                json_t* v;
                json_array_foreach(temp_obj, j, v) 
                {
                    char* game_val =  _strdup(json_string_value(v));
                    g_array_append_val(serv_games, game_val);
                }
                lib_room_info.games = serv_games;
            }
            
            if ((temp_obj = json_object_get(obj, "tags")))
            {
                GArray* serv_tags = g_array_new(true, true, sizeof(char*));
                size_t index;
                json_t* value;
                json_array_foreach(temp_obj, index, value)
                {
                    char* tag_val = _strdup(json_string_value(value));
                    g_array_append_val(serv_tags, tag_val);
                }
                lib_room_info.tags = serv_tags;
            }
            
            if ((temp_obj = json_object_get(obj, "version")))
            {
                const char* k;
                json_t* v;
                json_object_foreach(temp_obj, k, v)
                {
                    if (!strcmp(k, "major")) { lib_room_info.version.major = (int)json_integer_value(v); }
                    if (!strcmp(k, "minor")) { lib_room_info.version.minor = (int)json_integer_value(v); }
                    if (!strcmp(k, "build")) { lib_room_info.version.build = (int)json_integer_value(v); }
                }
            }
            
            if ((temp_obj = json_object_get(obj, "permissions")))
            {
                GHashTable* serv_perms = g_hash_table_new(g_string_hash, g_string_equal);
                const char* k;
                json_t* v;
                json_object_foreach(temp_obj, k, v)
                {
                    GString* gs_key = g_string_new(k);
                    uint64_t val = json_integer_value(v);
                    g_hash_table_insert(serv_perms, gs_key, &val);
                }
                lib_room_info.permissions = serv_perms;
            }

            
            if ((temp_obj = json_object_get(obj, "hint_cost"))) { lib_room_info.hint_cost = (int)json_integer_value(temp_obj); }
            if ((temp_obj = json_object_get(obj, "location_check_points"))) { lib_room_info.location_check_points = (int)json_integer_value(temp_obj); }
            
            if ((temp_obj = json_object_get(obj, "datapackage_checksums")))
            {
                GHashTable* serv_datapkg_checksums = g_hash_table_new(g_string_hash, g_string_equal);
                const char* k;
                json_t* v;
                json_object_foreach(temp_obj, k, v)
                {
                    GString* gs_key = g_string_new(k);
                    g_hash_table_insert(serv_datapkg_checksums, gs_key, _strdup(json_string_value(v)));
                }
                lib_room_info.datapackage_checksums = serv_datapkg_checksums;
            }
            
            if ((temp_obj = json_object_get(obj, "seed_name"))) { lib_room_info.seed_name = _strdup(json_string_value(temp_obj)); }
            if ((temp_obj = json_object_get(obj, "time"))) { lib_room_info.time = json_number_value(temp_obj); }
            if (!auth)
            {
                json_t* req_t = json_object();
                json_t* tags_array = json_array();
                json_t* version_obj = json_object();
                json_array_append_new(tags_array, json_string("AP"));
                ap_uuid = (((uint64_t)g_rand_int(seeded_rand) << 32) | g_rand_int(seeded_rand)) & 0x7FFFFFFFFFFFFFFF;
                json_object_set_new(req_t, "cmd", json_string("Connect"));
                json_object_set_new(req_t, "game", json_string(ap_game));
                json_object_set_new(req_t, "name", json_string(ap_player_name));
                json_object_set_new(req_t, "password", json_string(ap_passwd));
                json_object_set_new(req_t, "uuid", json_integer(ap_uuid));
                json_object_set_new(req_t, "tags", tags_array);
                json_object_set_new(version_obj, "major", json_integer(client_version->major));
                json_object_set_new(version_obj, "minor", json_integer(client_version->minor));
                json_object_set_new(version_obj, "build", json_integer(client_version->build));
                json_object_set_new(version_obj, "class", json_string("Version"));
                json_object_set_new(req_t, "version", version_obj);
                json_object_set_new(req_t, "items_handling", json_integer(7));
                json_array_append_new(request, req_t);
                g_queue_push_tail(outgoing_queue, json_deep_copy(request));
                AP_SendWeb();
                return true;
            }
            
        }
        else if (cmd && !strcmp(cmd, "Connected"))
        {
            // Waiting for call to AP_Start()
            while (!ap_ready) {   Sleep(100); }
            if ((temp_obj = json_object_get(obj, "slot"))) { ap_player_id = (int)json_integer_value(temp_obj); }
            // Avoid inconsistency if we disconnected before
            (*resetItemValues)();
            auth = true;
            ssl_success = auth && isSSL;
            refused = false;
            
            if ((temp_obj = json_object_get(obj, "checked_locations")))
            {
                size_t j;
                json_t* v;
                json_array_foreach(temp_obj, j, v)
                {
                    uint64_t loc_id = json_integer_value(v);
                    (*checklocfunc)(loc_id);
                }
            }
            if ((temp_obj = json_object_get(obj, "players")))
            {
                size_t j;
                json_t* v;
                json_array_foreach(temp_obj, j, v)
                {
                    int team = 0;
                    int slot = 0;
                    char* alias = NULL;
                    char* name = NULL;
                    char* class = NULL;
                    const char* kp;
                    json_t* vp;
                    json_object_foreach(v, kp, vp)
                    {
                        if (!strcmp(kp, "team")) { team = (int)json_integer_value(vp); }
                        if (!strcmp(kp, "slot")) { slot = (int)json_integer_value(vp); }
                        if (!strcmp(kp, "alias")) { alias = _strdup(json_string_value(vp)); }
                        if (!strcmp(kp, "name")) { name = _strdup(json_string_value(vp)); }
                        if (!strcmp(kp, "class")) { class = _strdup(json_string_value(vp)); }
                    }
                    struct AP_NetworkPlayer* player_struct = AP_NetworkPlayer_new(team, slot, alias, name, "undefined");
                    g_array_append_val(map_players, player_struct);
                }
            }
            if ((temp_obj = json_object_get(obj, "slot_info")))
            {
                int player_id = 1;
                const char* k;
                json_t* v;
                json_t* loop_obj;
                json_object_foreach(temp_obj, k, v)
                {
                    loop_obj = json_object_get(v, "game");
                    g_array_index(map_players, struct AP_NetworkPlayer*, player_id)->game = _strdup(json_string_value(loop_obj));
                    player_id++;
                }
            }
            if ((temp_obj = json_object_get(obj, "slot_data")))
            {
                const char* k;
                json_t* v;
                json_t* slot_data_obj;
                json_object_foreach(temp_obj, k, v)
                {
                    if (!strcmp("death_link", k)) 
                        enable_deathlink = (int)json_integer_value(v) && deathlinksupported;
                    else if (!strcmp("DeathLink", k)) 
                        enable_deathlink = (int)json_integer_value(v) && deathlinksupported;

                    if (!strcmp("death_link_amnesty", k)) {
                        deathlink_amnesty = (int)json_integer_value(v); cur_deathlink_amnesty = deathlink_amnesty;
                    }
                    else if (!strcmp("DeathLink_Amnesty", k)) {
                        deathlink_amnesty = (int)json_integer_value(v); cur_deathlink_amnesty = deathlink_amnesty;
                    }
                }
                // Iterate over keys we are searching for callbacks
                for (guint j = 0; j < slotdata_strings->len; j++)
                {
                    if ((slot_data_obj = json_object_get(temp_obj, g_array_index(slotdata_strings, GString*, j)->str)))
                    {
                        //Found a callback key in slot_data
                        
                        GString* callback_key = g_array_index(slotdata_strings, GString*, j);
                        gpointer callback_func_ptr = NULL;
                        if ((callback_func_ptr = (void*)g_hash_table_lookup(map_slotdata_callback_raw, callback_key)) != NULL)
                        {
                            // JSON Object callback, no type checking required
                            ((void(*)(json_t*))callback_func_ptr)(slot_data_obj);
                        }
                        
                        if ((callback_func_ptr = (void*)g_hash_table_lookup(map_slotdata_callback_int, callback_key)) != NULL)
                        {
                            // Int callback
                            if (json_is_integer(slot_data_obj))
                            {
                                //Only return data if type is int
                                uint64_t json_val = (uint64_t)json_integer_value(slot_data_obj);
                                uint64_t* read_val = &json_val;
                                ((void(*)(uint64_t*))callback_func_ptr)(read_val);
                            }
                            else { printf("AP_RegisterSlotDataIntCallback key %s has wrong type: %s", callback_key->str, jtype_to_string(slot_data_obj)); }
                        }
                        
                        if ((callback_func_ptr = (void*)g_hash_table_lookup(map_slotdata_callback_intarray, callback_key)) != NULL)
                        {
                            // Array callback
                            if(json_is_array(slot_data_obj))
                            {
                                //Only return data if base type is array
                                GArray* j_array = g_array_new(false, false, sizeof(uint64_t));
                                size_t key;
                                json_t* val;
                                json_array_foreach(slot_data_obj, key, val)
                                {
                                    if (json_is_integer(val))
                                    {
                                        uint64_t read_val = json_integer_value(val);
                                        g_array_append_val(j_array, read_val);
                                    }
                                    else { printf("AP_RegisterSlotDataIntArrayCallback array element in %s has wrong type: %s", callback_key->str, jtype_to_string(val)); }
                                }
                                ((void(*)(GArray*))callback_func_ptr)(j_array);
                            }
                            else { printf("AP_RegisterSlotDataIntArrayCallback key element %s has wrong type: %s", callback_key->str, jtype_to_string(slot_data_obj)); }
                        }
                    }
                }
            }//end of slotdata parse
            
            GString* resync_key = g_string_new("APCcLastRecv");
            g_string_append(resync_key, ap_player_name);
            g_string_append_printf(resync_key, "%i", ap_player_id);
            resync_serverdata_request = AP_GetServerDataRequest_new(Pending, resync_key->str, &last_item_idx, Int);
            AP_GetServerData(resync_serverdata_request);
            
            // Get datapackage for outdated games
            struct AP_RoomInfo* info = AP_RoomInfo_new(*AP_NetworkVersion_new(0, 0, 0), NULL, false, NULL, 0, 0, NULL, NULL, NULL, 0);
            AP_GetRoomInfo(info);
            //json_t* req_t = json_array();
            
            if (enable_deathlink && deathlinksupported)
            {
                json_t* setdeathlink = json_object();
                json_t* tagsarray = json_array();
                json_object_set_new(setdeathlink, "cmd", json_string("ConnectUpdate"));
                json_array_append_new(tagsarray, json_string("DeathLink"));
                json_object_set_new(setdeathlink, "tags", tagsarray);
                json_array_append_new(request, setdeathlink);
            }
            
            bool cache_outdated = false;
            json_t* resync_datapkg = json_object();
            json_t* resync_game_array = json_array();
            GString* ht_key;
            char* ht_value;
            g_hash_table_iter_init(&it, info->datapackage_checksums);
            
            while (g_hash_table_iter_next(&it, &ht_key, &ht_value))
            {
                // If no cache is present we need a full datapackage of all games
                if (!datapkg_cache)
                {
                    if (!cache_outdated) { json_object_set_new(resync_datapkg, "cmd", json_string("GetDataPackage")); }
                    cache_outdated = true;
                    json_array_append_new(resync_game_array, json_string(ht_key->str));
                }
                // Check if the game is present in our cache
                else
                {
                    bool found_game = false;
                    const char* dpkgc_k;
                    json_t* dpkgc_v;
                    json_t* loop_obj;
                    json_t* checksum_obj;
                    json_object_foreach(datapkg_cache, dpkgc_k, dpkgc_v)
                    {
                        if ((loop_obj = json_object_get(dpkgc_v, ht_key->str)))
                        { 
                            //Found game in cache, compare checksums
                            found_game = true;
                            checksum_obj = json_object_get(loop_obj, "checksum");
                            //Create resync packet if mismatched checksum is found
                            if (strcmp(json_string_value(checksum_obj), ht_value)) 
                            {
                                if (!cache_outdated) { json_object_set_new(resync_datapkg, "cmd", json_string("GetDataPackage")); }
                                cache_outdated = true;
                                json_array_append_new(resync_game_array, json_string(ht_key->str));
                            }
                        }
                    }
                    // Did not find game in cache, request datapackage
                    if (!found_game) 
                    { 
                        if (!cache_outdated) { json_object_set_new(resync_datapkg, "cmd", json_string("GetDataPackage")); }
                        cache_outdated = true;
                        json_array_append_new(resync_game_array, json_string(ht_key->str));
                    }
                }
            }
            
            if (cache_outdated)
            {
                json_object_set_new(resync_datapkg, "games", resync_game_array); 
                json_array_append_new(request, resync_datapkg);
            }
            else
            {
                parseDataPkgCache();
                json_t* sync_obj = json_object();
                json_object_set_new(sync_obj, "cmd", json_string("Sync"));
                json_array_append_new(request, sync_obj);
                data_synced = true;
            }
            g_queue_push_tail(outgoing_queue, json_deep_copy(request));
            AP_SendWeb();
            return true;
        }
        else if (cmd && !strcmp(cmd, "DataPackage")) 
        {           
            parseDataPkg(obj);
            json_t* sync_obj = json_object();
            json_object_set_new(sync_obj, "cmd", json_string("Sync"));
            json_array_append_new(request, sync_obj);
            g_queue_push_tail(outgoing_queue, json_deep_copy(request));
            AP_SendWeb();
            data_synced = true;
            return true;
        }
        else if (cmd && !strcmp(cmd, "Retrieved"))
        {
            json_t* keys_obj;
            if ((keys_obj = json_object_get(obj, "keys")))
            {
                const char* k;
                json_t* v;
                json_object_foreach(keys_obj, k, v)
                {
                    //Check if key is present in map_server_data
                    GString* gs_key = g_string_new(k);
                    struct AP_GetServerDataRequest* target = g_hash_table_lookup(map_server_data, gs_key);
                    if (target) 
                    {
                        if (json_is_null(v)) { target->value = NULL; target->status = Done; g_hash_table_remove(map_server_data, gs_key); break; }
                        switch (target->type)
                        {
                        case Int:
                            *((uint64_t*)target->value) = json_integer_value(v);
                            break;
                        case Double:
                            *((double*)target->value) = json_real_value(v);
                            break;
                        case Raw:
                            target->value = json_deep_copy(v);
                            break;
                        }
                        target->status = Done;
                        g_hash_table_remove(map_server_data, gs_key);
                    }
                }
            }
        }//end of retrieved
        else if (cmd && !strcmp(cmd, "SetReply"))
        {
            //TODO: Test SetReply
            if (setreplyfunc) 
            {
                uint64_t int_val;
                uint64_t int_orig_val;
                double dbl_val;
                double dbl_orig_val;
                char* raw_val;
                char* raw_orig_val;
                json_t* key_obj;
                json_t* val_obj;
                json_t* orig_val_obj;
                struct AP_SetReply* setreply = AP_SetReply_new(NULL, NULL, NULL);
                if ((key_obj = json_object_get(obj, "key")) && (val_obj = json_object_get(obj, "value")) && (orig_val_obj = json_object_get(obj, "original_value")))
                {
                    GString* gs_key = g_string_new(json_string_value(key_obj));
                    int* type = g_hash_table_lookup(map_serverdata_typemanage, gs_key);
                    switch (*type)
                    {
                        case Int:
                            int_val = json_integer_value(val_obj);
                            int_orig_val = json_integer_value(orig_val_obj);
                            setreply = AP_SetReply_new(gs_key->str, &int_orig_val, &int_val);
                            break;
                        case Double:
                            dbl_val = json_real_value(val_obj);
                            dbl_orig_val = json_real_value(orig_val_obj);
                            setreply = AP_SetReply_new(gs_key->str, &dbl_orig_val, &dbl_val);
                            break;
                        case Raw:
                            raw_val = _strdup(json_string_value(val_obj));
                            raw_orig_val = _strdup(json_string_value(orig_val_obj));
                            setreply = AP_SetReply_new(gs_key->str, &raw_orig_val, &raw_val);
                            break;
                    }
                    (*setreplyfunc)(setreply);
                }
            }
        }//end of setreply
        else if (cmd && !strcmp(cmd, "PrintJSON"))
        {
            json_t* type_obj;
            struct AP_NetworkPlayer* local_player = getPlayer(0, ap_player_id);
            if ((type_obj = json_object_get(obj, "type")))
            {
                char* type = _strdup(json_string_value(type_obj));
                if ((!strcmp(type, "ItemSend")) || (!strcmp(type, "ItemCheat"))) 
                {
                    json_t* item_obj = json_object_get(obj, "item");
                    uint64_t item_id = json_integer_value(json_object_get(item_obj, "item"));
                    json_t* item_recvplayer_obj = json_object_get(item_obj, "player");
                    struct AP_NetworkPlayer* recv_player = getPlayer(0, (int)json_integer_value(json_object_get(obj, "receiving")));
                    struct AP_NetworkPlayer* recv_item_player = getPlayer(0, (int)json_integer_value(item_recvplayer_obj));
                    if (!strcmp(recv_player->alias, local_player->alias) || (strcmp(recv_item_player->alias, local_player->alias))) { continue; }
                    char* item_name = getItemName(recv_player->game, item_id);
                    GArray* messageparts_array = g_array_new(true, true, sizeof(struct AP_MessagePart*));
                    struct AP_MessagePart* msg_p = AP_MessagePart_new(item_name, AP_ItemText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(" was sent to ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(recv_player->alias, AP_PlayerText);
                    g_array_append_val(messageparts_array, msg_p);
                    struct AP_ItemSendMessage* msg = AP_ItemSendMessage_new(item_name, recv_player->alias, ItemSend, messagePartsToPlainText(messageparts_array), messageparts_array);
                    g_queue_push_tail(messageQueue, msg);
                }
                else if ((!strcmp(type, "Hint")))
                {
                    json_t* rec_obj = json_object_get(obj, "receiving");
                    json_t* item_obj = json_object_get(obj, "item");
                    json_t* item_recvplayer_obj = json_object_get(item_obj, "player");
                    uint64_t item_id = json_integer_value(json_object_get(item_obj, "item"));
                    uint64_t loc_id = json_integer_value(json_object_get(item_obj, "location"));
                    json_t* found_obj = json_object_get(obj, "found");
                    struct AP_NetworkPlayer* send_player = getPlayer(0, (int)json_integer_value(item_recvplayer_obj));
                    struct AP_NetworkPlayer* recv_player = getPlayer(0, (int)json_integer_value(rec_obj));
                    char* item_name = getItemName(recv_player->game, item_id);
                    char* loc_name = getLocationName(send_player->game, loc_id);
                    bool checked = json_integer_value(found_obj);
                    GArray* messageparts_array = g_array_new(true, true, sizeof(struct AP_MessagePart*));
                    struct AP_MessagePart* msg_p = AP_MessagePart_new("Item ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(item_name, AP_ItemText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(" from ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(send_player->alias, AP_PlayerText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(" to ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(recv_player->alias, AP_PlayerText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(" at ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(loc_name, AP_LocationText);
                    g_array_append_val(messageparts_array, msg_p);
                    if (checked) { msg_p = AP_MessagePart_new(" (Checked) ", AP_NormalText); }
                    else { msg_p = AP_MessagePart_new(" (Unchecked) ", AP_NormalText); }
                    g_array_append_val(messageparts_array, msg_p);
                    struct AP_HintMessage* msg = AP_HintMessage_new(item_name, send_player->alias, recv_player->alias, loc_name, checked, Hint, messagePartsToPlainText(messageparts_array), messageparts_array);
                    g_queue_push_tail(messageQueue, msg);
                }
                else if ((!strcmp(type, "Countdown")))
                {
                    json_t* cntdwn_obj = json_object_get(obj, "countdown");
                    json_t* data_obj = json_object_get(obj, "data");
                    json_t* txt_obj = json_object_get(data_obj, "text");
                    int timer = (int)json_integer_value(cntdwn_obj);
                    GArray* messageparts_array = g_array_new(true, true, sizeof(struct AP_MessagePart*));
                    struct AP_MessagePart* msg_p = AP_MessagePart_new(json_string_value(txt_obj), AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    struct AP_CountdownMessage* msg = AP_CountdownMessage_new(timer, Countdown, messagePartsToPlainText(messageparts_array), messageparts_array);
                    g_queue_push_tail(messageQueue, msg);
                }
                else 
                {
                    GString* text = g_string_new(NULL);
                    json_t* data_obj = json_object_get(obj, "data");
                    int j;
                    json_t* v;
                    json_array_foreach(data_obj, j, v)
                    { 
                        json_t* text_obj = json_object_get(v, "text");
                        json_t* type2_obj = json_object_get(v, "type");
                        //TODO: Check if alias works correctly
                        if (type2_obj && !strcmp(json_string_value(type2_obj), "player_id")) {
                            g_string_append(text, getPlayer(0, (int)json_integer_value(text_obj))->alias);
                        }
                        else if (strcmp(json_string_value(text_obj),"")) 
                        {
                            g_string_append(text, json_string_value(text_obj));
                        }
                    }
                    struct AP_Message* msg = AP_Message_new(Plaintext, text->str, NULL);
                    g_queue_push_tail(messageQueue, msg);
                }
            }
        }//end of printjson
        else if (cmd && !strcmp(cmd, "LocationInfo"))
        {
            GArray* locations = g_array_new(true, true, sizeof(struct AP_NetworkItem*));
            json_t* locs_obj = json_object_get(obj, "locations");
            int j;
            json_t* v;
            json_array_foreach(locs_obj, j, v)
            {
                json_t* item_obj = json_object_get(v, "item");
                uint64_t item_id = json_integer_value(item_obj);
                json_t* loc_obj = json_object_get(v, "location");
                uint64_t loc_id = json_integer_value(loc_obj);
                json_t* player_obj = json_object_get(v, "player");
                int p_id = (int)json_integer_value(player_obj);
                struct AP_NetworkPlayer* player = getPlayer(0, p_id);
                json_t* flags_obj = json_object_get(v, "flags");
                int flags = (int)json_integer_value(flags_obj);
                struct AP_NetworkItem* item = AP_NetworkItem_new(item_id, loc_id, player->slot, flags, getItemName(ap_game, item_id), getLocationName(ap_game, loc_id), player->alias);
                g_array_append_val(locations, item);
            }
            locinfofunc(locations);
        }//end of locationinfo
        else if (cmd && !strcmp(cmd, "ReceivedItems"))
        {
            json_t* index_obj = json_object_get(obj, "index");
            uint64_t item_idx = json_integer_value(index_obj);
            bool notify = false;
            json_t* items_obj = json_object_get(obj, "items");
            int j;
            json_t* v;
            json_array_foreach(items_obj, j, v)
            {
                json_t* item_obj = json_object_get(v, "item");
                uint64_t item_id = json_integer_value(item_obj);
                notify = (item_idx == 0 && last_item_idx <= j && multiworld) || item_idx != 0;
                json_t* player_obj = json_object_get(v, "player");
                struct AP_NetworkPlayer* sender = getPlayer(0, (int)json_integer_value(player_obj));
                (*getitemfunc)(item_id, sender->slot, notify);
                if (queueitemrecvmsg && notify) {
                    char* item_name = getItemName(ap_game, item_id);
                    GArray* messageparts_array = g_array_new(true, true, sizeof(struct AP_MessagePart*));
                    struct AP_MessagePart* msg_p = AP_MessagePart_new("Received ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(item_name, AP_ItemText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(" from ", AP_NormalText);
                    g_array_append_val(messageparts_array, msg_p);
                    msg_p = AP_MessagePart_new(sender->alias, AP_PlayerText);
                    g_array_append_val(messageparts_array, msg_p);
                    struct AP_ItemRecvMessage* msg = AP_ItemRecvMessage_new(item_name, sender->alias, ItemRecv, messagePartsToPlainText(messageparts_array), messageparts_array);
                    g_queue_push_tail(messageQueue, msg);
                }
            }
            if (item_idx == 0)
            {
                last_item_idx = json_array_size(items_obj);
            }
            else {
                last_item_idx += json_array_size(items_obj);
            }
            GString* resync_key = g_string_new("APCcLastRecv");
            g_string_append(resync_key, ap_player_name);
            g_string_append_printf(resync_key, "%i", ap_player_id);
            struct AP_DataStorageOperation* replace_op = AP_DataStorageOperation_new("replace", &last_item_idx);
            GArray* operations_array = g_array_new(true, true, sizeof(struct AP_DataStorageOperation*));
            g_array_append_val(operations_array, replace_op);
            struct AP_SetServerDataRequest* requ = AP_SetServerDataRequest_new(Pending, resync_key->str, operations_array, 0, Int, false);
            AP_SetServerData(requ);
        }
        else if (cmd && !strcmp(cmd, "RoomUpdate"))
        {
            json_t* checked_locs_obj = json_object_get(obj, "checked_locations");
            size_t j = 0;
            json_t* v = NULL;
            json_array_foreach(checked_locs_obj, j, v)
            {
                uint64_t loc_id = json_integer_value(v);
                (*checklocfunc)(loc_id);
            }
            json_t* players_obj = json_object_get(obj, "players");
            json_array_foreach(players_obj, j, v)
            {
                int slot = 0;
                char* alias = NULL;
                const char* kp;
                json_t* vp;
                json_object_foreach(v, kp, vp)
                {
                    if (!strcmp(kp, "slot")) { slot = (int)json_integer_value(vp); }
                    if (!strcmp(kp, "alias")) { alias = _strdup(json_string_value(vp)); }
                }
                g_array_index(map_players, struct AP_NetworkPlayer*, slot)->alias = alias;
            }
        }//end roomupdate
        else if (cmd && !strcmp(cmd, "ConnectionRefused"))
        {
            auth = false;
            refused = true;
        }//end connectionrefused
        else if (cmd && !strcmp(cmd, "Bounced"))
        {
            if (!enable_deathlink) continue;
            json_t* tags_obj = json_object_get(obj, "tags");
            size_t j = 0;
            json_t* v = NULL;
            json_array_foreach(tags_obj, j, v)
            {
                if (!strcmp(json_string_value(v), "DeathLink"))
                {
                    json_t* data_obj = json_object_get(obj, "data");
                    json_t* source_obj = json_object_get(data_obj, "source");
                    GString* source_name = g_string_new(json_string_value(source_obj));
                    if (!strcmp(source_name->str, ap_player_name)) { break; }
                    deathlinkstat = true;
                    if (recvdeath != NULL) {
                        (*recvdeath)(source_name->str);
                    }
                    break;
                }
            }
        }//end bounced
    }
return false;
}

void AP_Shutdown() {
    lws_context_destroy(context);
    // Reset all states
    init = false;
    auth = false;
    refused = false;
    multiworld = true;
    data_synced = false;
    isSSL = true;
    ssl_success = false;
    ap_player_id = 0;
    ap_player_name="";
    ap_ip="";
    ap_game = "";
    ap_passwd = "";
    ap_uuid = 0;
    g_random_set_seed((guint32)time(NULL));
    AP_NetworkVersion_free(client_version);
    client_version = AP_NetworkVersion_new(0, 2, 6);
    deathlinkstat = false;
    deathlinksupported = false;
    enable_deathlink = false;
    deathlink_amnesty = 0;
    cur_deathlink_amnesty = 0;
    //while (AP_IsMessagePending()) AP_ClearLatestMessage();
    queueitemrecvmsg = true;
    g_array_free(map_players, true);
    //TODO: free the individual structs in the arrays and hash_tables as well
    map_players = g_array_new(true, true, sizeof(struct AP_NetworkPlayer*));
    g_hash_table_destroy(map_game_to_data);
    map_game_to_data = g_hash_table_new(g_string_hash, g_string_equal);
    resetItemValues = NULL;
    getitemfunc = NULL;
    checklocfunc = NULL;
    locinfofunc = NULL;
    recvdeath = NULL;
    setreplyfunc = NULL;
    g_hash_table_destroy(map_serverdata_typemanage);
    last_item_idx = 0;
    sp_save_path="";
    g_hash_table_destroy(map_server_data);
    map_server_data = g_hash_table_new(g_string_hash, g_string_equal);
    g_hash_table_destroy(map_slotdata_callback_int);
    map_slotdata_callback_int = g_hash_table_new(g_string_hash, g_string_equal);
    g_hash_table_destroy(map_slotdata_callback_raw);
    map_slotdata_callback_raw = g_hash_table_new(g_string_hash, g_string_equal);
    g_hash_table_destroy(map_slotdata_callback_intarray);
    map_slotdata_callback_intarray = g_hash_table_new(g_string_hash, g_string_equal);
    g_array_free(slotdata_strings, true);
    slotdata_strings = g_array_new(true, true, sizeof(char*));
}


void AP_EnableQueueItemRecvMsgs(bool b) {
    queueitemrecvmsg = b;
}

void AP_SetItemClearCallback(void (*f_itemclr)()) {
    resetItemValues = f_itemclr;
}

void AP_SetItemRecvCallback(void (*f_itemrecv)(uint64_t, int, bool)) {
    getitemfunc = f_itemrecv;
}

void AP_SetLocationCheckedCallback(void (*f_loccheckrecv)(uint64_t)) {
    checklocfunc = f_loccheckrecv;
}

void AP_SetLocationInfoCallback(void (*f_locinfrecv)(GArray*)) {
    locinfofunc = f_locinfrecv;
}

void AP_SetDeathLinkRecvCallback(void (*f_deathrecv)()) {
    recvdeath = f_deathrecv;
}

void AP_RegisterSlotDataIntCallback(char* key, void (*f_slotdata)(uint64_t)) {
    GString* gs_key = g_string_new(key);
    g_hash_table_insert(map_slotdata_callback_int, gs_key, f_slotdata);
    //g_array_append_val(slotdata_strings, gs_key);
    g_array_append_unique_gstring(slotdata_strings, gs_key);
}

// Returns the slot_data element as a string
void AP_RegisterSlotDataRawCallback(char* key, void (*f_slotdata)(json_t*)) {
    GString* gs_key = g_string_new(key);
    g_hash_table_insert(map_slotdata_callback_raw, gs_key , f_slotdata);
    // g_array_append_val(slotdata_strings, gs_key);
    g_array_append_unique_gstring(slotdata_strings, gs_key);
}

// Returns the slot_data element as a GArray filled with integers
void AP_RegisterSlotDataIntArrayCallback(char* key, void (*f_slotdata)(GArray*)) {
    GString* gs_key = g_string_new(key);
    g_hash_table_insert(map_slotdata_callback_intarray, gs_key, f_slotdata);
    //g_array_append_val(slotdata_strings, gs_key);
    g_array_append_unique_gstring(slotdata_strings, gs_key);
}

void AP_SetDeathLinkSupported(bool supdeathlink) {
    deathlinksupported = supdeathlink;
}

bool AP_DeathLinkPending() {
    return deathlinkstat;
}

void AP_DeathLinkClear() {
    deathlinkstat = false;
}

bool AP_IsMessagePending() {
    if (messageQueue) return messageQueue->length > 0;
    else return false;
}

void* AP_GetLatestMessage() {
    return g_queue_peek_head(messageQueue);
}

void AP_ClearLatestMessage() {
    if (AP_IsMessagePending()) {
        g_queue_pop_head(messageQueue);
    }
}

int AP_GetRoomInfo(struct AP_RoomInfo* client_roominfo) {
    if (!auth) return 0;
    *client_roominfo = lib_room_info;
    return 1;
}

AP_ConnectionStatus AP_GetConnectionStatus() {
    if (refused)
    {
        return ConnectionRefused;
    }
    else if (connected) 
    {
        if (auth) 
        {
            return Authenticated;
        }
        else 
        {
            return Connected;
        }
    }
    else 
    {
        return Disconnected;
    }
}

AP_DataPackageSyncStatus AP_GetDataPackageStatus() {
    if (!auth) {
        return NotChecked;
    }
    if (data_synced) {
        return Synced;
    }
    else {
        return SyncPending;
    }
}

uint64_t AP_GetUUID() {
    return ap_uuid;
}

int AP_GetPlayerID() {
    return ap_player_id;
}

char* AP_GetLocationName(uint64_t id) {
    return getLocationName(ap_game, id);
}

char* AP_GetItemName(uint64_t id) {
    return getItemName(ap_game, id);
}

/*
 * Local addition (not in upstream APCc): reverse lookups from a name to its
 * numeric id for the connected game, by scanning the data-package maps.  Needed
 * so the host game can refer to checks/items by name instead of hardcoding ids.
 * Returns -1 if the data package isn't loaded yet or the name is unknown.
 */
static int64_t AP_GetIdByName_(GHashTable* (*pick)(struct AP_GameData*),
                               const char* name) {
    GString* gs_key;
    struct AP_GameData* gamedata;
    GHashTable* table;
    GHashTableIter iter;
    gpointer k, v;

    if (!map_game_to_data || !name) return -1;
    gs_key = g_string_new(ap_game);
    gamedata = g_hash_table_lookup(map_game_to_data, gs_key);
    g_string_free(gs_key, TRUE);
    if (!gamedata) return -1;

    table = pick(gamedata);
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &k, &v)) {
        if (v && strcmp((const char*)v, name) == 0)
            return *(int64_t*)k;
    }
    return -1;
}

static GHashTable* pick_location_data_(struct AP_GameData* gd) { return gd->location_data; }
static GHashTable* pick_item_data_(struct AP_GameData* gd) { return gd->item_data; }

int64_t AP_GetLocationIdByName(const char* name) {
    return AP_GetIdByName_(pick_location_data_, name);
}

int64_t AP_GetItemIdByName(const char* name) {
    return AP_GetIdByName_(pick_item_data_, name);
}

char* AP_GetLocalHintDataPrefix() {
    char* buffer = (char*)malloc(32);
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return NULL;
    }
    struct AP_NetworkPlayer* player_data = g_array_index(map_players, struct AP_NetworkPlayer*, ap_player_id);
    sprintf(buffer, "_read_hints_%d_%d", player_data->team, player_data->slot);
    return buffer;
}

json_t* AP_GetLocalHints() {
    char* buffer = (char*)malloc(32);
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return NULL;
    }
    struct AP_NetworkPlayer* player_data = g_array_index(map_players, struct AP_NetworkPlayer*, ap_player_id);
    sprintf(buffer, "_read_hints_%d_%d", player_data->team, player_data->slot);

    struct AP_GetServerDataRequest* hint_serverdata_request = AP_GetServerDataRequest_new(Pending, buffer, &last_item_idx, Raw);
    AP_GetServerData(hint_serverdata_request);
    while (hint_serverdata_request->status != Done) {
        //printf("Waiting");
    }
    json_t* hint_obj = hint_serverdata_request->value;

    free(buffer);

    return hint_serverdata_request->value;
}

void AP_SetServerData(struct AP_SetServerDataRequest* sd_request) {
    if (!map_serverdata_typemanage) { map_serverdata_typemanage = g_hash_table_new(g_string_hash, g_string_equal); }
    sd_request->status = Pending;
    json_t* req_t = json_object();
    json_t* req_array = json_array();
    json_t* op_array = json_array();
    json_object_set_new(req_t, "cmd", json_string("Set"));
    json_object_set_new(req_t, "key", json_string(sd_request->key));
    switch (sd_request->type)
    {
    case Int:
        for (guint i = 0; i < sd_request->operations->len; i++)
        {
            json_t* op_obj = json_object();
            struct AP_DataStorageOperation* ap_dso = g_array_index(sd_request->operations, struct AP_DataStorageOperation*, i);
            json_object_set_new(op_obj, "operation", json_string(ap_dso->operation));
            json_object_set_new(op_obj, "value", json_integer(*(int*)ap_dso->value));
            json_array_append_new(op_array, op_obj);
        }
        json_object_set_new(req_t, "operations", op_array);
        break;
    case Double:
        for (guint i = 0; i < sd_request->operations->len; i++)
        {
            json_t* op_obj = json_object();
            struct AP_DataStorageOperation* ap_dso = g_array_index(sd_request->operations, struct AP_DataStorageOperation*, i);
            json_object_set_new(op_obj, "operation", json_string(ap_dso->operation));
            json_object_set_new(op_obj, "value", json_real(*(double*)ap_dso->value));
            json_array_append_new(op_array, op_obj);
        }
        json_object_set_new(req_t, "operations", op_array);
        break;
    default:
        for (guint i = 0; i < sd_request->operations->len; i++)
        {
            json_t* op_obj = json_object();
            struct AP_DataStorageOperation* ap_dso = g_array_index(sd_request->operations, struct AP_DataStorageOperation*, i);
            json_object_set_new(op_obj, "operation", json_string(ap_dso->operation));
            json_object_set_new(op_obj, "value", json_string((char*)ap_dso->value));
            json_array_append_new(op_array, op_obj);
        }
        json_object_set_new(req_t, "operations", op_array);
        json_object_set_new(req_t, "default", json_string((char*)sd_request->default_value));
        break;
    }
    json_object_set_new(req_t, "want_reply", json_integer(sd_request->want_reply));
    GString* gs_key = g_string_new(sd_request->key);
    g_hash_table_insert(map_serverdata_typemanage, gs_key, &sd_request->type);
    if (multiworld)
    {
        json_array_append(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else if (sp_testing)
    {
        //TODO: SP test
        localSetServerData(req_t);
    }
    sd_request->status = Done;
}

void AP_RegisterSetReplyCallback(void (*f_setreply)(struct AP_SetReply* reply)) {
    setreplyfunc = f_setreply;
}

//TODO: Test SetNotify
void AP_SetNotify_Keylist(GHashTable* keylist) {
    json_t* req_t = json_object();
    json_t* req_array = json_array();
    json_t* req_key_array = json_array();
    json_object_set_new(req_t, "cmd", json_string("SetNotify"));
    GString* key;
    char* val;
    g_hash_table_iter_init(&it, keylist);

    while (g_hash_table_iter_next(&it, &key, &val))
    {
        json_array_append_new(req_key_array, json_string(key->str));
        json_object_set_new(req_t, "keys", req_key_array);
        GString* gs_key = g_string_new(key->str);
        g_hash_table_insert(map_serverdata_typemanage, gs_key, val);
    }
    json_array_append_new(req_array, req_t);
    g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
    AP_SendWeb();
}

//TODO: Test SetNotify
void AP_SetNotify_Type(char* key, AP_DataType type) {
    GHashTable* keylist = g_hash_table_new(g_string_hash, g_string_equal);
    GString* gs_key = g_string_new(key);
    g_hash_table_insert(keylist, gs_key, &type);
    AP_SetNotify_Keylist(keylist);
}

void AP_GetServerData(struct AP_GetServerDataRequest* sd_request) {
    if (!map_server_data) { map_server_data = g_hash_table_new(g_string_hash, g_string_equal); }
    sd_request->status = Pending;
    GString* gs_key = g_string_new(sd_request->key);
    if (g_hash_table_lookup(map_server_data, gs_key) != NULL) { printf("Returning without action.\n"); return; }
    g_hash_table_insert(map_server_data, gs_key, sd_request);

    if (multiworld)
    {
        json_t* req_t = json_object();
        json_t* req_array = json_array();
        json_t* req_key_array = json_array();
        json_object_set_new(req_t, "cmd", json_string("Get"));
        json_array_append_new(req_key_array, json_string(sd_request->key));
        json_object_set_new(req_t, "keys", req_key_array);
        json_array_append_new(req_array, req_t);
        g_queue_push_tail(outgoing_queue, json_deep_copy(req_array));
        AP_SendWeb();
    }
    else if (sp_testing)
    {
        //TODO: Test fake messages for sp
        json_t* fake_req_array = json_array();
        json_t* fake_req_t = json_object();
        json_t* fake_req_key_array = json_array();
        json_object_set_new(fake_req_t, "cmd", json_string("Retrieved"));
        const char* k_s;
        json_t* v_s;
        json_object_foreach(sp_save_root, k_s, v_s)
        {
            if (!strcmp(k_s, "store"))
            {
                json_array_append(fake_req_key_array, v_s);
                break;
            }
        }
        json_object_set_new(fake_req_t, "keys", fake_req_key_array);
        json_array_append_new(fake_req_array, fake_req_t);
        parse_response(fake_req_array);
    }
}

GString* AP_GetPrivateServerDataPrefix() {
    GString* return_string = g_string_new("APCc");
    g_string_append(return_string, lib_room_info.seed_name);
    g_string_append(return_string, "APCc");
    g_string_append_printf(return_string, "%i", ap_player_id);
    g_string_append(return_string, "APCc");
    return return_string;
}

struct AP_NetworkPlayer* getPlayer(int team, int slot) {
    return g_array_index(map_players, struct AP_NetworkPlayer*, slot);
}

char* getItemName(const char* gamename, uint64_t id) {
    if (!map_game_to_data) {
        printf("Game Lookup Table does not exist yet.\n");
        return "Unknown Item";
    }
    
    GString* gs_key = g_string_new(gamename);
    struct AP_GameData* gamedata = g_hash_table_lookup(map_game_to_data, gs_key);
    if (gamedata) {        
        if (g_hash_table_lookup(gamedata->item_data, &id)) {
            char* item_name = g_hash_table_lookup(gamedata->item_data, &id);
            return item_name ? item_name : "Unknown Item";
        }
    }
    else {
        size_t buffer_size = 256;  // Adjust size as needed
        char* message = malloc(buffer_size);
        snprintf(message, buffer_size, "Gamename %s not found", gamename);
        return message;
    }
    return "Unknown Item";
}

char* getLocationName(const char* gamename, uint64_t id) {
    if (!map_game_to_data) {
        printf("Game Lookup Table does not exist yet.\n");
        return "Unknown Item";
    }
    
    GString* gs_key = g_string_new(gamename);
    struct AP_GameData* gamedata = g_hash_table_lookup(map_game_to_data, gs_key);
    if (gamedata) {
        if (g_hash_table_lookup(gamedata->location_data, &id))        {
            char* location_name = g_hash_table_lookup(gamedata->location_data, &id);
            return location_name ? location_name : "Unknown Location";
        }
    }
    else {
        size_t buffer_size = 256;  // Adjust size as needed
        char* message = malloc(buffer_size);
        snprintf(message, buffer_size, "Gamename %s not found", gamename);
        return message;
    }
    return "Unknown Item";
}

void parseDataPkgCache() {
    if (!datapkg_cache)
    {
        printf("parseDataPkgCache called without a valid datapkg_cache.\n");
        return;
    }
    else
    {
        json_t* cache_games_obj = json_object_get(datapkg_cache, "games");
        json_t* items_obj;
        json_t* locs_obj;
        const char* k;
        json_t* v;
        const char* itemk;
        json_t* itemv;
        const char* lock;
        json_t* locv;
        map_game_to_data = g_hash_table_new(g_string_hash, g_string_equal);
        json_object_foreach(cache_games_obj, k, v)
        {
            GHashTable* map_item_id_name = g_hash_table_new(g_int64_hash, g_int64_equal);
            GHashTable* map_location_id_name = g_hash_table_new(g_int64_hash, g_int64_equal);
            struct AP_GameData* game_data = AP_GameData_new(map_item_id_name, map_location_id_name);
            items_obj = json_object_get(v, "item_name_to_id");
            locs_obj = json_object_get(v, "location_name_to_id");
            json_object_foreach(items_obj, itemk, itemv) 
            {
                uint64_t* key = malloc(sizeof(uint64_t));
                if (key != NULL) *key = json_integer_value(itemv);
                g_hash_table_insert(map_item_id_name, key, _strdup(itemk));
            }
            json_object_foreach(locs_obj, lock, locv)
            {
                uint64_t* key = malloc(sizeof(uint64_t));
                if(key!=NULL) *key = json_integer_value(locv);
                g_hash_table_insert(map_location_id_name, key, _strdup(lock));
            }
            GString* gs_key = g_string_new(k);
            g_hash_table_insert(map_game_to_data, gs_key, game_data);
        }
    }
}

void parseDataPkg(json_t* new_datapkg) {
    if (!new_datapkg)
    {
        printf("parseDataPkg received null ptr.\n");
    }
    else if (!datapkg_cache)
    {
        json_t* data_obj = json_object_get(new_datapkg, "data");
        //No cache to compare to, that means this is our full new datapackage
        json_dump_file(data_obj, datapkg_cache_path, 0);
        datapkg_cache = json_load_file(datapkg_cache_path, 0, &jerror);
    }
    else
    {
        //Need to replace specific game data in the old datapackage
        json_t* data_obj;
        
        if ((data_obj = json_object_get(new_datapkg, "data")))
        { 
            json_t* new_games_obj = json_object_get(data_obj, "games");
            json_t* cache_games_obj = json_object_get(datapkg_cache, "games");
            
            json_t* v;
            const char* k;
            json_object_foreach(new_games_obj, k, v)
            {
                json_object_set(cache_games_obj, k, v);
            }
        }
        else 
        {
            printf("parseDataPkg received invalid datapackage json.\n");
        }
        json_dump_file(datapkg_cache, datapkg_cache_path, 0);
        datapkg_cache = json_load_file(datapkg_cache_path, 0, &jerror);
    }
    parseDataPkgCache();
}

char* messagePartsToPlainText(GArray* messagePartsIn) {
    GString* out = g_string_new(NULL);
    for (guint i = 0; i < messagePartsIn->len; i++)
    {
        g_string_append(out , g_array_index(messagePartsIn, struct AP_MessagePart*, i)->text);
    }
    return out->str;
}

void localSetServerData(json_t* req)
{
    json_t* def_obj = json_object_get(req, "default");
    json_t* key_obj = json_object_get(req, "key");
    json_t* store_obj = json_object_get(sp_save_root, "store");
    const char* key = json_string_value(key_obj);
    json_t* tmp_val = json_object_get(store_obj, key);
    json_t* old = json_object_get(store_obj, key);

    if (!tmp_val) 
    {
        tmp_val = def_obj;
    }
    size_t i;
    json_t* v;
    json_t* oper_obj = json_object_get(req, "operations");
    //TODO: SP Testing
    json_array_foreach(oper_obj, i, v) 
    {
        json_t* op_obj = json_object_get(v, "operation");
        const char* op = json_string_value(op_obj);
        json_t* val_obj = json_object_get(v, "value");
        if (!strcmp(op, "replace")) 
        { 
            json_object_set(tmp_val, key, val_obj);
        }
    }
    json_object_set(store_obj, key, tmp_val);
    json_object_set(sp_save_root, "store", store_obj);
    json_dump_file(sp_save_root, sp_save_path, 0);

    json_t* fake_req_array = json_array();
    json_t* fake_req_t = json_object();
    json_object_set_new(fake_req_t, "cmd", json_string("SetReply"));
    json_object_set_new(fake_req_t, "key", json_string(key));
    json_object_set_new(fake_req_t, "value", tmp_val);
    json_object_set_new(fake_req_t, "original_value", old);
    json_array_append_new(fake_req_array, fake_req_t);
    parse_response(fake_req_array);
}