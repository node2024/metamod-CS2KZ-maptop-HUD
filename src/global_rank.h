#pragma once
#include "model.h"

namespace maptop {
enum class GlobalRankState { Hidden, Loading, Ready, Missing, Unavailable };
struct GlobalRank {
    GlobalRankState state = GlobalRankState::Hidden;
    int rank = 0, teleports = 0;
    double time = 0;
};
inline bool SameName(std::string a, std::string b) {
    for (auto& c : a) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    for (auto& c : b) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return a == b;
}
inline std::string GlobalPBURL(const std::string& api, const Filter& f, uint64_t steam) {
    return api + "/records?top=true&player=" + std::to_string(steam)
        + "&map=" + Encode(f.map) + "&course=" + Encode(f.course) + "&mode=" + f.mode
        + (f.pro ? "&has_teleports=false" : "") + "&sort_by=time&sort_order=ascending&limit=1";
}
inline GlobalRank ParseGlobalRank(const std::string& body, const Filter& f, uint64_t steam) {
    const auto data = Json::parse(body);
    const auto& values = data.at("values");
    if (!values.is_array() || values.size() > 1) throw std::runtime_error("Invalid global PB response");
    if (values.empty()) return {GlobalRankState::Missing};
    const auto& record = values.front();
    if (SteamID(record.at("player").at("id").get<std::string>()) != steam
        || !SameName(record.at("map").at("name").get<std::string>(), f.map)
        || !SameName(record.at("course").at("name").get<std::string>(), f.course)
        || record.at("mode").get<std::string>() != f.mode
        || !record.at("styles").is_array() || !record.at("styles").empty()
        || !UUID(record.at("id").get<std::string>())) throw std::runtime_error("Global PB does not match the selected run");
    GlobalRank result;
    result.time = record.at("time").get<double>();
    result.teleports = record.at("teleports").get<int>();
    if (!std::isfinite(result.time) || result.time <= 0 || result.teleports < 0 || (f.pro && result.teleports))
        throw std::runtime_error("Invalid global PB time");
    const auto rank = record.find(f.pro ? "pro_rank" : "nub_rank");
    if (rank == record.end() || rank->is_null()) return {GlobalRankState::Missing};
    if (!rank->is_number_integer() || rank->get<double>() < 1 || rank->get<double>() > INT32_MAX)
        throw std::runtime_error("Invalid global PB rank");
    result.rank = rank->get<int>(); result.state = GlobalRankState::Ready;
    // A player-filtered result's total is 0 or 1, not the leaderboard's player count.
    return result;
}
inline std::vector<std::string> GlobalRankLines(const GlobalRank& rank, const Record& local, const Filter& filter) {
    if (filter.global || rank.state == GlobalRankState::Hidden) return {};
    if (rank.state == GlobalRankState::Loading) return {"Global PB: Loading..."};
    if (rank.state == GlobalRankState::Missing) return {"Global PB: No ranked record"};
    if (rank.state == GlobalRankState::Unavailable) return {"Global PB: Unavailable"};
    std::vector<std::string> lines = {std::string("Global PB ") + (filter.pro ? "Pro" : "Overall") + " #" + std::to_string(rank.rank)};
    if (RankingTime(rank.time) != RankingTime(local.time) || rank.teleports != local.teleports)
        lines.push_back("PB: " + RankingTime(rank.time) + " | TP " + std::to_string(rank.teleports));
    return lines;
}
} // namespace maptop
