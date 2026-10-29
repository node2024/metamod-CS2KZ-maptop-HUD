#pragma once
#include "model.h"
#include "hud_settings.h"
#include "config.h"
#include "run_details.h"
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <steam/steam_gameserver.h>
#include <steam/steam_api_flat.h>
#include <sql_mm.h>
#include <mysql_mm.h>
#include <sqlite_mm.h>

namespace maptop {
class Data {
public:
    using HttpCallback = std::function<void(std::string, std::string)>;
    using CoursesCallback = std::function<void(std::vector<std::string>, std::string)>;
    using PageCallback = std::function<void(Page, std::string)>;
    using RankCallback = std::function<void(GlobalRank)>;
    std::string base, api = "https://api.cs2kz.org", configError;
    int menuSeconds = 120;
    DisplayMode displayMode = DisplayMode::Studio;

    bool LoadConfig(const std::string& baseDir, IFileSystem* filesystem, bool initial = true) {
        base = baseDir;
        try {
            KeyValues root("ServerConfig");
            if (!root.LoadFromFile(filesystem, (base + "/cfg/cs2kz-server-config.txt").c_str(), nullptr))
                throw std::runtime_error("Cannot load cfg/cs2kz-server-config.txt");
            auto candidate = ReadConnections(root);
            // CS2KZ's kz_reload_config reconnects the API, but its DB settings take effect only on restart.
            if (initial || !initialized_) connections_ = candidate;
            connections_.api = candidate.api;
            api = candidate.api;
            initialized_ = true;
            configError.clear();
        } catch (const std::exception&) {
            if (!initialized_) configError = "Cannot read CS2KZ connection settings from cfg/cs2kz-server-config.txt";
            return false; // Preserve a working configuration on a malformed reload, as CS2KZ does.
        }
        try {
            std::ifstream file(base + "/addons/kz_maptop_hud/config.json");
            if (file) {
                const auto settings = ReadHudSettings(Json::parse(file));
                menuSeconds = settings.menuSeconds;
                displayMode = settings.displayMode;
            }
        } catch (const std::exception&) { /* UI settings must not override or disable CS2KZ's connections. */ }
        return true;
    }

    void RefreshSQL(ISQLInterface* sql) {
        if (sql != sql_) { connection_ = nullptr; localReady_ = false; failed_ = false; sql_ = sql; }
        if (!sql_ || connection_ || !configError.empty()) return;
        nextConnect_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        try {
            if (connections_.driver == "sqlite") {
                // SQLMM opens SQLite with CREATE; reject a wrong path instead of creating an empty database.
                if (!std::filesystem::is_regular_file(std::filesystem::path(base) / connections_.database)) return;
                connection_ = sql_->GetSQLiteClient()->CreateSQLiteConnection({connections_.database.c_str()});
            } else if (connections_.driver == "mysql") {
                connection_ = sql_->GetMySQLClient()->CreateMySQLConnection(
                    {connections_.host.c_str(), connections_.user.c_str(), connections_.password.c_str(), connections_.database.c_str(), connections_.port, connections_.timeout});
            }
            if (connection_) connection_->Connect([this](bool ok) {
                localReady_ = ok; failed_ = !ok;
                if (!ok) nextConnect_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            });
        } catch (const std::exception&) { localReady_ = false; }
    }

    bool LocalReady() const { return localReady_; }
    void Shutdown(bool sqlStillLoaded) {
        if (connection_ && sqlStillLoaded) connection_->Destroy();
        connection_ = nullptr; sql_ = nullptr; localReady_ = false;
        auto* http = SteamAPI_SteamGameServerHTTP();
        if (http) for (const auto& request : requests_) http->ReleaseHTTPRequest(request->request);
        requests_.clear(); ready_.clear();
    }

    void Get(const std::string& url, HttpCallback callback) {
        auto* http = SteamAPI_SteamGameServerHTTP();
        if (!http || requests_.size() >= 32 || !configError.empty() || api.empty()) {
            ready_.push_back([callback] { callback({}, "Global API unavailable or busy. Try again shortly."); }); return;
        }
        auto request = std::make_unique<Http>();
        request->request = http->CreateHTTPRequest(k_EHTTPMethodGET, url.c_str());
        request->callback = std::move(callback);
        request->started = std::chrono::steady_clock::now();
        http->SetHTTPRequestAbsoluteTimeoutMS(request->request, 8000);
        http->SetHTTPRequestRequiresVerifiedCertificate(request->request, true);
        http->SetHTTPRequestHeaderValue(request->request, "Accept", "application/json");
        http->SetHTTPRequestUserAgentInfo(request->request, "KZMapTopHUD/0.1.15");
        if (!request->request || !http->SendHTTPRequest(request->request, &request->call)) {
            if (request->request) http->ReleaseHTTPRequest(request->request);
            auto fn = std::move(request->callback);
            ready_.push_back([fn] { fn({}, "Could not send global API request."); }); return;
        }
        requests_.push_back(std::move(request));
    }

    void Frame() {
        if (failed_ && connection_) { connection_->Destroy(); connection_ = nullptr; failed_ = false; }
        if (!connection_ && sql_ && std::chrono::steady_clock::now() >= nextConnect_) RefreshSQL(sql_);
        auto* http = SteamAPI_SteamGameServerHTTP();
        auto* utils = SteamGameServerUtils();
        for (size_t i = 0; i < requests_.size();) {
            auto& request = requests_[i];
            bool failed = false;
            bool expired = std::chrono::steady_clock::now() - request->started > std::chrono::seconds(12);
            if (!expired && (!http || !utils || !utils->IsAPICallCompleted(request->call, &failed))) { ++i; continue; }
            std::string body, error;
            HTTPRequestCompleted_t result{};
            if (expired || !http || !utils || failed || !utils->GetAPICallResult(request->call, &result, sizeof(result), HTTPRequestCompleted_t::k_iCallback, &failed)
                || failed || !result.m_bRequestSuccessful) error = "Global API request failed or timed out.";
            else if (result.m_eStatusCode != 200) error = "Global API: HTTP " + std::to_string(static_cast<int>(result.m_eStatusCode));
            else if (result.m_unBodySize > 1024 * 1024) error = "Global API response is too large.";
            else {
                body.resize(result.m_unBodySize);
                if (!http->GetHTTPResponseBodyData(request->request, reinterpret_cast<uint8*>(body.data()), result.m_unBodySize)) error = "Could not read global API response.";
            }
            if (http) http->ReleaseHTTPRequest(request->request);
            auto callback = std::move(request->callback);
            requests_.erase(requests_.begin() + i);
            ready_.push_back([callback, body = std::move(body), error = std::move(error)] { callback(body, error); });
        }
        // Call consumers only here, on the game thread, outside the SQLMM/Steam callback stacks.
        auto ready = std::move(ready_); ready_.clear();
        for (auto& completion : ready) completion();
    }

    void PlayerGlobalRank(const Filter& filter, uint64_t steam, RankCallback callback) {
        Get(GlobalPBURL(api, filter, steam), [filter, steam, callback](const std::string& body, const std::string& error) {
            if (!error.empty()) { callback({GlobalRankState::Unavailable}); return; }
            try { callback(ParseGlobalRank(body, filter, steam)); }
            catch (...) { callback({GlobalRankState::Unavailable}); }
        });
    }

    void Courses(const Filter& filter, CoursesCallback callback) {
        if (filter.global) {
            Get(api + "/maps/" + Encode(filter.map), [callback](const std::string& body, const std::string& error) {
                if (!error.empty()) { callback({}, error); return; }
                try {
                    std::vector<std::string> courses;
                    auto data = Json::parse(body);
                    for (const auto& course : data.at("courses")) courses.push_back(course.at("name").get<std::string>());
                    callback(courses, {});
                } catch (...) { callback({}, "Invalid map response from global API."); }
            });
        } else {
            if (!localReady_) { callback({}, LocalError()); return; }
            std::string query = "SELECT mc.Name FROM MapCourses mc JOIN Maps m ON m.ID=mc.MapID WHERE m.Name='"
                + connection_->Escape(filter.map.c_str()) + "' ORDER BY mc.StageID,mc.ID";
            connection_->ExecuteTransaction({{query}}, [this, callback](std::vector<ISQLQuery*> queries) {
                std::vector<std::string> courses;
                auto* result = queries.empty() ? nullptr : queries[0]->GetResultSet();
                if (result) while (result->FetchRow()) courses.emplace_back(result->GetString(0));
                ready_.push_back([callback, courses] { callback(courses, {}); });
            }, [this, callback](std::string, int) {
                ready_.push_back([callback] { callback({}, "Local database query failed. Check cfg/cs2kz-server-config.txt."); });
            });
        }
    }

    void Records(const Filter& filter, PageCallback callback) {
        if (filter.global) {
            Get(RecordsURL(api, filter), [callback, filter](const std::string& body, const std::string& error) {
                if (!error.empty()) { callback({}, error); return; }
                try { callback(ParseRecords(body, filter), {}); }
                catch (...) { callback({}, "Invalid record response from global API."); }
            });
        } else {
            if (!localReady_) { callback({}, LocalError()); return; }
            const auto query = RecordsSQL(filter, [this](const std::string& s) { return connection_->Escape(s.c_str()); });
            const auto count = RankingCountSQL(filter, [this](const std::string& s) { return connection_->Escape(s.c_str()); });
            connection_->ExecuteTransaction({{query, count}}, [this, callback, filter](std::vector<ISQLQuery*> queries) {
                Page page;
                auto* total = queries.size() > 1 ? queries[1]->GetResultSet() : nullptr;
                const uint64_t rankedPlayers = total && total->FetchRow() ? static_cast<uint64_t>(std::max<int64_t>(0, total->GetInt64(0))) : 0;
                auto* result = queries.empty() ? nullptr : queries[0]->GetResultSet();
                if (result) while (result->FetchRow()) {
                    Record row;
                    row.id = result->GetString(0); row.steam = static_cast<uint64_t>(result->GetInt64(1));
                    row.name = Text(result->GetString(2));
                    // SQLMM's GetFloat narrows to float. Read decimal text to retain millisecond precision.
                    try { row.time = std::stod(result->GetString(3)); } catch (...) { continue; }
                    row.teleports = result->GetInt(4); row.rank = filter.offset + static_cast<int>(page.rows.size()) + 1;
                    row.rankedPlayers = rankedPlayers;
                    if (!result->IsNull(5)) row.courseStats = CourseStats(result->GetString(5), row.time);
                    if (!result->IsNull(6)) row.created = Text(result->GetString(6), 32);
                    row.replay = LocalReplay(row.id);
                    if (!row.steam || row.time <= 0 || !std::isfinite(row.time)) continue;
                    page.rows.push_back(std::move(row));
                }
                page.more = page.rows.size() > static_cast<size_t>(RecordsPerPage(filter));
                if (page.more) page.rows.resize(RecordsPerPage(filter));
                ready_.push_back([callback, page] { callback(page, {}); });
            }, [this, callback](std::string, int) {
                ready_.push_back([callback] { callback({}, "Local database query failed. Check the CS2KZ v0.0.189 schema and cfg."); });
            });
        }
    }

    bool LocalReplay(const std::string& id) const {
        if (!UUID(id)) return false;
        std::error_code error;
        return std::filesystem::is_regular_file(base + "/kzreplays/" + id + ".replay", error)
            || std::filesystem::is_regular_file(base + "/kzreplays/downloads/" + id + ".replay", error);
    }

private:
    struct Http {
        HTTPRequestHandle request = 0;
        SteamAPICall_t call = 0;
        HttpCallback callback;
        std::chrono::steady_clock::time_point started;
    };
    ISQLInterface* sql_ = nullptr;
    Connections connections_;
    bool initialized_ = false;
    ISQLConnection* connection_ = nullptr;
    bool localReady_ = false;
    bool failed_ = false;
    std::chrono::steady_clock::time_point nextConnect_{};
    std::vector<std::unique_ptr<Http>> requests_;
    std::deque<std::function<void()>> ready_;
    std::string LocalError() const {
        return configError.empty() ? "Local DB unavailable. Check SQLMM and cfg/cs2kz-server-config.txt." : "Invalid config: " + configError;
    }
};
} // namespace maptop
