#pragma once
#include <dpp/dpp.h>
#include <expected>
#include <string>
#include <string_view>
#include <fstream>
#include <print>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <format>
#include <optional>
#include <iostream>

#include "../handlers/command_handler.h"
#include "../utils/challenge_utils.h"

using json = nlohmann::ordered_json;

/**
 * @brief Discord slash command that edits or deletes a normal or bonus challenge.
 *
 * Steps for edit:
 *  1. Load challenges, players and rank definitions.
 *  2. Find the challenge by number, update provided fields.
 *  3. If rank or rating changed → re‑sort, re‑number and recalc thresholds.
 *  4. Recalculate every player's normal rating (rank unchanged).
 *  5. Re‑sort players and persist.
 *
 * Steps for delete:
 *  1. Remove the challenge, re‑sort and re‑number.
 *  2. Recalc thresholds and update players (remove deleted challenge from lists).
 *  3. Re‑sort players and persist.
 */
class ChallengeEditCommand : public ICommand
{
public:
    std::string get_name() const override { return "challenge-edit"; }

    dpp::slashcommand definition(dpp::snowflake bot_id) const override
    {
        dpp::slashcommand cmd("challenge-edit", "Edit a challenge", bot_id);
        cmd.set_default_permissions(dpp::p_administrator);

        // ── normal subcommand group ─────────────────────────────────────
        dpp::command_option normal_group(dpp::co_sub_command_group, "normal", "Edit or delete a normal challenge");

        dpp::command_option normal_edit(dpp::co_sub_command, "edit", "Edit a normal challenge");
        normal_edit.add_option(dpp::command_option(dpp::co_number, "number", "Number of the challenge to edit", true));
        normal_edit.add_option(dpp::command_option(dpp::co_string, "name", "Edit challenge's name", false));
        normal_edit.add_option(dpp::command_option(dpp::co_string, "rank", "Edit challenge's rank", false)
            .add_choice(dpp::command_option_choice("Beginner", "beginner"))
            .add_choice(dpp::command_option_choice("Apprentice", "apprentice"))
            .add_choice(dpp::command_option_choice("Intermediate", "intermediate"))
            .add_choice(dpp::command_option_choice("Advanced", "advanced"))
            .add_choice(dpp::command_option_choice("Expert", "expert"))
            .add_choice(dpp::command_option_choice("Master", "master"))
            .add_choice(dpp::command_option_choice("Legend", "legend"))
            .add_choice(dpp::command_option_choice("Mythic", "mythic"))
            .add_choice(dpp::command_option_choice("Demigod", "demigod"))
            .add_choice(dpp::command_option_choice("Air dribble god", "airdribblegod"))
        );
        normal_edit.add_option(dpp::command_option(dpp::co_number, "rating", "Edit rating value", false));
        normal_edit.add_option(dpp::command_option(dpp::co_string, "description", "Edit description", false));
        normal_edit.add_option(dpp::command_option(dpp::co_string, "url1", "Edit first example's url", false));
        normal_edit.add_option(dpp::command_option(dpp::co_string, "url2", "Edit optional second example's url", false));
        normal_edit.add_option(dpp::command_option(dpp::co_boolean, "delete_url2", "Delete the second url", false));
        normal_group.add_option(normal_edit);

        dpp::command_option normal_delete(dpp::co_sub_command, "delete", "Delete a normal challenge");
        normal_delete.add_option(dpp::command_option(dpp::co_number, "number", "Number of the challenge to delete", true));
        normal_group.add_option(normal_delete);
        cmd.add_option(normal_group);

        // ── bonus subcommand group ──────────────────────────────────────
        dpp::command_option bonus_group(dpp::co_sub_command_group, "bonus", "Edit or delete a bonus challenge");

        dpp::command_option bonus_edit(dpp::co_sub_command, "edit", "Edit a bonus challenge");
        bonus_edit.add_option(dpp::command_option(dpp::co_number, "number", "Number of the challenge to edit", true));
        bonus_edit.add_option(dpp::command_option(dpp::co_string, "name", "Edit challenge's name", false));
        bonus_edit.add_option(dpp::command_option(dpp::co_number, "rating", "Edit rating value", false));
        bonus_edit.add_option(dpp::command_option(dpp::co_string, "description", "Edit description", false));
        bonus_edit.add_option(dpp::command_option(dpp::co_string, "url1", "Edit first example's url", false));
        bonus_edit.add_option(dpp::command_option(dpp::co_string, "url2", "Edit optional second example's url", false));
        bonus_edit.add_option(dpp::command_option(dpp::co_boolean, "delete_url2", "Delete the second url", false));
        bonus_group.add_option(bonus_edit);

        dpp::command_option bonus_delete(dpp::co_sub_command, "delete", "Delete a bonus challenge");
        bonus_delete.add_option(dpp::command_option(dpp::co_number, "number", "Number of the challenge to delete", true));
        bonus_group.add_option(bonus_delete);
        cmd.add_option(bonus_group);

        return cmd;
    }

    std::expected<dpp::message, std::string> execute(const dpp::slashcommand_t& event) const override
    {
        const auto& opts = event.command.get_command_interaction().options;
        if (opts.empty())
            return std::unexpected("❌ No options provided.");

        // The first option must be a subcommand group
        const auto& root_opt = opts[0];
        if (root_opt.type != dpp::co_sub_command_group)
            return std::unexpected("❌ Expected subcommand group.");

        if (root_opt.options.empty())
            return std::unexpected("❌ No subcommand inside group.");

        const auto& sub_opt = root_opt.options[0];  // the subcommand ("edit" or "delete")
        std::string group = root_opt.name;
        std::string subcmd = sub_opt.name;

        // Helper to safely get a parameter from the subcommand's options
        auto get_param = [&](const std::string& name) -> std::optional<dpp::command_data_option> {
            for (const auto& p : sub_opt.options)
                if (p.name == name) return p;
            return std::nullopt;
            };

        // Extractors
        auto get_string = [&](const std::string& name) -> std::optional<std::string> {
            auto p = get_param(name);
            if (p && std::holds_alternative<std::string>(p->value))
                return std::get<std::string>(p->value);
            return std::nullopt;
            };
        auto get_number = [&](const std::string& name) -> std::optional<double> {
            auto p = get_param(name);
            if (p && std::holds_alternative<double>(p->value))
                return std::get<double>(p->value);
            return std::nullopt;
            };
        auto get_bool = [&](const std::string& name) -> std::optional<bool> {
            auto p = get_param(name);
            if (p && std::holds_alternative<bool>(p->value))
                return std::get<bool>(p->value);
            return std::nullopt;
            };

        // 'number' is always required (in edit and delete)
        auto num_opt = get_number("number");
        if (!num_opt)
            return std::unexpected("❌ Number parameter is required.");
        int number = static_cast<int>(*num_opt);

        if (group == "normal" && subcmd == "edit")
        {
            auto name = get_string("name");
            auto rank = get_string("rank");
            auto rating = get_number("rating");
            auto description = get_string("description");
            auto url1 = get_string("url1");
            auto url2 = get_string("url2");
            auto delete_url2 = get_bool("delete_url2");

            if (!name && !rank && !rating && !description && !url1 && !url2 && !delete_url2)
                return std::unexpected("❌ No changes were provided.");

            return edit_normal(number, name, rank, rating, description, url1, url2, delete_url2);
        }
        else if (group == "normal" && subcmd == "delete")
        {
            return delete_normal(number);
        }
        else if (group == "bonus" && subcmd == "edit")
        {
            auto name = get_string("name");
            auto rating = get_number("rating");
            auto description = get_string("description");
            auto url1 = get_string("url1");
            auto url2 = get_string("url2");
            auto delete_url2 = get_bool("delete_url2");

            if (!name && !rating && !description && !url1 && !url2 && !delete_url2)
                return std::unexpected("❌ No changes were provided.");

            return edit_bonus(number, name, rating, description, url1, url2, delete_url2);
        }
        else if (group == "bonus" && subcmd == "delete")
        {
            return delete_bonus(number);
        }

        return std::unexpected("Unknown subcommand");
    }

private:
    // ─────────────────────────────────────────────────────────────
    //  Normal challenge edit
    // ─────────────────────────────────────────────────────────────
    std::expected<dpp::message, std::string> edit_normal(
        int number,
        std::optional<std::string> new_name,
        std::optional<std::string> new_rank,
        std::optional<double> new_rating,
        std::optional<std::string> new_description,
        std::optional<std::string> new_url1,
        std::optional<std::string> new_url2,
        std::optional<bool> delete_url2) const
    {
        json challenges_data = utils::challenge::load_json("data/challenges.json");
        if (!challenges_data.contains("challenges")) challenges_data["challenges"] = json::array();
        json& challenges = challenges_data["challenges"];

        // Find the original challenge
        auto it = std::find_if(challenges.begin(), challenges.end(),
            [number](const json& c) { return c["number"].get<int>() == number; });
        if (it == challenges.end())
            return std::unexpected("❌ Challenge #" + std::to_string(number) + " not found.");

        json ranks = utils::challenge::load_json("data/ranks.json");
        if (ranks.empty()) return std::unexpected("❌ ranks.json not configured.");

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // Apply modifications
        bool rank_or_rating_changed = false;
        if (new_name) (*it)["challengeName"] = *new_name;
        if (new_rank) { (*it)["rank"] = *new_rank; rank_or_rating_changed = true; }
        if (new_rating) { (*it)["rating"] = *new_rating; rank_or_rating_changed = true; }
        if (new_description) (*it)["description"] = *new_description;
        if (new_url1) (*it)["url1"] = utils::challenge::normalize_url(*new_url1);
        if (new_url2) (*it)["url2"] = utils::challenge::normalize_url(*new_url2);
        if (delete_url2 && *delete_url2) (*it)["url2"] = nullptr;

        // Store the new number (it may change after reordering)
        int new_number = number;

        if (rank_or_rating_changed) {
            // Reorder and renumber
            auto rank_index_map = utils::challenge::build_rank_index_map(ranks);
            auto rank_prefix_map = utils::challenge::build_rank_prefix_map(ranks);
            auto old_to_new = utils::challenge::reassign_normal_numbers(challenges, rank_index_map, rank_prefix_map);

            // Retrieve the new number of the edited challenge
            new_number = old_to_new.at(number);

            // Update ratingNeeded
            std::unordered_map<std::string, double> sum_by_rank;
            for (auto& c : challenges)
                sum_by_rank[std::string(c["rank"].get<std::string_view>())] += c["rating"].get<double>();
            double cumulative = 0.0;
            for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
                if (rit.key() == "airdribblegod") continue;
                cumulative += sum_by_rank[rit.key()];
                rit.value()["ratingNeeded"] = std::round(cumulative * 1.1 * 10) / 10.0;
            }

            // Precompute base ratings
            std::size_t num_ranks = rank_index_map.size();
            std::vector<double> base_ratings(num_ranks);
            for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
                int idx = rank_index_map.at(rit.key());
                base_ratings[idx] = rit.value()["ratingNeeded"].get<double>() / 1.1;
            }

            // Fast lookup maps for challenges
            std::unordered_map<int, double> challenge_rating_map;
            std::unordered_map<int, int> challenge_rank_idx_map;
            challenge_rating_map.reserve(challenges.size());
            challenge_rank_idx_map.reserve(challenges.size());
            for (auto& c : challenges) {
                int num = c["number"].get<int>();
                challenge_rating_map[num] = c["rating"].get<double>();
                challenge_rank_idx_map[num] = rank_index_map.at(c["rank"].get<std::string>());
            }

            // Update players' ratings (without changing rank)
            for (auto& player : players) {
                utils::challenge::remap_player_numbers(player, old_to_new, false);
                std::string player_rank_key(player["rank"].get<std::string_view>());
                int player_rank_idx = rank_index_map.at(player_rank_key);
                double base = base_ratings[player_rank_idx];
                double extra = 0.0;
                if (player.contains("completedChallenges")) {
                    for (auto& comp : player["completedChallenges"]) {
                        int comp_num = comp["number"].get<int>();
                        auto cit = challenge_rank_idx_map.find(comp_num);
                        if (cit != challenge_rank_idx_map.end() && cit->second > player_rank_idx)
                            extra += challenge_rating_map.at(comp_num);
                    }
                }
                player["rating"] = base + extra;
            }

            // Sort players
            std::unordered_map<std::string, double> rating_by_id;
            rating_by_id.reserve(players.size());
            for (auto& p : players) rating_by_id[p["id"]] = p["rating"].get<double>();
            for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
                if (rit.key() == "airdribblegod") continue;
                if (!rit.value().contains("players")) rit.value()["players"] = json::array();
                json& rank_players = rit.value()["players"];
                std::sort(rank_players.begin(), rank_players.end(),
                    [&](const json& a, const json& b) { return rating_by_id[a["id"]] > rating_by_id[b["id"]]; });
            }
            std::sort(players.begin(), players.end(),
                [](const json& a, const json& b) { return a["rating"].get<double>() > b["rating"].get<double>(); });
            ranks["airdribblegod"]["ratingNeeded"] = players.empty() ? 0.0 : players[0]["rating"].get<double>();
        }

        if (!utils::challenge::save_json("data/challenges.json", challenges_data) ||
            !utils::challenge::save_json("data/ranks.json", ranks) ||
            !utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write changes.");

        // Find the challenge with the updated number for the embed
        auto edited_it = std::find_if(challenges.begin(), challenges.end(),
            [new_number](const json& c) { return c["number"].get<int>() == new_number; });
        if (edited_it == challenges.end())
            return std::unexpected("❌ Challenge not found after edit.");

        dpp::embed embed;
        embed.set_title("✅ Challenge edited")
            .set_color(0xFFD700)
            .add_field("Name", edited_it->at("challengeName").get<std::string>(), true)
            .add_field("Bonus", "No", true)
            .add_field("Rank", ranks[edited_it->at("rank").get<std::string>()]["name"].get<std::string>(), true)
            .add_field("Number", std::format("{:03}", new_number), true)
            .add_field("Rating", std::to_string(static_cast<int>(edited_it->at("rating").get<double>())), true)
            .add_field("Description", edited_it->at("description").get<std::string>())
            .add_field("URL 1", edited_it->at("url1").get<std::string>())
            .add_field("URL 2", edited_it->at("url2").is_null() ? "None" : edited_it->at("url2").get<std::string>());
        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }

    // ─────────────────────────────────────────────────────────────
    //  Normal challenge delete
    // ─────────────────────────────────────────────────────────────
    std::expected<dpp::message, std::string> delete_normal(int number) const
    {
        json challenges_data = utils::challenge::load_json("data/challenges.json");
        if (!challenges_data.contains("challenges")) challenges_data["challenges"] = json::array();
        json& challenges = challenges_data["challenges"];

        auto it = std::find_if(challenges.begin(), challenges.end(),
            [number](const json& c) { return c["number"].get<int>() == number; });
        if (it == challenges.end())
            return std::unexpected("❌ Challenge #" + std::to_string(number) + " not found.");

        json ranks = utils::challenge::load_json("data/ranks.json");
        if (ranks.empty()) return std::unexpected("❌ ranks.json not configured.");

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // Delete the challenge
        challenges.erase(it);

        // Reorder and renumber
        auto rank_index_map = utils::challenge::build_rank_index_map(ranks);
        auto rank_prefix_map = utils::challenge::build_rank_prefix_map(ranks);
        auto old_to_new = utils::challenge::reassign_normal_numbers(challenges, rank_index_map, rank_prefix_map);

        // Update ratingNeeded
        std::unordered_map<std::string, double> sum_by_rank;
        for (auto& c : challenges)
            sum_by_rank[std::string(c["rank"].get<std::string_view>())] += c["rating"].get<double>();
        double cumulative = 0.0;
        for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
            if (rit.key() == "airdribblegod") continue;
            cumulative += sum_by_rank[rit.key()];
            rit.value()["ratingNeeded"] = std::round(cumulative * 1.1 * 10) / 10.0;
        }

        // Precompute base ratings and challenge maps
        std::size_t num_ranks = rank_index_map.size();
        std::vector<double> base_ratings(num_ranks);
        for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
            int idx = rank_index_map.at(rit.key());
            base_ratings[idx] = rit.value()["ratingNeeded"].get<double>() / 1.1;
        }

        std::unordered_map<int, double> challenge_rating_map;
        std::unordered_map<int, int> challenge_rank_idx_map;
        challenge_rating_map.reserve(challenges.size());
        challenge_rank_idx_map.reserve(challenges.size());
        for (auto& c : challenges) {
            int num = c["number"].get<int>();
            challenge_rating_map[num] = c["rating"].get<double>();
            challenge_rank_idx_map[num] = rank_index_map.at(c["rank"].get<std::string>());
        }

        // Update players (remove deleted challenge from lists and recalc rating)
        for (auto& player : players) {
            // Remove from lists
            if (player.contains("completedChallenges")) {
                auto& arr = player["completedChallenges"];
                arr.erase(std::remove_if(arr.begin(), arr.end(),
                    [number](const json& c) { return c["number"].get<int>() == number; }), arr.end());
            }
            if (player.contains("challengesWaitingToBeRated")) {
                auto& arr = player["challengesWaitingToBeRated"];
                arr.erase(std::remove_if(arr.begin(), arr.end(),
                    [number](const json& c) { return c["number"].get<int>() == number; }), arr.end());
            }
            utils::challenge::remap_player_numbers(player, old_to_new, false);

            // Recalculate rating
            std::string player_rank_key(player["rank"].get<std::string_view>());
            int player_rank_idx = rank_index_map.at(player_rank_key);
            double base = base_ratings[player_rank_idx];
            double extra = 0.0;
            if (player.contains("completedChallenges")) {
                for (auto& comp : player["completedChallenges"]) {
                    int comp_num = comp["number"].get<int>();
                    auto cit = challenge_rank_idx_map.find(comp_num);
                    if (cit != challenge_rank_idx_map.end() && cit->second > player_rank_idx)
                        extra += challenge_rating_map.at(comp_num);
                }
            }
            player["rating"] = base + extra;
        }

        // Sort players
        std::unordered_map<std::string, double> rating_by_id;
        rating_by_id.reserve(players.size());
        for (auto& p : players) rating_by_id[p["id"]] = p["rating"].get<double>();
        for (auto rit = ranks.items().begin(); rit != ranks.items().end(); ++rit) {
            if (rit.key() == "airdribblegod") continue;
            if (!rit.value().contains("players")) rit.value()["players"] = json::array();
            json& rank_players = rit.value()["players"];
            std::sort(rank_players.begin(), rank_players.end(),
                [&](const json& a, const json& b) { return rating_by_id[a["id"]] > rating_by_id[b["id"]]; });
        }
        std::sort(players.begin(), players.end(),
            [](const json& a, const json& b) { return a["rating"].get<double>() > b["rating"].get<double>(); });
        ranks["airdribblegod"]["ratingNeeded"] = players.empty() ? 0.0 : players[0]["rating"].get<double>();

        if (!utils::challenge::save_json("data/challenges.json", challenges_data) ||
            !utils::challenge::save_json("data/ranks.json", ranks) ||
            !utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write changes.");

        dpp::embed embed;
        embed.set_title("🗑️ Challenge deleted")
            .set_color(0xFF0000)
            .add_field("Number", std::format("{:03}", number), true)
            .add_field("Bonus", "No", true)
            .add_field("Status", "Deleted successfully", true);
        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }

    // ─────────────────────────────────────────────────────────────
    //  Bonus challenge edit
    // ─────────────────────────────────────────────────────────────
    std::expected<dpp::message, std::string> edit_bonus(
        int number,
        std::optional<std::string> new_name,
        std::optional<double> new_rating,
        std::optional<std::string> new_description,
        std::optional<std::string> new_url1,
        std::optional<std::string> new_url2,
        std::optional<bool> delete_url2) const
    {
        json bonus_data = utils::challenge::load_json("data/bonus.json");
        if (!bonus_data.contains("challenges")) bonus_data["challenges"] = json::array();
        json& challenges = bonus_data["challenges"];

        auto it = std::find_if(challenges.begin(), challenges.end(),
            [number](const json& c) { return c["number"].get<int>() == number; });
        if (it == challenges.end())
            return std::unexpected("❌ Bonus challenge #" + std::to_string(number) + " not found.");

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // Apply modifications
        bool rating_changed = false;
        if (new_name) (*it)["challengeName"] = *new_name;
        if (new_rating) { (*it)["rating"] = *new_rating; rating_changed = true; }
        if (new_description) (*it)["description"] = *new_description;
        if (new_url1) (*it)["url1"] = utils::challenge::normalize_url(*new_url1);
        if (new_url2) (*it)["url2"] = utils::challenge::normalize_url(*new_url2);
        if (delete_url2 && *delete_url2) (*it)["url2"] = nullptr;

        int new_number = number; // in case rating didn't change

        if (rating_changed) {
            auto old_to_new = utils::challenge::reassign_bonus_numbers(challenges);
            new_number = old_to_new.at(number);

            // Rating lookup by number
            std::unordered_map<int, double> bonus_rating_by_number;
            bonus_rating_by_number.reserve(challenges.size());
            for (auto& c : challenges)
                bonus_rating_by_number[c["number"].get<int>()] = c["rating"].get<double>();

            for (auto& player : players) {
                utils::challenge::remap_player_numbers(player, old_to_new, true);
                double total_bonus = 0.0;
                if (player.contains("completedBonus")) {
                    for (auto& b : player["completedBonus"]) {
                        auto bit = bonus_rating_by_number.find(b["number"].get<int>());
                        if (bit != bonus_rating_by_number.end()) total_bonus += bit->second;
                    }
                }
                player["bonusRating"] = total_bonus;
            }
        }

        if (!utils::challenge::save_json("data/bonus.json", bonus_data) ||
            !utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write changes.");

        // Find the updated challenge
        auto edited_it = std::find_if(challenges.begin(), challenges.end(),
            [new_number](const json& c) { return c["number"].get<int>() == new_number; });
        if (edited_it == challenges.end())
            return std::unexpected("❌ Bonus challenge not found after edit.");

        dpp::embed embed;
        embed.set_title("✅ Bonus challenge edited")
            .set_color(0xFFD700)
            .add_field("Name", edited_it->at("challengeName").get<std::string>(), true)
            .add_field("Bonus", "Yes", true)
            .add_field("Number", std::format("{:03}", new_number), true)
            .add_field("Rating", std::to_string(static_cast<int>(edited_it->at("rating").get<double>())), true)
            .add_field("Description", edited_it->at("description").get<std::string>())
            .add_field("URL 1", edited_it->at("url1").get<std::string>())
            .add_field("URL 2", edited_it->at("url2").is_null() ? "None" : edited_it->at("url2").get<std::string>());
        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }

    // ─────────────────────────────────────────────────────────────
    //  Bonus challenge delete
    // ─────────────────────────────────────────────────────────────
    std::expected<dpp::message, std::string> delete_bonus(int number) const
    {
        json bonus_data = utils::challenge::load_json("data/bonus.json");
        if (!bonus_data.contains("challenges")) bonus_data["challenges"] = json::array();
        json& challenges = bonus_data["challenges"];

        auto it = std::find_if(challenges.begin(), challenges.end(),
            [number](const json& c) { return c["number"].get<int>() == number; });
        if (it == challenges.end())
            return std::unexpected("❌ Bonus challenge #" + std::to_string(number) + " not found.");

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // Delete the challenge
        challenges.erase(it);
        auto old_to_new = utils::challenge::reassign_bonus_numbers(challenges);

        std::unordered_map<int, double> bonus_rating_by_number;
        bonus_rating_by_number.reserve(challenges.size());
        for (auto& c : challenges)
            bonus_rating_by_number[c["number"].get<int>()] = c["rating"].get<double>();

        for (auto& player : players) {
            // Remove from lists
            if (player.contains("completedBonus")) {
                auto& arr = player["completedBonus"];
                arr.erase(std::remove_if(arr.begin(), arr.end(),
                    [number](const json& c) { return c["number"].get<int>() == number; }), arr.end());
            }
            if (player.contains("bonusWaitingToBeRated")) {
                auto& arr = player["bonusWaitingToBeRated"];
                arr.erase(std::remove_if(arr.begin(), arr.end(),
                    [number](const json& c) { return c["number"].get<int>() == number; }), arr.end());
            }
            utils::challenge::remap_player_numbers(player, old_to_new, true);

            double total_bonus = 0.0;
            if (player.contains("completedBonus")) {
                for (auto& b : player["completedBonus"]) {
                    auto bit = bonus_rating_by_number.find(b["number"].get<int>());
                    if (bit != bonus_rating_by_number.end()) total_bonus += bit->second;
                }
            }
            player["bonusRating"] = total_bonus;
        }

        if (!utils::challenge::save_json("data/bonus.json", bonus_data) ||
            !utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write changes.");

        dpp::embed embed;
        embed.set_title("🗑️ Bonus challenge deleted")
            .set_color(0xFF0000)
            .add_field("Number", std::format("{:03}", number), true)
            .add_field("Bonus", "Yes", true)
            .add_field("Status", "Deleted successfully", true);
        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }
};