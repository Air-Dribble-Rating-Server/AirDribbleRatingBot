#include <dpp/dpp.h>
#include <nlohmann/json.hpp>
#include <print>
#include <fstream>
#include <string>
#include <format>
#include <iostream>

#include "handlers/event_handler.h"
#include "handlers/command_handler.h"

using json = nlohmann::json;

int main()
{
    std::ifstream config_file("config.json");
    if (!config_file.is_open())
    {
        std::println(stderr, "[ERROR] Failed to find config.json");
        std::println(stderr, "  Create a config.json with: {{ \"token\": \"token_here\" }}");
        return 1;
    }

    json config;
    try 
    {
        config = json::parse(config_file);
    }
    catch (const json::parse_error& e)
    {
        std::println(stderr, "[ERROR] config.json has an invalid format: {}", e.what());
        return 1;
    }

    if (!config.contains("token") || !config["token"].is_string())
    {
        std::println(stderr, "[ERROR] config.json needs to have the key \"token\" with the token of the bot");
        return 1;
    }

    const std::string token = config["token"].get<std::string>();

    dpp::cluster bot{token, dpp::i_default_intents | dpp::i_message_content};

    EventHandler event_handler(bot);
    CommandHandler command_handler(bot);
    
    event_handler.register_all();
    command_handler.register_all();

    bot.start(dpp::st_wait);

    return 0;
}
