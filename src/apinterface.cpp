#include <fstream>
#include <map>

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_THREAD_

#include "apclientpp/apclient.hpp"
#include "uuid.h"

extern "C" {
  #include "player.h"
  #include "apinterface.h"
  #include "message.h"
}

#define HIDWORD(n) (int32_t)((n >> 8) & 0xffffffff)
#define LODWORD(n) (int32_t)(n & 0xffffffff)

#define GAME_NAME "Angband"
#define DATAPACKAGE_CACHE "datapackage.json"

using nlohmann::json;

APClient *ap = NULL;
bool ap_sync_queued = false;
bool ap_connect_sent = false;
double deathtime = -1;

int nextCheckToGet = 0;

std::string server;
std::string slotname;
std::string password;

int goal = 0;
bool deathlink = false;

char SetAPSettings(struct player *p) {
  server = p->server;
  slotname = p->slotname;
  message_add("AP settings intialized", MSG_GENERIC);
  /*std::ifstream i("angband-ap.json");
  if (i.is_open()) {
    std::ostringstream fullserver;
    json settings;
    i >> settings;
    i.close();

    bool apEnabled = settings["ap-enable"].get<bool>();
    if (!apEnabled) return 0;

    fullserver
      << (settings["server"].is_string() ? settings["server"].get<std::string>() : (std::string)"localhost")
      << ":"
      << (settings["port"].is_number() ? settings["port"].get<int>() : 38281);
    server = fullserver.str();

    password = settings["password"].is_string() ? settings["password"].get<std::string>() : (std::string)"";
    slotname = settings["slotname"].get<std::string>();

    return 1;
  } else return 0;*/
}

void ConnectAP()
{
  std::string uri = server;
  // read or generate uuid, required by AP
  std::string uuid = get_uuid();

  deathlink = false;

  if (ap) delete ap;
  ap = nullptr;
  if (!uri.empty() && uri.find("ws://") != 0 && uri.find("wss://") != 0) uri = "ws://"+uri;

  if (uri.empty()) ap = new APClient(uuid, GAME_NAME);
  else ap = new APClient(uuid, GAME_NAME, uri);
  message_add("Connecting to AP Client", MSG_GENERIC);

  // load DataPackage cache
  FILE* f = fopen(DATAPACKAGE_CACHE, "rb");
  if (f) {
    char* buf = nullptr;
    size_t len = (size_t)0;
    if ((0 == fseek(f, 0, SEEK_END)) &&
        ((len = ftell(f)) > 0) &&
        ((buf = (char*)malloc(len+1))) &&
        (0 == fseek(f, 0, SEEK_SET)) &&
        (len == fread(buf, 1, len, f)))
    {
      buf[len] = 0;
      /// NOTE: deprecated
      // try {
      //   ap->set_data_package(json::parse(buf));
      // } catch (std::exception&) { /* ignore */ }
    }
    free(buf);
    fclose(f);
  }

  // set state and callbacks
  ap_sync_queued = false;
  ap->set_socket_connected_handler([](){
    // TODO: what to do when we've connected to the server
    message_add("Authenticating AP Client", MSG_GENERIC);
  });
  ap->set_socket_disconnected_handler([](){
    // TODO: what to do when we've disconnected from the server
    message_add("Disconnected from AP Client", MSG_GENERIC);
  });
  ap->set_room_info_handler([](){
    printf("Room info received\n");
    std::list<std::string> tags;
    ap->ConnectSlot(slotname, password, 0b111, {"AP"}, {0,4,6});
    ap_connect_sent = true; // TODO: move to APClient::State ?
  });
  ap->set_slot_connected_handler([](const json& data){
    if (data.find("death_link") != data.end() && data["death_link"].is_boolean())
      deathlink = data["death_link"].get<bool>();
    else deathlink = false;

    if (data.find("goal") != data.end() && data["goal"].is_number_integer())
      goal = data["goal"].get<int>();
    else goal = 0;

/*
    if (data.find("cost_scale") != data.end() && data["cost_scale"].is_array())
      apCostScale = data["cost_scale"].get<std::vector<int>>();
    else apCostScale = std::vector<int>({80,5,4}); 
*/
    if (deathlink) ap->ConnectUpdate(false, 0b111, true, {"AP", "DeathLink"});
    ap->StatusUpdate(APClient::ClientStatus::PLAYING);
    message_add("Connected to AP Client", MSG_GENERIC);
  });
  ap->set_slot_disconnected_handler([](){
    message_add("Disconnected from AP Client", MSG_GENERIC);
    ap_connect_sent = false;
  });
  ap->set_slot_refused_handler([](const std::list<std::string>& errors){
    ap_connect_sent = false;
    if (std::find(errors.begin(), errors.end(), "InvalidSlot") != errors.end()) {
      //bad_slot(game?game->get_slot():"");
    } else {
      msg(errors.front().c_str());
    }
  });
  ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
    auto me = ap->get_player_number();
    if (!ap->is_data_package_valid()) {
      // NOTE: this should not happen since we ask for data package before connecting
      if (!ap_sync_queued) ap->Sync();
      ap_sync_queued = true;
      return;
    }
    for (const auto& item: items) {
      auto itemname = ap->get_item_name(item.item, ap->get_player_game(item.player));
      auto sender = item.player ? (ap->get_player_alias(item.player) + "'s world") : "out of nowhere";
      auto location = ap->get_location_name(item.location);
      message_add("Got a thing!", MSG_GENERIC);
/*      printf("  #%d: %s (%" PRId64 ") from %s - %s\n",
              item.index, itemname.c_str(), item.item,
              sender.c_str(), location.c_str());*/

      if (item.index < nextCheckToGet) {
        message_add("Ignoring remaining items.", MSG_GENERIC);
        continue;
      }
      nextCheckToGet = item.index + 1;

      // Note: !GetItem has item.player == me but has location == -1 which crashes due to negative array indexing
      if (item.player == me && item.location != -1) {
        auto localLocation = item.location;
  /*      if (!(*apStores[localLocation / 24])[localLocation % 24]) {
          printf("Not yet purchased on this save; storing\n");
          apStores[localLocation / 24]->StoreItem(localLocation % 24, item.item);
          continue;
        }*/
        //location = IDToLocation(item.location);
      }
      
      /*ReceiveItem((t_itemTypes)(item.item),
        (item.player == 0 && item.location == -2)
        ? NULL
        : (item.player == ap->get_player_number()
          ? location.c_str()
          : sender.c_str()));*/
    }
    fflush(stdout);
  });
  ap->set_location_info_handler([](const std::list<APClient::NetworkItem> &items) {
    auto me = ap->get_player_number();
    for (auto item: items) {
      if (item.player != me) {
        auto itemname = ap->get_item_name(item.item, ap->get_player_game(item.player));
        auto recipient = ap->get_player_alias(item.player);

        /*ReportSentItem(IDToLocation(item.location), recipient.c_str(), itemname.c_str());*/
      }
    }
  });
  ap->set_data_package_changed_handler([](const json& data) {
    // NOTE: what to do when the data package changes (probably doesn't need to change this code)
    FILE* f = fopen(DATAPACKAGE_CACHE, "wb");
    if (f) {
      std::string s = data.dump();
      fwrite(s.c_str(), 1, s.length(), f);
      fclose(f);
    }
  });
  ap->set_print_handler([](const std::string& msg) {
    printf("%s\n", msg.c_str());
  });
  ap->set_print_json_handler([](const std::list<APClient::TextNode>& msg) {
    printf("%s\n", ap->render_json(msg, APClient::RenderFormat::ANSI).c_str());
  });
  ap->set_bounced_handler([](const json& cmd) {
    /*
    if (deathlink) {
      auto tagsIt = cmd.find("tags");
      auto dataIt = cmd.find("data");
      if (tagsIt != cmd.end() && tagsIt->is_array()
          && std::find(tagsIt->begin(), tagsIt->end(), "DeathLink") != tagsIt->end())
      {
        if (dataIt != cmd.end() && dataIt->is_object()) {
          json data = *dataIt;
          if (data["time"].is_number() && isEqual(data["time"].get<double>(), deathtime)) {
            deathtime = -1;
          } else {
            KillPlayer(data["source"].is_string() ? data["source"].get<std::string>().c_str() : "???");
          }
        } else {
          printf("Bad deathlink packet!\n");
        }
      }
    }*/
  });
}

void DisconnectAP()
{
  if (ap) {
    delete ap;
    ap = NULL;
  }
}

void WriteAPState() {}
void ReadAPState() {}

void PollServer()
{
  if (ap) ap->poll();
}

char isDeathLink() {}
char SendDeathLink() {}

char AnnounceAPVictory()
{
  if (!ap || ap->get_state() != APClient::State::SLOT_CONNECTED) return 0;
  ap->StatusUpdate(APClient::ClientStatus::GOAL);
}

