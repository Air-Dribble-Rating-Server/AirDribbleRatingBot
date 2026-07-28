#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <fstream>
#include <print>

namespace utils::challenge
{
    using json = nlohmann::ordered_json;

    // ---- File helpers ----
    inline json load_json(const std::string& path)
    {
        json j;
        std::ifstream f(path);
        if (f.is_open()) {
            try { j = json::parse(f); }
            catch (...) { j = json::object(); }
            f.close();
        }
        else {
            j = json::object();
        }
        return j;
    }

    inline bool save_json(const std::string& path, const json& j)
    {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << std::setw(4) << j << std::endl;
        f.close();
        return true;
    }

    // ---- URL normalization ----
    inline std::string normalize_url(std::string_view url)
    {
        if (url.starts_with("http://") || url.starts_with("https://"))
            return std::string(url);
        return "https://" + std::string(url);
    }

    // ---- Rank helpers ----
    inline std::unordered_map<std::string, int> build_rank_index_map(const json& ranks)
    {
        std::unordered_map<std::string, int> map;
        map.reserve(ranks.size());
        int idx = 0;
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
            map[it.key()] = idx++;
        return map;
    }

    inline std::unordered_map<std::string, int> build_rank_prefix_map(const json& ranks)
    {
        std::unordered_map<std::string, int> map;
        map.reserve(ranks.size());
        int prefix = 0;
        for (auto it = ranks.items().begin(); it != ranks.items().end(); ++it)
            if (it.key() != "grounddweller")
                map[it.key()] = prefix++;
        return map;
    }

    // ---- Challenge numbering & re‑ordering ----
    inline std::unordered_map<int, int> reassign_normal_numbers(
        json& challenges,
        const std::unordered_map<std::string, int>& rank_index_map,
        const std::unordered_map<std::string, int>& rank_prefix_map)
    {
        std::sort(challenges.begin(), challenges.end(),
            [&](const json& a, const json& b) {
                int idxA = rank_index_map.at(a["rank"].get<std::string>());
                int idxB = rank_index_map.at(b["rank"].get<std::string>());
                if (idxA != idxB) return idxA < idxB;
                return a["rating"].get<double>() < b["rating"].get<double>();
            });

        std::unordered_map<int, int> old_to_new;
        old_to_new.reserve(challenges.size());
        std::unordered_map<std::string, int> counter_per_rank;

        for (auto& c : challenges) {
            std::string rank_str(c["rank"].get<std::string_view>());
            int old_num = c["number"].get<int>();
            int& counter = counter_per_rank[rank_str];
            int rank_prefix = rank_prefix_map.at(rank_str);
            int new_num = rank_prefix * 100 + counter + 1;
            old_to_new[old_num] = new_num;
            c["number"] = new_num;
            ++counter;
        }
        return old_to_new;
    }

    inline std::unordered_map<int, int> reassign_bonus_numbers(json& challenges)
    {
        std::sort(challenges.begin(), challenges.end(),
            [](const json& a, const json& b) { return a["rating"].get<double>() < b["rating"].get<double>(); });
        std::unordered_map<int, int> old_to_new;
        old_to_new.reserve(challenges.size());
        int idx = 1;
        for (auto& c : challenges) {
            int old_num = c["number"].get<int>();
            c["number"] = idx;
            old_to_new[old_num] = idx;
            ++idx;
        }
        return old_to_new;
    }

    // ---- Player number remapping ----
    inline void remap_player_numbers(json& player, const std::unordered_map<int, int>& mapping, bool is_bonus)
    {
        auto remap_array = [&](json& arr) {
            for (auto& item : arr) {
                auto it = mapping.find(item["number"].get<int>());
                if (it != mapping.end()) item["number"] = it->second;
            }
            std::sort(arr.begin(), arr.end(),
                [](const json& a, const json& b) { return a["number"].get<int>() < b["number"].get<int>(); });
            };

        if (is_bonus) {
            if (player.contains("completedBonus")) remap_array(player["completedBonus"]);
            if (player.contains("bonusWaitingToBeRated")) remap_array(player["bonusWaitingToBeRated"]);
        }
        else {
            if (player.contains("completedChallenges")) remap_array(player["completedChallenges"]);
            if (player.contains("challengesWaitingToBeRated")) remap_array(player["challengesWaitingToBeRated"]);
        }
    }
}