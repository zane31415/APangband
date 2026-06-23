#define MAX_PAYLOAD_SIZE 32768
#define WRITE_BUFFER_SIZE 32768

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <jansson.h>
#include <libwebsockets.h>
#include <glib.h>

void AP_Init(const char* ip, int port, const char* game, const char* player_name, const char* passwd);
bool AP_IsInit();
void AP_WebService();

void AP_Start();
void AP_Shutdown();

// Struct potentially required for Data Maps using ("Gamename", int64_t) tuples
// Not used for now because it seems unnecessary
struct pair_char_uint64_t {
    char* name;
    uint64_t id;
};

struct AP_NetworkVersion {
    int major;
    int minor;
    int build;
};

struct AP_NetworkItem {
    uint64_t item;
    uint64_t location;
    int player;
    int flags;
    char* itemName;
    char* locationName;
    char* playerName;
};

struct AP_NetworkPlayer {
    int team;
    int slot;
    char* name;
    char* alias;
    char* game;
};

struct AP_GameData {
    GHashTable* item_data;
    GHashTable* location_data;
};

typedef enum {
    Pending, Done, Error
} AP_RequestStatus;

typedef enum {
    Raw, Int, Double
} AP_DataType;

// Struct funcs

struct AP_NetworkVersion* AP_NetworkVersion_new(int major, int minor, int build);
void AP_NetworkVersion_free(struct AP_NetworkVersion* version);

struct AP_NetworkItem* AP_NetworkItem_new(uint64_t item, uint64_t location, int player, int flags, const char* itemName, const char* locationName, const char* playerName);
void AP_NetworkItem_free(struct AP_NetworkItem* item);

struct AP_NetworkPlayer* AP_NetworkPlayer_new(int team, int slot, const char* name, const char* alias, const char* game);
void AP_NetworkPlayer_free(struct AP_NetworkPlayer* player);

struct AP_SetServerDataRequest* AP_SetServerDataRequest_new(AP_RequestStatus status, const char* key, GArray* operations, void* default_value, AP_DataType type, bool want_reply);
void AP_SetServerDataRequest_free(struct AP_SetServerDataRequest* request);

struct AP_GetServerDataRequest* AP_GetServerDataRequest_new(AP_RequestStatus status, const char* key, void* value, AP_DataType type);
void AP_GetServerDataRequest_free(struct AP_GetServerDataRequest* request);

struct AP_SetReply* AP_SetReply_new(const char* key, void* original_value, void* value);
void AP_SetReply_free(struct AP_SetReply* reply);

struct AP_DataStorageOperation* AP_DataStorageOperation_new(const char* operation, void* value);
void AP_DataStorageOperation_free(struct AP_DataStorageOperation* operation_struct);

struct AP_RoomInfo* AP_RoomInfo_new(struct AP_NetworkVersion version, GArray* tags, bool password_required, GHashTable* permissions, int hint_cost, int location_check_points, GArray* games ,GHashTable* datapackage_checksums, const char* seed_name, double time);
void AP_RoomInfo_free(struct AP_RoomInfo* room_info);

struct AP_GameData* AP_GameData_new(GHashTable* item_data, GHashTable* location_data);
void AP_GameData_free(struct AP_GameData* game_data);

// Set current client version
void AP_SetClientVersion(struct AP_NetworkVersion* version);

/* Configuration Functions */

void AP_EnableQueueItemRecvMsgs(bool);

void AP_SetDeathLinkSupported(bool);

/* Name Lookup Functions */
char* AP_GetLocationName(uint64_t id);
char* AP_GetItemName(uint64_t id);
/* Local additions (not upstream): reverse name -> id lookups; -1 if unknown. */
int64_t AP_GetLocationIdByName(const char* name);
int64_t AP_GetItemIdByName(const char* name);
json_t* AP_GetLocalHints();
char* AP_GetLocalHintDataPrefix();

/* Required Callback Functions */

//Parameter Function must reset local state
void AP_SetItemClearCallback(void (*f_itemclr)());
//Parameter Function must collect item id given with parameter. Second parameter is the player id that sent that item. Third parameter indicates whether or not to notify player
void AP_SetItemRecvCallback(void (*f_itemrecv)(uint64_t, int, bool));
//Parameter Function must mark given location id as checked
void AP_SetLocationCheckedCallback(void (*f_locrecv)(uint64_t));

/* Optional Callback Functions */

//Parameter Function will be called when Death Link is received. Alternative to Pending/Clear usage
void AP_SetDeathLinkRecvCallback(void (*f_deathrecv)());

// Parameter Function receives Slotdata of respective type
void AP_RegisterSlotDataIntCallback(char*, void (*f_slotdata)(uint64_t));
void AP_RegisterSlotDataIntArrayCallback(char*, void (*f_slotdata)(GArray*));
void AP_RegisterSlotDataRawCallback(char*, void (*f_slotdata)(json_t*));

// Send LocationScouts packet
//void AP_SendLocationScouts(std::vector<int64_t> const& locations, int create_as_hint);
void AP_SendLocationScouts(GArray* locations, int create_as_hint);
void AP_SendLocationScout(uint64_t location, int create_as_hint);
// Receive Function for LocationInfo
//void AP_SetLocationInfoCallback(void (*f_locrecv)(std::vector<AP_NetworkItem>));
void AP_SetLocationInfoCallback(void (*f_locrecv)(GArray*));

// Sends LocationCheck for given index
void AP_SendItem(uint64_t);

// Send a chat message to the server
void AP_SendMsg(char*);

// Called when Story completed, sends StatusUpdate
void AP_StoryComplete();

/* Deathlink Functions */

bool AP_DeathLinkPending();
void AP_DeathLinkClear();
void AP_DeathLinkSend();

/* Message Management Types */

typedef enum {
    Plaintext, ItemSend, ItemRecv, Hint, Countdown
} AP_MessageType;

typedef enum {
    AP_NormalText, AP_LocationText, AP_ItemText, AP_PlayerText
} AP_MessagePartType;

struct AP_MessagePart {
    char* text;
    AP_MessagePartType type;
};

struct AP_Message {
    AP_MessageType type;
    char* text;
    GArray* messageParts;
};

struct AP_ItemSendMessage {
    struct AP_Message base;
    char* item;
    char* recvPlayer;
};

struct AP_ItemRecvMessage {
    struct AP_Message base;
    char* item;
    char* sendPlayer;
};

struct AP_HintMessage {
    struct AP_Message base;
    char* item;
    char* sendPlayer;
    char* recvPlayer;
    char* location;
    int checked;
};

struct AP_CountdownMessage {
    struct AP_Message base;
    int timer;
};

/* Message Management Functions */

bool AP_IsMessagePending();
void AP_ClearLatestMessage();
void* AP_GetLatestMessage();

/* Connection Information Types */

typedef enum {
    Disconnected, Connected, Authenticated, ConnectionRefused
} AP_ConnectionStatus;

typedef enum {
    NotChecked, SyncPending, Synced
} AP_DataPackageSyncStatus;

#define AP_PERMISSION_DISABLED 0b000
#define AP_PERMISSION_ENABLED 0b001
#define AP_PERMISSION_GOAL 0b010
#define AP_PERMISSION_AUTO 0b110

struct AP_RoomInfo {
    struct AP_NetworkVersion version;
    GArray* tags;
    bool password_required;
    GHashTable* permissions;
    int hint_cost;
    int location_check_points;
    GArray* games;
    GHashTable* datapackage_checksums;
    char* seed_name;
    double time;
};

/* Connection Information Functions */

int AP_GetRoomInfo(struct AP_RoomInfo*);
AP_ConnectionStatus AP_GetConnectionStatus();
AP_DataPackageSyncStatus AP_GetDataPackageStatus();
uint64_t AP_GetUUID();
int AP_GetPlayerID();

/* Serverside Data Types */



struct AP_GetServerDataRequest {
    AP_RequestStatus status;
    char* key;
    void* value;
    AP_DataType type;
};

struct AP_DataStorageOperation {
    char* operation;
    void* value;
};

struct AP_SetServerDataRequest {
    AP_RequestStatus status;
    char* key;
    //std::vector<AP_DataStorageOperation> operations;
    GArray* operations;
    void* default_value;
    AP_DataType type;
    bool want_reply;
};

struct AP_SetReply {
    char* key;
    void* original_value;
    void* value;
};

/* Serverside Data Functions */

// Set and Receive Data
void AP_SetServerData(struct AP_SetServerDataRequest* request);
void AP_GetServerData(struct AP_GetServerDataRequest* request);

// This returns a string prefix, consistent across game connections and unique to the player slot.
// Intended to be used for getting / setting private server data
// No guarantees are made regarding the content of the prefix!
GString* AP_GetPrivateServerDataPrefix();

// Parameter Function receives all SetReply's
// ! Pointers in AP_SetReply struct only valid within function !
// If values are required beyond that a copy is needed
void AP_RegisterSetReplyCallback(void (*f_setreply)(struct AP_SetReply* reply));

// Receive all SetReplys with Keys in parameter list
void AP_SetNotify_Keylist(GHashTable* keylist);
// Single Key version of above for convenience
void AP_SetNotify_Type(char* key, AP_DataType type);

int AP_WebsocketSulInit(uint32_t timeout_ms);
void service_loop();
/* Local addition: wake a blocked AP_WebService()/lws_service() cross-thread. */
void AP_WakeService();