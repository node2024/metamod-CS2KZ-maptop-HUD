#pragma once
#include "global_rank.h"

namespace maptop {
// Keep the time in the row's uninterrupted Label instead of two-character numeric cells.
inline std::string RunTableLabel(const Record& row) {
    return "#" + std::to_string(row.rank) + "   " + RankingTime(row.time) + "   " + Text(row.name, 32);
}
inline std::array<std::string, 5> RunTableCells(const Record& row, bool expanded = false) {
    std::array<std::string, 5> cells;
    auto column = [&](std::string text, size_t start, size_t count) {
        const auto width = count * 2;
        if (text.size() > width) text = text.substr(0, width - 2) + "..";
        text.resize(width, ' ');
        for (size_t i = 0; i < count; ++i) cells[start + i] = text.substr(i * 2, 2);
    };
    column(std::to_string(row.teleports), 0, 3);
    cells[3] = expanded ? "-" : "+";
    cells[4] = row.replay ? "OK" : "No";
    return cells;
}

// v0.0.189 stores zone passage times, not whole-run movement statistics.
// Invalid or legacy metadata must never make an otherwise valid ranking unusable.
inline std::vector<std::string> CourseStats(const std::string& metadata, double runtime) {
    std::vector<std::string> lines;
    if (metadata.empty() || metadata.size() > 16384) return lines;
    const auto value = Json::parse(metadata, nullptr, false);
    if (!value.is_object()) return lines;
    const std::array<const char*, 3> keys = {"splitZoneTimes", "cpZoneTimes", "stageZoneTimes"};
    const std::array<const char*, 3> labels = {"Splits", "Checkpoints", "Stages"};
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto found = value.find(keys[i]);
        if (found == value.end() || !found->is_array() || found->empty() || found->size() > 128) continue;
        size_t passed = 0;
        double last = -1;
        bool valid = true;
        for (const auto& time : *found) {
            if (!time.is_number()) { valid = false; break; }
            const double seconds = time.get<double>();
            if (!std::isfinite(seconds) || (seconds != -1 && (seconds < 0 || seconds > runtime))) { valid = false; break; }
            if (seconds >= 0) { ++passed; last = std::max(last, seconds); }
        }
        if (!valid) continue;
        std::string line = std::string(labels[i]) + ": " + std::to_string(passed) + "/" + std::to_string(found->size());
        if (last >= 0) line += " | " + RankingTime(last);
        lines.push_back(std::move(line));
    }
    return lines;
}

inline std::vector<std::string> RunDetailLines(const Record& row, const Filter& filter, const GlobalRank& global = {}) {
    std::string rank = std::string(filter.global ? "Global " : "Local ") + (filter.pro ? "Pro" : "Overall") + " #" + std::to_string(row.rank);
    if (row.rankedPlayers >= static_cast<uint64_t>(std::max(1, row.rank))) rank += "/" + std::to_string(row.rankedPlayers);
    std::vector<std::string> lines = {
        "Player: " + Text(row.name, 64),
        "Runtime: " + RankingTime(row.time) + " (" + std::to_string(row.teleports) + " TPs)",
        "Rank: " + rank,
        std::string("Mode: ") + (filter.mode == "classic" ? "CKZ" : "VNL") + " | " + (filter.global ? "Global" : "Local"),
        "Map: " + Text(filter.map, 64),
        "Course: " + Text(filter.course, 64),
        "",
        "Server",
        row.server.empty() ? (filter.global ? "Not provided by API" : "Not stored in local DB") : Text(row.server, 80),
    };
    const auto globalLines = GlobalRankLines(global, row, filter);
    lines.insert(lines.begin() + 3, globalLines.begin(), globalLines.end());
    if (!row.created.empty()) lines.push_back("Recorded: " + Text(row.created, 32));
    lines.push_back("");
    lines.push_back("More Stats");
    if (filter.global && row.hasPoints) {
        char points[64]; std::snprintf(points, sizeof(points), "Points: %.2f", row.points);
        lines.emplace_back(points);
    }
    lines.insert(lines.end(), row.courseStats.begin(), row.courseStats.end());
    lines.emplace_back(row.replay ? "Replay: Available" : "Replay: Unavailable");
    lines.emplace_back("Sync / Air Time: unavailable");
    return lines;
}
inline std::string RunDetailBody(const Record& row, const Filter& filter) {
    std::string body;
    for (const auto& line : RunDetailLines(row, filter)) { if (!body.empty()) body += '\n'; body += line; }
    return body;
}

// ShowMenuConfirm's StripColors removes bytes 0x00..0x10, including ASCII LF.
// Keep the stock plain-text Label: U+2028 survives that filter without HTML markup.
inline constexpr char PopupLineBreak[] = "\xE2\x80\xA8";
inline std::string RunPopupBody(const Record& row, const Filter& filter, const GlobalRank& global = {}) {
    std::string rank = std::string(filter.global ? "Global " : "Local ") + (filter.pro ? "Pro" : "Overall") + " #" + std::to_string(row.rank);
    if (row.rankedPlayers >= static_cast<uint64_t>(std::max(1, row.rank))) rank += " / " + std::to_string(row.rankedPlayers);
    std::vector<std::string> lines = {
        RankingTime(row.time) + "   |   " + std::to_string(row.teleports) + " TP",
        rank,
        std::string(filter.mode == "classic" ? "CKZ" : "VNL") + "   |   " + (filter.global ? "Global" : "Local"),
        "", "MAP / COURSE", Text(filter.map, 64), Text(filter.course, 64),
    };
    const auto globalLines = GlobalRankLines(global, row, filter);
    lines.insert(lines.begin() + 2, globalLines.begin(), globalLines.end());
    auto section = [&](const char* title, const std::string& value) {
        if (value.empty()) return;
        lines.emplace_back(""); lines.emplace_back(title); lines.push_back(value);
    };
    section("SERVER", Text(row.server, 80));
    section("RECORDED", Text(row.created, 32));
    if ((filter.global && row.hasPoints) || !row.courseStats.empty()) {
        lines.emplace_back(""); lines.emplace_back("STATS");
        if (filter.global && row.hasPoints) {
            char points[64]; std::snprintf(points, sizeof(points), "Points: %.2f", row.points);
            lines.emplace_back(points);
        }
        lines.insert(lines.end(), row.courseStats.begin(), row.courseStats.end());
    }
    lines.emplace_back("");
    lines.emplace_back(row.replay ? "Replay available" : "Replay unavailable");
    std::string body;
    for (const auto& line : lines) {
        // The space also separates words if a client renders the separator as ordinary whitespace.
        if (!body.empty()) { body += ' '; body += PopupLineBreak; }
        body += line;
    }
    return body;
}
} // namespace maptop
