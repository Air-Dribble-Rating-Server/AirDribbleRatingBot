#pragma once
#include <dpp/dpp.h>

class EventHandler
{
public:
    EventHandler(dpp::cluster& bot_ref);

    void register_all();
private:
    dpp::cluster& bot;
    
    void on_ready(const dpp::ready_t&);
};