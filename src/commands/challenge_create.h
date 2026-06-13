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
        std::string url1        = normalize_url(std::get<std::string>(event.get_parameter("url1")));
        std::string url2;
        bool        has_url2    = false;
        {
            auto url2_param = event.get_parameter("url2");
            if (std::holds_alternative<std::string>(url2_param))
            {
                url2     = normalize_url(std::get<std::string>(url2_param));
                has_url2 = true;
            }
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
    //  File helpers
    // ─────────────────────────────────────────────────────────────────────

    /** Loads a JSON file, returning an empty object on error. */
    json load_json(const std::string& path) const
    {
        json j;
        std::ifstream f(path);
        if (f.is_open())
        {
            try { j = json::parse(f); }
            catch (...) { j = json::object(); }
            f.close();
        }
        else { j = json::object(); }
        return j;
    }

    /** Saves a JSON object to file with indentation. */
    inline bool save_json(const std::string& path, const json& j) const
    {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << std::setw(4) << j << std::endl;
        f.close();
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  URL normalization
    // ─────────────────────────────────────────────────────────────────────

    /** Ensures the URL starts with http:// or https://, prepending https:// if missing. */
    static std::string normalize_url(std::string_view url)
    {
        if (url.starts_with("http://") || url.starts_with("https://"))
            return std::string(url);
        return "https://" + std::string(url);
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Rank helpers
    // ─────────────────────────────────────────────────────────────────────

    /** Builds a mapping rank key → index (position in ranks.json). */
    static std::unordered_map<std::string, int> build_rank_index_map(const json& ranks)
    {
        std::unordered_map<std::string, int> map;
        map.reserve(ranks.size());
        int idx = 0;
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
            map[it.key()] = idx++;
        return map;
    }

    /** Builds a mapping rank key → numbering prefix index (excludes grounddweller). */
    static std::unordered_map<std::string, int> build_rank_prefix_map(const json& ranks)
    {
        std::unordered_map<std::string, int> map;
        map.reserve(ranks.size());
        int prefix = 0;
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
            if (it.key() != "grounddweller")
                map[it.key()] = prefix++;
        return map;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Challenge numbering & re‑ordering
    // ─────────────────────────────────────────────────────────────────────

    /**
     * Sorts normal challenges by rank hierarchy → rating ascending,
     * then assigns rank‑prefixed numbers (1,2,3… / 101,102… etc.).
     *
     * @return Mapping old number → new number.
     */
    std::unordered_map<int, int> reassign_normal_numbers(
        json&                                         challenges,
        const std::unordered_map<std::string, int>&   rank_index_map,
        const std::unordered_map<std::string, int>&   rank_prefix_map) const
    {
        // Sort by rank index then rating (both ascending)
        std::sort(challenges.begin(), challenges.end(),
            [&](const json& a, const json& b) {
                int idxA = rank_index_map.at(a["rank"].get<std::string>());
                int idxB = rank_index_map.at(b["rank"].get<std::string>());
                if (idxA != idxB) return idxA < idxB;
                return a["rating"].get<double>() < b["rating"].get<double>();
            });

        std::unordered_map<int, int>         old_to_new;
        old_to_new.reserve(challenges.size());
        std::unordered_map<std::string, int> counter_per_rank;

        for (auto& c : challenges)
        {
            std::string rank_str(c["rank"].get<std::string_view>());
            int         old_num = c["number"].get<int>();
            int&        counter = counter_per_rank[rank_str];

            int rank_prefix = rank_prefix_map.at(rank_str);
            int new_num     = rank_prefix * 100 + counter + 1;
            old_to_new[old_num] = new_num;
            c["number"]         = new_num;
            ++counter;
        }
        return old_to_new;
    }

    /**
     * Sorts bonus challenges by rating ascending, assigns sequential numbers 1,2,3…
     *
     * @return Mapping old number → new number.
     */
    std::unordered_map<int, int> reassign_bonus_numbers(json& challenges) const
    {
        std::sort(challenges.begin(), challenges.end(),
            [](const json& a, const json& b) {
                return a["rating"].get<double>() < b["rating"].get<double>();
            });

        std::unordered_map<int, int> old_to_new;
        old_to_new.reserve(challenges.size());
        int idx = 1;
        for (auto& c : challenges)
        {
            int old_num = c["number"].get<int>();
            c["number"] = idx;
            old_to_new[old_num] = idx;
            ++idx;
        }
        return old_to_new;
    }

    /** Remaps challenge numbers inside a player's completed/waiting arrays. */
    void remap_player_numbers(json& player, const std::unordered_map<int, int>& mapping, bool is_bonus) const
    {
        auto remap_array = [&](json& arr) {
            for (auto& item : arr)
            {
                auto it = mapping.find(item["number"].get<int>());
                if (it != mapping.end())
                    item["number"] = it->second;
            }
            std::sort(arr.begin(), arr.end(),
                [](const json& a, const json& b) { return a["number"].get<int>() < b["number"].get<int>(); });
        };

        if (is_bonus)
        {
            if (player.contains("completedBonus"))
                remap_array(player["completedBonus"]);
            if (player.contains("bonusWaitingToBeRated"))
                remap_array(player["bonusWaitingToBeRated"]);
        }
        else
        {
            if (player.contains("completedChallenges"))
                remap_array(player["completedChallenges"]);
            if (player.contains("challengesWaitingToBeRated"))
                remap_array(player["challengesWaitingToBeRated"]);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Subcommand execution
    // ─────────────────────────────────────────────────────────────────────

    std::expected<dpp::message, std::string> execute_normal(
        std::string_view name, std::string_view rank, double rating,
        std::string_view description, std::string_view url1, std::string_view url2, bool has_url2) const
    {
        // ---------- load data ----------
        json challenges_data = load_json("data/challenges.json");
        if (!challenges_data.contains("challenges")) challenges_data["challenges"] = json::array();
        json& challenges = challenges_data["challenges"];

        json ranks = load_json("data/ranks.json");
        if (ranks.empty()) return std::unexpected("❌ ranks.json not configured. Use /set-settings first.");

        json players_data = load_json("data/players.json");
        if (!players_data.contains("players")) players_data["players"] = json::array();
        json& players = players_data["players"];

        // ---------- pre‑compute helpers ----------
        std::unordered_map<std::string, int> rank_index_map  = build_rank_index_map(ranks);
        std::unordered_map<std::string, int> rank_prefix_map = build_rank_prefix_map(ranks);
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

        auto old_to_new = reassign_normal_numbers(challenges, rank_index_map, rank_prefix_map);

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
            remap_player_numbers(player, old_to_new, false);

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
        if (!save_json("data/challenges.json", challenges_data))
            return std::unexpected("❌ Could not write challenges.json");
        if (!save_json("data/ranks.json", ranks))
            return std::unexpected("❌ Could not write ranks.json");
        if (!save_json("data/players.json", players_data))
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
        json bonus_data = load_json("data/bonus.json");
        if (!bonus_data.contains("challenges")) bonus_data["challenges"] = json::array();
        json& challenges = bonus_data["challenges"];

        json players_data = load_json("data/players.json");
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

        auto old_to_new = reassign_bonus_numbers(challenges);

        std::unordered_map<int, double> bonus_rating_by_number;
        bonus_rating_by_number.reserve(challenges.size());
        for (const auto& c : challenges)
            bonus_rating_by_number[c["number"].get<int>()] = c["rating"].get<double>();

        for (auto& player : players)
        {
            remap_player_numbers(player, old_to_new, true);

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

        if (!save_json("data/bonus.json", bonus_data))
            return std::unexpected("❌ Could not write bonus.json");
        if (!save_json("data/players.json", players_data))
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