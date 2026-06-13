#include <print>

#include "command_handler.h"
#include "../commands/ping.h"

CommandHandler::CommandHandler(dpp::cluster& bot_ref) : bot(bot_ref)
{
    load_commands();
}

void CommandHandler::load_commands()
{
    commands.reserve(1); // This has to be changed based on how many commands we have
    commands.push_back(std::make_unique<PingCommand>());
}

void CommandHandler::register_all()
{
    // Register all the commands when the bot is ready
    bot.on_ready([this](const dpp::ready_t&)
    {
        for (const auto& cmd : commands)
        {
            bot.global_command_create(cmd->definition(bot.me.id));
        }
        std::println("{} registered commands.", commands.size());
    });

    bot.on_slashcommand([this](const dpp::slashcommand_t& event)
    {
        on_slash_command(event);
    });
}

void CommandHandler::on_slash_command(const dpp::slashcommand_t& event)
{
    std::string cmd_name = event.command.get_command_name();

    for (const auto& cmd : commands)
    {
        if (cmd->get_name() == cmd_name)
        {
            try
            {
                event.thinking();
                auto result = cmd->execute(event);
                if (result)
                {
                    event.edit_response(result.value());  // result.value() es un dpp::message
                }
                else
                {
                    event.edit_response("❌ Error: " + result.error());
                }
            }
            catch (const std::exception& e)
            {
                try
                {
                    event.edit_response("❌ Unexpected error: " + std::string(e.what()));
                }
                catch (...)
                {
                    event.reply("❌ Unexpected error: " + std::string(e.what()) + "\n\n**Contact with a developer to fix this.**");
                }
            }
            return;
        }
    }
    event.reply("❌ Unknown command: " + cmd_name);
}