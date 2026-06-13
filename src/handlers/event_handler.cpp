#include <print>

#include "event_handler.h"

EventHandler::EventHandler(dpp::cluster& bot_ref) : bot(bot_ref) {}

void EventHandler::register_all()
{
    bot.on_ready([this](const dpp::ready_t& event)
    {
        on_ready(event);
    });

    bot.on_log([](const dpp::log_t& event)
    {
        if (event.severity >= dpp::ll_warning)
        {
            std::println("[{}] {}", dpp::utility::loglevel(event.severity), event.message);
        }
    });
}

void EventHandler::on_ready(const dpp::ready_t&)
{
    bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "Coding in C++23"));
    std::println("[BOT] Connected as {} ({})", bot.me.username, std::to_string(bot.me.id));
}