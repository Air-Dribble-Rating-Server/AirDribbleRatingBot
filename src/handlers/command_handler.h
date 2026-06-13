#pragma once
#include <dpp/dpp.h>
#include <vector>
#include <memory>
#include <expected>
#include <string>

class ICommand
{
public:
    virtual ~ICommand() = default;

    virtual std::string get_name() const = 0;

    virtual dpp::slashcommand definition(dpp::snowflake bot_id) const = 0;

    virtual std::expected<dpp::message, std::string> execute(const dpp::slashcommand_t& event) const = 0;
};

class CommandHandler
{
public:
    explicit CommandHandler(dpp::cluster& bot_ref);

    void register_all();
private:
    dpp::cluster& bot;
    std::vector<std::unique_ptr<ICommand>> commands;

    void load_commands();

    void on_slash_command(const dpp::slashcommand_t& event);
};