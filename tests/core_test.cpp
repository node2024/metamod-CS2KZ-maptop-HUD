#include "model.h"
#include "hud_click.h"
#include "run_details.h"
#include "hud_settings.h"
#include <cassert>
#include <iostream>
#include <fstream>
using namespace maptop;

std::string Escape(const std::string& s) {
    std::string out; for (char c : s) { out += c; if (c == '\'') out += c; } return out;
}
int main(int argc, char** argv) {
    Filter filter; filter.map = "kz_test"; filter.course = "word's backyard";
    if (argc == 8 && std::string(argv[1]) == "--global-rank") {
        std::ifstream file(argv[2]);
        const std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        filter.pro = std::string(argv[4]) == "pro"; filter.map = argv[5]; filter.course = argv[6]; filter.mode = argv[7];
        const auto rank = ParseGlobalRank(body, filter, SteamID(argv[3]));
        std::cout << "Global PB rank=" << rank.rank << ", time=" << RankingTime(rank.time) << ", TP=" << rank.teleports << "\n";
        return rank.state == GlobalRankState::Ready ? 0 : 1;
    }
    if (argc > 2 && std::string(argv[1]) == "--parse") {
        std::ifstream file(argv[2]);
        std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        filter.global = true; filter.pro = argc > 3 && std::string(argv[3]) == "pro";
        auto page = ParseRecords(body, filter);
        std::cout << "REST records parsed: " << page.rows.size() << ", more=" << page.more << "\n";
        return 0;
    }
    if (argc > 1 && (std::string(argv[1]) == "--sql" || std::string(argv[1]) == "--count-sql")) {
        filter.pro = argc > 2 && std::string(argv[2]) == "pro";
        if (argc > 3) filter.offset = std::stoi(argv[3]);
        if (argc > 4) filter.mode = argv[4];
        if (argc > 5) filter.pageSize = std::stoi(argv[5]);
        std::cout << (std::string(argv[1]) == "--sql" ? RecordsSQL(filter, Escape) : RankingCountSQL(filter, Escape)); return 0;
    }
    const std::string background = "\x08\x01\x12\x08" "cs_stage";
    auto backgroundClick = [](const std::string& packet, int type = 390) {
        return StudioBackgroundClick(type, packet.data(), static_cast<uint32_t>(packet.size()));
    };
    assert(backgroundClick(background));
    assert(backgroundClick("\x12\x08" "cs_stage" "\x08\x01")); // protobuf fields may arrive in either order
    assert(backgroundClick(background + "\x18\x05")); // ignore an unknown field
    assert(backgroundClick(background + std::string("\x19", 1) + std::string(8, '\0')));
    assert(!backgroundClick(background, 389));
    auto other = background; other.replace(4, 8, "cs_item0");
    assert(!backgroundClick(other));
    assert(!backgroundClick(std::string("\x08\x00\x12\x08", 4) + "cs_stage"));
    assert(!backgroundClick(background.substr(0, background.size() - 1)));
    assert(!backgroundClick(std::string("\x08", 1) + std::string(10, '\xff')));
    assert(!backgroundClick("\x08\x80\x80\x80\x80\x10\x12\x08" "cs_stage"));
    assert(!backgroundClick("\x08\x01\x12\xff\x7f"));
    assert(!backgroundClick(std::string(1, '\0') + background));
    assert(!backgroundClick(std::string(257, 'x')));
    assert(!StudioBackgroundClick(390, nullptr, 12) && !backgroundClick(""));
    assert(Time(59.9997) == "1:00.000");
    assert(Time(3600.125) == "1:00:00.125");
    assert(Time(-1) == "--:--.---");
    assert(RankingTime(314.453125) == "05:14.453");
    assert(RankingTime(624.8125) == "10:24.813");
    assert(RankingTime(3600.125) == "1:00:00.125");
    assert(SteamID("STEAM_1:0:182656610") == SteamBase + 365313220);
    assert(SteamID(std::to_string(SteamBase + 365313220)) == SteamBase + 365313220);
    assert(SteamID("STEAM_1:2:20") == 0);
    assert(SteamID("STEAM_1:0:99999999999999999999") == 0);
    assert(SteamID("76561190000000000") == 0);
    assert(CS2KZProfileURL(SteamBase + 365313220) == "https://cs2kz.org/profile/76561198325578948");
    assert(CS2KZProfileURL(0).empty() && CS2KZProfileURL(UINT64_MAX).empty());
    const std::string uuid = "019b618d-60f0-75c3-9e82-82ed4c19954a";
    assert(UUID(uuid)); assert(!UUID(uuid + ";quit")); assert(!UUID("1234"));
    assert(Text("\x01name\n") == "name");
    assert(Text("日本語", 5) == "日");
    auto url = RecordsURL("https://api.cs2kz.org", filter);
    assert(url.find("word%27s%20backyard") != std::string::npos);
    assert(url.find("top=true") != std::string::npos);
    assert(url.find("limit=6&offset=0") != std::string::npos);
    filter.mode = "vanilla";
    assert(RecordsURL("", filter).find("mode=vanilla") != std::string::npos);
    assert(RecordsSQL(filter, Escape).find("mo.ShortName='VNL'") != std::string::npos);
    filter.mode = "classic";
    assert(RecordsSQL(filter, Escape).find("mo.ShortName='CKZ'") != std::string::npos);
    filter.pro = true;
    assert(RecordsURL("", filter).find("has_teleports=false") != std::string::npos);
    Ticket ticket{SteamBase + 2, 9, "kz_test"};
    assert(ticket.Matches(SteamBase + 2, 9, "kz_test"));
    assert(!ticket.Matches(SteamBase + 4, 9, "kz_test")); // slot reused
    assert(!ticket.Matches(SteamBase + 2, 10, "kz_test")); // menu closed/replaced
    assert(!ticket.Matches(SteamBase + 2, 9, "kz_other")); // map changed
    Json response = {{"total", PageSize + 1}, {"values", Json::array()}};
    for (int i = 0; i < PageSize + 1; ++i) response["values"].push_back({
        {"id", uuid}, {"player", {{"id", "STEAM_1:0:182656610"}, {"name", "Player"}}},
        {"time", 65.125 + i}, {"teleports", 0}, {"pro_rank", i + 1}, {"pro_points", 1000},
        {"server", {{"name", "Test Server\n"}}}, {"replay_available", i == 0}});
    auto page = ParseRecords(response.dump(), filter);
    assert(page.more && page.rows.size() == PageSize);
    assert(page.rows[0].replay && !page.rows[1].replay);
    assert(page.rows[0].steam == SteamBase + 365313220);
    assert(page.rows[0].rankedPlayers == PageSize + 1 && page.rows[0].server == "Test Server" && page.rows[0].hasPoints);
    filter.global = true;
    const auto globalDetails = RunDetailBody(page.rows[0], filter);
    assert(globalDetails.find("Rank: Global Pro #1/6") != std::string::npos && globalDetails.find("Test Server") != std::string::npos);
    assert(globalDetails.find("Points: 1000.00") != std::string::npos);
    assert(globalDetails.find("Sync / Air Time: unavailable") != std::string::npos);
    const auto globalPopup = RunPopupBody(page.rows[0], filter);
    assert(globalPopup.find("01:05.125   |   0 TP") == 0);
    assert(globalPopup.find("Pro #1 / 6") != std::string::npos);
    assert(globalPopup.find(std::string("SERVER ") + PopupLineBreak + "Test Server") != std::string::npos);
    assert(globalPopup.find("Points: 1000.00") != std::string::npos);
    assert(globalPopup.find('\n') == std::string::npos && globalPopup.find("<br>") == std::string::npos);
    auto local = page.rows[0]; local.server.clear(); local.created = "2026-10-24 12:34:56";
    local.courseStats = CourseStats(R"({"splitZoneTimes":[10.25,-1,50],"cpZoneTimes":[20,40],"stageZoneTimes":[]})", local.time);
    assert(local.courseStats.size() == 2 && local.courseStats[0] == "Splits: 2/3 | 00:50.000");
    assert(local.courseStats[1] == "Checkpoints: 2/2 | 00:40.000");
    filter.global = false;
    auto localDetails = RunDetailBody(local, filter);
    assert(localDetails.find("Recorded: 2026-10-24 12:34:56") != std::string::npos);
    assert(localDetails.find("Not stored in local DB") != std::string::npos && localDetails.find("Points:") == std::string::npos);
    const auto localPopup = RunPopupBody(local, filter);
    assert(localPopup.find("RECORDED") != std::string::npos && localPopup.find("Splits: 2/3") != std::string::npos);
    assert(localPopup.find("SERVER") == std::string::npos && localPopup.find("Sync / Air Time") == std::string::npos);
    auto compact = local; compact.created.clear(); compact.courseStats.clear();
    const auto compactPopup = RunPopupBody(compact, filter);
    assert(compactPopup.find("RECORDED") == std::string::npos && compactPopup.find("STATS") == std::string::npos);
    assert(CourseStats("not json", 10).empty());
    assert(CourseStats(R"({"splitZoneTimes":[11],"cpZoneTimes":["wrong"],"stageZoneTimes":[-2]})", 10).empty());
    assert(CourseStats(std::string(16385, ' '), 10).empty());
    response["values"][0]["pro_points"] = nullptr;
    response["values"][0]["server"] = nullptr;
    response.erase("total");
    const auto sparse = ParseRecords(response.dump(), filter).rows[0];
    assert(!sparse.hasPoints && sparse.server.empty() && !sparse.rankedPlayers);
    assert(RunDetailBody(sparse, filter).find("Rank: Local Pro #1\n") != std::string::npos);
    const auto player = SteamBase + 365313220;
    auto pbURL = GlobalPBURL("https://api.example.test", filter, player);
    assert(pbURL.find("player=76561198325578948") != std::string::npos && pbURL.find("course=word%27s%20backyard") != std::string::npos);
    assert(pbURL.find("has_teleports=false") != std::string::npos && pbURL.find("limit=1") != std::string::npos);
    assert(pbURL.find("offset=") == std::string::npos);
    Json pb = {{"total", 1}, {"values", Json::array({{
        {"id", uuid}, {"player", {{"id", "STEAM_1:0:182656610"}}},
        {"map", {{"name", "KZ_TEST"}}}, {"course", {{"name", filter.course}}},
        {"mode", "classic"}, {"styles", Json::array()}, {"time", 62.5}, {"teleports", 0}, {"nub_rank", 17}, {"pro_rank", 9}
    }})}};
    const auto proRank = ParseGlobalRank(pb.dump(), filter, player);
    assert(proRank.state == GlobalRankState::Ready && proRank.rank == 9 && proRank.time == 62.5);
    const auto rankLines = GlobalRankLines(proRank, sparse, filter);
    assert(rankLines.size() == 2 && rankLines[0] == "Global PB Pro #9" && rankLines[1] == "PB: 01:02.500 | TP 0");
    assert(RunPopupBody(sparse, filter, proRank).find("Global PB Pro #9") != std::string::npos);
    assert(RunPopupBody(sparse, filter, proRank).find("#9 / 1") == std::string::npos); // filtered total is not rank population
    auto sameTime = sparse; sameTime.time = 62.5; sameTime.teleports = 0;
    assert(GlobalRankLines(proRank, sameTime, filter).size() == 1);
    filter.pro = false;
    assert(ParseGlobalRank(pb.dump(), filter, player).rank == 17);
    assert(GlobalPBURL("", filter, player).find("has_teleports") == std::string::npos);
    auto invalidPB = [&](Json value) {
        try { ParseGlobalRank(value.dump(), filter, player); return false; } catch (...) { return true; }
    };
    for (const auto& field : {"map", "course"}) {
        auto wrong = pb; wrong["values"][0][field]["name"] = "wrong"; assert(invalidPB(wrong));
    }
    auto wrong = pb; wrong["values"][0]["player"]["id"] = "STEAM_1:0:1"; assert(invalidPB(wrong));
    wrong = pb; wrong["values"][0]["mode"] = "vanilla"; assert(invalidPB(wrong));
    wrong = pb; wrong["values"][0]["styles"] = Json::array({"auto-bhop"}); assert(invalidPB(wrong));
    wrong = pb; wrong["values"][0]["nub_rank"] = -1; assert(invalidPB(wrong));
    wrong = pb; wrong["values"][0]["nub_rank"] = nullptr;
    assert(ParseGlobalRank(wrong.dump(), filter, player).state == GlobalRankState::Missing);
    assert(ParseGlobalRank(R"({"total":0,"values":[]})", filter, player).state == GlobalRankState::Missing);
    filter.global = true;
    assert(GlobalRankLines(proRank, sparse, filter).empty());
    filter.global = false; filter.pro = true;
    wrong = pb; wrong["values"][0]["teleports"] = 1;
    bool wrongPro = false; try { ParseGlobalRank(wrong.dump(), filter, player); } catch (...) { wrongPro = true; }
    assert(wrongPro);
    response["values"].erase(PageSize);
    assert(!ParseRecords(response.dump(), filter).more); // exactly one full page, no lookahead
    response["values"][0]["teleports"] = 1;
    bool rejected = false; try { ParseRecords(response.dump(), filter); } catch (...) { rejected = true; }
    assert(rejected);
    const auto defaults = ReadHudSettings(Json::object());
    assert(defaults.menuSeconds == 120 && defaults.displayMode == DisplayMode::Studio);
    const auto tableSettings = ReadHudSettings({{"display_mode", "table"}, {"menu_seconds", 300}});
    assert(tableSettings.displayMode == DisplayMode::Table && tableSettings.menuSeconds == 300);
    assert(ReadHudSettings({{"display_mode", "studio"}, {"menu_seconds", 1}}).menuSeconds == 15);
    assert(ReadHudSettings({{"menu_seconds", 999}}).menuSeconds == 600);
    for (const auto& bad : {Json::array(), Json(nullptr), Json{{"display_mode", "grid"}},
                           Json{{"display_mode", true}}, Json{{"menu_seconds", "120"}}}) {
        bool invalid = false;
        try { ReadHudSettings(bad); } catch (const std::exception&) { invalid = true; }
        assert(invalid);
    }
    Filter tableFilter = filter;
    tableFilter.pageSize = TablePageSize; tableFilter.offset = TablePageSize; tableFilter.pro = false;
    assert(RecordsURL("", tableFilter).find("limit=16&offset=15") != std::string::npos);
    assert(RecordsSQL(tableFilter, Escape).find("LIMIT 16 OFFSET 15") != std::string::npos);
    Json tableResponse = {{"values", Json::array()}};
    for (int i = 0; i <= TablePageSize; ++i) tableResponse["values"].push_back({
        {"id", uuid}, {"player", {{"id", "STEAM_1:0:182656610"}, {"name", "Player"}}},
        {"time", 65.125 + i}, {"teleports", i}, {"replay_available", i != 1}});
    const auto tablePage = ParseRecords(tableResponse.dump(), tableFilter);
    assert(tablePage.rows.size() == TablePageSize && tablePage.more);
    assert(tablePage.rows.front().rank == 16 && tablePage.rows.back().rank == 30);
    tableResponse["values"].erase(TablePageSize);
    assert(!ParseRecords(tableResponse.dump(), tableFilter).more);
    auto cells = RunTableCells(tablePage.rows.front());
    assert(RunTableLabel(tablePage.rows.front()) == "#16   01:05.125   Player");
    assert(cells[0] + cells[1] + cells[2] == "0     " && cells[3] == "+" && cells[4] == "OK");
    const auto unavailable = RunTableCells(tablePage.rows[1]);
    assert(unavailable[3] == "+" && unavailable[4] == "No");
    assert(RunTableCells(tablePage.rows.front(), true)[3] == "-");
    auto oversized = tablePage.rows[0]; oversized.time = 360000; oversized.teleports = 1234567;
    cells = RunTableCells(oversized);
    assert(cells[2] == "..");
    assert(RunTableLabel(oversized).find("100:00:00.000") != std::string::npos);
    tableFilter.pageSize = 999; assert(RecordsPerPage(tableFilter) == TablePageSize);
    tableFilter.pageSize = 0; assert(RecordsPerPage(tableFilter) == 1);
    std::cout << "core: OK\n";
}
