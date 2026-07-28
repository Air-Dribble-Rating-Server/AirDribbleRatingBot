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

#include "../handlers/command_handler.h"
#include "../utils/challenge_utils.h"

using json = nlohmann::ordered_json;

/**
 * @brief Discord slash command that creates a normal or bonus challenge.
 *
 * Steps:
 *  1. Load challenges, players and rank definitions.
 *  2. Insert the new challenge, then re‑sort and re‑number the list.
 *  3. Recalculate rating thresholds (ratingNeeded) for every rank in O(C+R).
 *  4. Update all affected players (same or higher rank) while preserving rank.
 *     Rating = (ratingNeeded / 1.1) + sum of completed challenges with
 *              rank strictly higher than the player's current rank.
 *  5. Re‑sort players globally and per‑rank.
 *  6. Persist changes and reply with an embed.
 */
class ChallengeCreateCommand : public ICommand
{
public:
    std::string get_name() const override { return "challenge-create"; }

    dpp::slashcommand definition(dpp::snowflake bot_id) const override
    {
        dpp::slashcommand cmd("challenge-create", "Create a new challenge", bot_id);
        cmd.set_default_permissions(dpp::p_administrator);

        // ── "normal" subcommand ──────────────────────────────────────────
        dpp::command_option normal_sub(dpp::co_sub_command, "normal", "Create a normal challenge");
        normal_sub.add_option(dpp::command_option(dpp::co_string, "name", "Challenge's name", true));
        normal_sub.add_option(dpp::command_option(dpp::co_string, "rank", "Challenge's rank", true)
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
        normal_sub.add_option(dpp::command_option(dpp::co_number, "rating", "Rating value", true));
        normal_sub.add_option(dpp::command_option(dpp::co_string, "description", "Description", true));
        normal_sub.add_option(dpp::command_option(dpp::co_string, "url1", "First example's url", true));
        normal_sub.add_option(dpp::command_option(dpp::co_string, "url2", "Optional second example's url", false));
        cmd.add_option(normal_sub);

        // ── "bonus" subcommand ───────────────────────────────────────────
        dpp::command_option bonus_sub(dpp::co_sub_command, "bonus", "Create a bonus challenge");
        bonus_sub.add_option(dpp::command_option(dpp::co_string, "name", "Challenge's name", true));
        bonus_sub.add_option(dpp::command_option(dpp::co_number, "rating", "Rating value", true));
        bonus_sub.add_option(dpp::command_option(dpp::co_string, "description", "Description", true));
        bonus_sub.add_option(dpp::command_option(dpp::co_string, "url1", "First example's url", true));
        bonus_sub.add_option(dpp::command_option(dpp::co_string, "url2", "Optional second example's url", false));
        cmd.add_option(bonus_sub);

        return cmd;
    }

    std::expected<dpp::message, std::string> execute(const dpp::slashcommand_t& event) const override
    {
        std::string subcmd = event.command.get_command_interaction().options[0].name;

        // Common parameters
        std::string name        = std::get<std::string>(event.get_parameter("name"));
        double      rating      = std::get<double>(event.get_parameter("rating"));
        std::string description = std::get<std::string>(event.get_parameter("description"));
        std::string url1        = utils::challenge::normalize_url(std::get<std::string>(event.get_parameter("url1")));
        std::string url2;
        bool        has_url2    = false;
        if (auto url2_param = event.get_parameter("url2"); auto* url2_value = std::get_if<std::string>(&url2_param))
        {
            url2     = utils::challenge::normalize_url(*url2_value);
            has_url2 = true;
        }

        if (subcmd == "normal")
        {
            std::string rank = std::get<std::string>(event.get_parameter("rank"));
            return execute_normal(std::move(name), std::move(rank), rating,
                                  std::move(description), std::move(url1), std::move(url2), has_url2);
        }
        else if (subcmd == "bonus")
        {
            return execute_bonus(std::move(name), rating,
                                 std::move(description), std::move(url1), std::move(url2), has_url2);
        }
        return std::unexpected("Unknown subcommand");
    }

private:
    // ─────────────────────────────────────────────────────────────────────
    //  Subcommand execution
    // ─────────────────────────────────────────────────────────────────────

    std::expected<dpp::message, std::string> execute_normal(
        std::string_view name, std::string_view rank, double rating,
        std::string_view description, std::string_view url1, std::string_view url2, bool has_url2) const
    {
        // ---------- load data ----------
        json challenges_data = utils::challenge::load_json("data/challenges.json");
        if (!challenges_data.contains("challenges")) challenges_data["challenges"] = json::array();
        json& challenges = challenges_data["challenges"];

        json ranks = utils::challenge::load_json("data/ranks.json");
        if (ranks.empty()) return std::unexpected("❌ ranks.json not configured. Use /set-settings first.");

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // ---------- pre‑compute helpers ----------
        std::unordered_map<std::string, int> rank_index_map  = utils::challenge::build_rank_index_map(ranks);
        std::unordered_map<std::string, int> rank_prefix_map = utils::challenge::build_rank_prefix_map(ranks);
        std::size_t num_ranks = rank_index_map.size();

        // ---------- insert & re‑number ----------
        json new_challenge;
        new_challenge["challengeName"] = name;
        new_challenge["rank"]          = rank;
        new_challenge["number"]        = -1;
        new_challenge["rating"]        = rating;
        new_challenge["description"]   = description;
        new_challenge["url1"]          = url1;
        new_challenge["url2"]          = has_url2 ? json(url2) : json(nullptr);
        challenges.push_back(std::move(new_challenge));

        auto old_to_new = utils::challenge::reassign_normal_numbers(challenges, rank_index_map, rank_prefix_map);

        // ---------- update ratingNeeded (single O(C+R) pass) ----------
        std::unordered_map<std::string, double> sum_by_rank;
        for (const auto& c : challenges)
            sum_by_rank[std::string(c["rank"].get<std::string_view>())] += c["rating"].get<double>();

        double cumulative = 0.0;
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
        {
            if (it.key() == "airdribblegod") continue;
            cumulative += sum_by_rank[it.key()];
            it.value()["ratingNeeded"] = std::round(cumulative * 1.1 * 10) / 10.0;
        }

        // ---------- precompute base_ratings per rank index ----------
        std::vector<double> base_ratings(num_ranks);
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
        {
            int idx = rank_index_map.at(it.key());
            base_ratings[idx] = it.value()["ratingNeeded"].get<double>() / 1.1;
        }

        // ---------- build fast lookup for challenge ratings & rank indices ----------
        std::unordered_map<int, double> challenge_rating_map;
        std::unordered_map<int, int>    challenge_rank_idx_map;
        challenge_rating_map.reserve(challenges.size());
        challenge_rank_idx_map.reserve(challenges.size());
        for (const auto& c : challenges)
        {
            int num = c["number"].get<int>();
            challenge_rating_map[num] = c["rating"].get<double>();
            challenge_rank_idx_map[num] = rank_index_map.at(c["rank"].get<std::string>());
        }

        std::unordered_map<std::string, double> rating_by_id;
        rating_by_id.reserve(players.size());
        for (const auto& p : players)
            rating_by_id[p["id"]] = p["rating"].get<double>();

        int new_rank_idx = rank_index_map.at(std::string(rank));

        // ---------- update all players' ratings (preserve rank) ----------
        for (auto& player : players)
        {
            utils::challenge::remap_player_numbers(player, old_to_new, false);

            std::string player_rank_key(player["rank"].get<std::string_view>());
            int         player_rank_idx = rank_index_map.at(player_rank_key);

            if (player_rank_idx >= new_rank_idx)
            {
                double base  = base_ratings[player_rank_idx];
                double extra = 0.0;

                if (player.contains("completedChallenges"))
                {
                    for (const auto& comp : player["completedChallenges"])
                    {
                        int comp_num = comp["number"].get<int>();
                        auto it = challenge_rank_idx_map.find(comp_num);
                        if (it != challenge_rank_idx_map.end() && it->second > player_rank_idx)
                            extra += challenge_rating_map.at(comp_num);
                    }
                }

                double new_rating = base + extra;
                player["rating"] = new_rating;
                rating_by_id[player["id"]] = new_rating;
            }
        }

        // ---------- sort players ----------
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
        {
            if (it.key() == "airdribblegod") continue;
            if (!it.value().contains("players")) it.value()["players"] = json::array();
            json& rank_players = it.value()["players"];
            std::sort(rank_players.begin(), rank_players.end(),
                [&](const json& a, const json& b) {
                    return rating_by_id[a["id"]] > rating_by_id[b["id"]];
                });
        }

        std::sort(players.begin(), players.end(),
            [](const json& a, const json& b) {
                return a["rating"].get<double>() > b["rating"].get<double>();
            });

        ranks["airdribblegod"]["ratingNeeded"] = players.empty() ? 0.0 : players[0]["rating"].get<double>();

        // ---------- persist ----------
        if (!utils::challenge::save_json("data/challenges.json", challenges_data))
            return std::unexpected("❌ Could not write challenges.json");
        if (!utils::challenge::save_json("data/ranks.json", ranks))
            return std::unexpected("❌ Could not write ranks.json");
        if (!utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write players.json");

        int new_number = old_to_new.at(-1);

        // ---------- embed ----------
        dpp::embed embed;
        embed.set_title("✅ Challenge created")
             .set_color(0xFFD700)
             .add_field("Name", std::string(name), true)
             .add_field("Bonus", "No", true)
             .add_field("Rank", ranks[std::string(rank)]["name"].get<std::string>(), true)
             .add_field("Number", std::format("{:03}", new_number), true)
             .add_field("Rating", std::to_string(static_cast<int>(rating)), true)
             .add_field("Description", std::string(description))
             .add_field("URL 1", std::string(url1))
             .add_field("URL 2", has_url2 ? std::string(url2) : "None");

        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Bonus challenge execution
    // ─────────────────────────────────────────────────────────────────────
    std::expected<dpp::message, std::string> execute_bonus(
        std::string_view name, double rating,
        std::string_view description, std::string_view url1, std::string_view url2, bool has_url2) const
    {
        json bonus_data = utils::challenge::load_json("data/bonus.json");
        if (!bonus_data.contains("challenges")) bonus_data["challenges"] = json::array();
        json& challenges = bonus_data["challenges"];

        json players_data = utils::challenge::load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        json new_challenge;
        new_challenge["challengeName"] = name;
        new_challenge["number"]        = -1;
        new_challenge["rating"]        = rating;
        new_challenge["description"]   = description;
        new_challenge["url1"]          = url1;
        new_challenge["url2"]          = has_url2 ? json(url2) : json(nullptr);
        challenges.push_back(std::move(new_challenge));

        auto old_to_new = utils::challenge::reassign_bonus_numbers(challenges);

        std::unordered_map<int, double> bonus_rating_by_number;
        bonus_rating_by_number.reserve(challenges.size());
        for (const auto& c : challenges)
            bonus_rating_by_number[c["number"].get<int>()] = c["rating"].get<double>();

        for (auto& player : players)
        {
            utils::challenge::remap_player_numbers(player, old_to_new, true);

            double total_bonus = 0.0;
            if (player.contains("completedBonus"))
            {
                for (const auto& b : player["completedBonus"])
                {
                    auto it = bonus_rating_by_number.find(b["number"].get<int>());
                    if (it != bonus_rating_by_number.end())
                        total_bonus += it->second;
                }
            }
            player["bonusRating"] = total_bonus;
        }    

        if (!utils::challenge::save_json("data/bonus.json", bonus_data))
            return std::unexpected("❌ Could not write bonus.json");
        if (!utils::challenge::save_json("data/players.json", players_data))
            return std::unexpected("❌ Could not write players.json");

        int new_number = old_to_new.at(-1);

        dpp::embed embed;
        embed.set_title("✅ Challenge created")
             .set_color(0xFFD700)
             .add_field("Name", std::string(name), true)
             .add_field("Bonus", "Yes", true)
             .add_field("Number", std::format("{:03}", new_number), true)
             .add_field("Rating", std::to_string(static_cast<int>(rating)), true)
             .add_field("Description", std::string(description))
             .add_field("URL 1", std::string(url1))
             .add_field("URL 2", has_url2 ? std::string(url2) : "None");

        dpp::message msg;
        msg.add_embed(embed);
        return msg;
    }
};
