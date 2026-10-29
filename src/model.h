#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace maptop {
using Json = nlohmann::json;
// Studio permits 18 items / 24 rendered slots per native page.
// Courses, five records (10 items, each spanning 2+1 columns) and three pager buttons fit together.
constexpr int PageSize = 5;
// Stock Table has 18 rows: one course selector, 15 records and two inline action rows.
// Its pager lives in the header, immediately after Type.
constexpr int TablePageSize = 15;
constexpr uint64_t SteamBase = 76561197960265728ULL;

inline bool UUID(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
        else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F'))) return false;
    }
    return true;
}

inline uint64_t SteamID(const std::string& s) {
    // The REST API returns STEAM_1:Y:Z, while SQL stores SteamID64.
    try {
        if (s.rfind("STEAM_1:", 0) == 0 || s.rfind("STEAM_0:", 0) == 0) {
            if (s.size() < 11 || (s[8] != '0' && s[8] != '1') || s[9] != ':') return 0;
            const auto account = s.substr(10);
            if (account.empty() || account.find_first_not_of("0123456789") != std::string::npos) return 0;
            const auto z = std::stoull(account);
            if (z > 2147483647ULL) return 0;
            return SteamBase + z * 2 + (s[8] - '0');
        }
        if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) return 0;
        auto id = std::stoull(s);
        return id > SteamBase && id <= SteamBase + UINT32_MAX ? id : 0;
    } catch (...) { return 0; }
}

inline std::string Text(const std::string& s, size_t maxBytes = 96) {
    std::string result;
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        size_t length = c < 0x80 ? 1 : c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
        if (!length || i + length > s.size()) { ++i; continue; }
        bool valid = true;
        for (size_t j = 1; j < length; ++j) if ((static_cast<unsigned char>(s[i+j]) & 0xC0) != 0x80) valid = false;
        if (!valid) { ++i; continue; }
        if (length == 1 && (c < 32 || c == 127)) { ++i; continue; }
        if (result.size() + length > maxBytes) break;
        result.append(s, i, length); i += length;
    }
    return result;
}

inline std::string Time(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 8640000) return "--:--.---";
    const auto ms = static_cast<uint64_t>(std::llround(seconds * 1000));
    char out[40];
    if (ms >= 3600000) std::snprintf(out, sizeof(out), "%llu:%02llu:%02llu.%03llu",
        static_cast<unsigned long long>(ms / 3600000), static_cast<unsigned long long>((ms / 60000) % 60),
        static_cast<unsigned long long>((ms / 1000) % 60), static_cast<unsigned long long>(ms % 1000));
    else std::snprintf(out, sizeof(out), "%llu:%02llu.%03llu", static_cast<unsigned long long>(ms / 60000),
        static_cast<unsigned long long>((ms / 1000) % 60), static_cast<unsigned long long>(ms % 1000));
    return out;
}

struct Filter {
    bool global = false, pro = false;
    std::string map, course, mode = "classic";
    int offset = 0;
    int pageSize = PageSize;
};
inline int RecordsPerPage(const Filter& filter) { return std::clamp(filter.pageSize, 1, TablePageSize); }
struct Record {
    std::string id, name, server, created;
    std::vector<std::string> courseStats;
    uint64_t steam = 0, rankedPlayers = 0;
    double time = 0, points = 0;
    int teleports = 0, rank = 0;
    bool replay = false, hasPoints = false;
};
inline std::string RankingTime(double seconds) {
    auto value = Time(seconds);
    if (value.size() == 8 && value[1] == ':') value.insert(value.begin(), '0');
    return value;
}
inline std::string CS2KZProfileURL(uint64_t steam) {
    const auto id = std::to_string(steam);
    return SteamID(id) ? "https://cs2kz.org/profile/" + id : std::string();
}
struct Page {
    std::vector<Record> rows;
    bool more = false;
};

inline std::string Encode(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += static_cast<char>(c);
        else { char hex[4]; std::snprintf(hex, sizeof(hex), "%%%02X", c); out += hex; }
    }
    return out;
}
inline std::string RecordsURL(const std::string& api, const Filter& f) {
    return api + "/records?top=true&map=" + Encode(f.map) + "&course=" + Encode(f.course)
        + "&mode=" + f.mode + (f.pro ? "&has_teleports=false" : "")
        + "&sort_by=time&sort_order=ascending&limit=" + std::to_string(RecordsPerPage(f) + 1) + "&offset=" + std::to_string(f.offset);
}
inline Page ParseRecords(const std::string& body, const Filter& f) {
    auto data = Json::parse(body);
    if (!data.at("values").is_array()) throw std::runtime_error("Invalid API response");
    Page page;
    for (const auto& item : data.at("values")) {
        Record r;
        r.id = item.at("id").get<std::string>();
        r.name = Text(item.at("player").at("name").get<std::string>());
        r.steam = SteamID(item.at("player").at("id").get<std::string>());
        r.time = item.at("time").get<double>();
        r.teleports = item.at("teleports").get<int>();
        if (data.contains("total") && data["total"].is_number_unsigned()) r.rankedPlayers = data["total"].get<uint64_t>();
        const auto server = item.find("server");
        if (server != item.end() && server->is_object() && server->contains("name") && (*server)["name"].is_string())
            r.server = Text((*server)["name"].get<std::string>(), 96);
        auto rank = item.find(f.pro ? "pro_rank" : "nub_rank");
        r.rank = rank != item.end() && rank->is_number_integer() ? rank->get<int>() : f.offset + static_cast<int>(page.rows.size()) + 1;
        auto points = item.find(f.pro ? "pro_points" : "nub_points");
        if (points != item.end() && points->is_number()) {
            r.points = points->get<double>(); r.hasPoints = std::isfinite(r.points) && r.points >= 0;
        }
        r.replay = item.value("replay_available", false) && UUID(r.id);
        if (!r.steam || !UUID(r.id) || !std::isfinite(r.time) || r.time <= 0 || r.teleports < 0 || (f.pro && r.teleports != 0))
            throw std::runtime_error("Invalid record in API response");
        page.rows.push_back(std::move(r));
    }
    page.more = page.rows.size() > static_cast<size_t>(RecordsPerPage(f));
    if (page.more) page.rows.resize(RecordsPerPage(f));
    return page;
}

// SQLMM supplies driver-specific escaping; the SQL itself works on SQLite and MySQL.
inline std::string RankingSourceSQL(const Filter& f, const std::function<std::string(const std::string&)>& escape) {
    return "FROM Times t "
        "JOIN MapCourses mc ON mc.ID=t.MapCourseID JOIN Maps m ON m.ID=mc.MapID "
        "JOIN Modes mo ON mo.ID=t.ModeID JOIN Players p ON p.SteamID64=t.SteamID64 "
        "LEFT JOIN Bans b ON b.SteamID64=t.SteamID64 AND (b.ExpiresAt IS NULL OR b.ExpiresAt>CURRENT_TIMESTAMP) "
        "LEFT JOIN Times t2 ON t2.SteamID64=t.SteamID64 AND t2.MapCourseID=t.MapCourseID "
        "AND t2.ModeID=t.ModeID AND t2.StyleIDFlags=t.StyleIDFlags "
        + std::string(f.pro ? "AND t2.Teleports=0 " : "")
        + "AND (t2.RunTime<t.RunTime OR (t2.RunTime=t.RunTime AND t2.ID<t.ID)) "
        "WHERE t2.ID IS NULL AND b.ID IS NULL AND t.StyleIDFlags=0 "
        + std::string(f.pro ? "AND t.Teleports=0 " : "")
        + "AND m.Name='" + escape(f.map) + "' AND mc.Name='" + escape(f.course)
        + "' AND mo.ShortName='" + (f.mode == "classic" ? "CKZ" : "VNL") + "'";
}
inline std::string RecordsSQL(const Filter& f, const std::function<std::string(const std::string&)>& escape) {
    return "SELECT t.ID,t.SteamID64,p.Alias,t.RunTime,t.Teleports,t.Metadata,t.Created " + RankingSourceSQL(f, escape)
        + " ORDER BY t.RunTime,t.SteamID64,t.ID LIMIT " + std::to_string(RecordsPerPage(f) + 1)
        + " OFFSET " + std::to_string(f.offset);
}
inline std::string RankingCountSQL(const Filter& f, const std::function<std::string(const std::string&)>& escape) {
    return "SELECT COUNT(DISTINCT t.SteamID64) " + RankingSourceSQL(f, escape);
}

// A request belongs to an authenticated player, a map session, and a particular menu generation.
struct Ticket {
    uint64_t steam = 0, generation = 0;
    std::string map;
    bool Matches(uint64_t currentSteam, uint64_t currentGeneration, const std::string& currentMap) const {
        return steam != 0 && steam == currentSteam && generation == currentGeneration && map == currentMap;
    }
};
} // namespace maptop
