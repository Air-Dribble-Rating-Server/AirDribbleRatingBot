#pragma once
#include <format>

#include "../handlers/command_handler.h"

class PingCommand : public ICommand
{
public:
    std::string get_name() const override { return "ping"; }

    dpp::slashcommand definition(dpp::snowflake bot_id) const override
    {
        return dpp::slashcommand("ping", "¡Pong!", bot_id);
    }

    std::expected<dpp::message, std::string> execute(const dpp::slashcommand_t& event) const override
    {
        dpp::message msg;
        msg.set_content("¡Pong! 🏓 Latency: " + std::format("{:.2f}", event.from()->creator->rest_ping * 1000) + "ms");
        return msg;
    }
};