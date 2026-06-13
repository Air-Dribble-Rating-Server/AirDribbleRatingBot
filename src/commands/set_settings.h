#pragma once
#include "../handlers/command_handler.h"
#include <dpp/dpp.h>
#include <expected>
#include <string>
#include <fstream>
#include <print>
#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

class SetSettingsCommand : public ICommand
{
public:
    std::string get_name() const override { return "set-settings"; }

    dpp::slashcommand definition(dpp::snowflake bot_id) const override
    {
        dpp::slashcommand cmd("set-settings", "Set the needed settings for the bot to work", bot_id);
        cmd.set_default_permissions(dpp::p_administrator); // Only admins

        cmd.add_option(dpp::command_option(dpp::co_channel, "channel", "Set the moderator channel to receive players' submits", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "grounddweller", "Set the Ground Dweller role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "beginner", "Set the Beginner role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "apprentice", "Set the Apprentice role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "intermediate", "Set the Intermediate role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "advanced", "Set the Advanced role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "expert", "Set the Expert role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "master", "Set the Master role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "legend", "Set the Legend role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "mythic", "Set the Mythic role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "demigod", "Set the Demigod role to use", true));
        cmd.add_option(dpp::command_option(dpp::co_role, "airdribblegod", "Set the Air Dribble God role to use", true));

        return cmd;
    }

    std::expected<dpp::message, std::string> execute(const dpp::slashcommand_t& event) const override
    {
        // Get parameters
        dpp::snowflake channel_id = std::get<dpp::snowflake>(event.get_parameter("channel"));
        dpp::snowflake grounddweller = std::get<dpp::snowflake>(event.get_parameter("grounddweller"));
        dpp::snowflake beginner = std::get<dpp::snowflake>(event.get_parameter("beginner"));
        dpp::snowflake apprentice = std::get<dpp::snowflake>(event.get_parameter("apprentice"));
        dpp::snowflake intermediate = std::get<dpp::snowflake>(event.get_parameter("intermediate"));
        dpp::snowflake advanced = std::get<dpp::snowflake>(event.get_parameter("advanced"));
        dpp::snowflake expert = std::get<dpp::snowflake>(event.get_parameter("expert"));
        dpp::snowflake master = std::get<dpp::snowflake>(event.get_parameter("master"));
        dpp::snowflake legend = std::get<dpp::snowflake>(event.get_parameter("legend"));
        dpp::snowflake mythic = std::get<dpp::snowflake>(event.get_parameter("mythic"));
        dpp::snowflake demigod = std::get<dpp::snowflake>(event.get_parameter("demigod"));
        dpp::snowflake airdribblegod = std::get<dpp::snowflake>(event.get_parameter("airdribblegod"));

        // Check if the channel is a text channel
        dpp::channel* ch = dpp::find_channel(channel_id);
        if (!ch || !ch->is_text_channel())
            return std::unexpected("❌ The selected channel is not a text channel");

        // Read the ranks.json file and parse it
        json ranks;
        std::ifstream ranks_file("data/ranks.json");
        if (ranks_file.is_open())
        {
            try { ranks = json::parse(ranks_file); }
            catch (const json::parse_error& e)
            {
                return std::unexpected("❌ Error parsing ranks.json: " + std::string(e.what()));
            }
        }
        else
        {
            // Create an empty object if the file doesnt exist
            ranks = json::object();
        }

        ranks_file.close();

        // Update roleID in each rank
        ranks["grounddweller"]["roleID"] = std::to_string(grounddweller);
        ranks["beginner"]["roleID"] = std::to_string(beginner);
        ranks["apprentice"]["roleID"] = std::to_string(apprentice);
        ranks["intermediate"]["roleID"] = std::to_string(intermediate);
        ranks["advanced"]["roleID"] = std::to_string(advanced);
        ranks["expert"]["roleID"] = std::to_string(expert);
        ranks["master"]["roleID"] = std::to_string(master);
        ranks["legend"]["roleID"] = std::to_string(legend);
        ranks["mythic"]["roleID"] = std::to_string(mythic);
        ranks["demigod"]["roleID"] = std::to_string(demigod);
        ranks["airdribblegod"]["roleID"] = std::to_string(airdribblegod);

        // Ensure that the basic fields exist if they were not already present
        for (auto& [key, val] : ranks.items())
        {
            if (!val.contains("name")) val["name"] = key;
            if (!val.contains("ratingNeeded")) val["ratingNeeded"] = 0;
            if (key == "airdribblegod")
            {
                if (!val.contains("player")) val["player"] = nullptr;
            }
            else
            {
                if (!val.contains("players")) val["players"] = json::array();
            }
        }

        // Save ranks.json
        std::ofstream ranks_out("data/ranks.json");
        if (!ranks_out.is_open())
            return std::unexpected("❌ Could not write to ranks.json");
        ranks_out << std::setw(4) << ranks << std::endl;
        ranks_out.close();

        // Save channel.json
        json channel = json::object();
        channel["channelID"] = std::to_string(channel_id);
        std::ofstream channel_out("data/channel.json");
        if (!channel_out.is_open())
            return std::unexpected("❌ Could not write to channel.json");
        channel_out << std::setw(4) << channel << std::endl;
        channel_out.close();

        // Reply
        dpp::message msg;
        msg.set_content("✅ Channel configured: <#" + std::to_string(channel_id) + ">, and all roles configured successfully");
        return msg;
    }
};