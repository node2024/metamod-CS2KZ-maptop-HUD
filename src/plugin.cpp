#include <ISmmPlugin.h>
#include <engine2/isource2server.h>
#include <engine2/ivengineserver2.h>
#include <engine2/inetworkserverservice.h>
#include <engine2/networkgameserverbase.h>
#include <icvar.h>
#include <filesystem/ifilesystem.h>
#include <ics2menus.h>
#include <ics2kz.h>
#include "data.h"
#include "hud_click.h"
#include <set>

PLUGIN_GLOBALVARS();
namespace {
ICvar* g_engineCvar = nullptr;
IVEngineServer* engine = nullptr;
IServerGameDLL* server = nullptr;
IServerGameClients* clients = nullptr;
INetworkServerService* network = nullptr;
IFileSystem* filesystem = nullptr;
using namespace maptop;

bool Equal(const char* a, const char* b) { return a && !V_stricmp(a, b); }
bool Alias(const char* name) { return Equal(name, "mtop") || Equal(name, "maptop_hud"); }
bool ChatCommand(const CCommand& args, bool shared) {
    if (!Equal(args.Arg(0), "say") && !Equal(args.Arg(0), "say_team")) return false;
    CCommand words;
    words.Tokenize(args.Arg(1));
    if (words.ArgC() != 1) return false;
    const char* text = words.Arg(0);
    if (*text != '!' && *text != '/') return false;
    return shared ? Equal(text + 1, "maptop") : Alias(text + 1);
}

struct RunView {
    Record row;
    Filter filter;
    GlobalRank global;
    Ticket ticket;
    uint64_t revision = 0;
    bool actions = false;
};
struct Session {
    uint64_t steam = 0, generation = 0;
    MenuHandle menu = 0;
    Filter filter;
    std::optional<Page> board;
    std::shared_ptr<RunView> details;
    std::vector<std::string> courses;
    std::string loadingMessage;
    std::chrono::steady_clock::time_point lastOpen{};
};

class MapTopHUD final : public ISmmPlugin, public IMetamodListener {
    friend struct MapTopHUDUITest;
    ICS2Menus* menus_ = nullptr;
    ICS2KZ* kz_ = nullptr;
    ISQLInterface* sql_ = nullptr;
    Data data_;
    std::array<Session, 64> sessions_;
    std::set<MenuHandle> live_;
    std::string map_;
    bool unloading_ = false;
    KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext&, const CCommand&> dispatch_{
        &ICvar::DispatchConCommand, this, &MapTopHUD::DispatchPre, &MapTopHUD::DispatchPost};
    KHook::Virtual<IServerGameClients, void, CPlayerSlot, const CCommand&> command_{
        &IServerGameClients::ClientCommand, this, &MapTopHUD::CommandPre, &MapTopHUD::CommandPost};
    KHook::Virtual<IServerGameClients, void, CPlayerSlot, int, uint32, const void*> hudClick_{
        &IServerGameClients::ClientSvcUserMessage, this, nullptr, &MapTopHUD::HudClickPost};
    KHook::Virtual<IServerGameDLL, void, bool, bool, bool> frame_{
        &IServerGameDLL::GameFrame, this, nullptr, &MapTopHUD::Frame};
    KHook::Virtual<IServerGameClients, void, CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char*> disconnect_{
        &IServerGameClients::ClientDisconnect, this, &MapTopHUD::Disconnect, nullptr};

    uint64_t Steam(int slot) const { return kz_ && slot >= 0 && slot < 64 && kz_->IsValidPlayer(slot) ? kz_->GetSteamID64(slot) : 0; }
    Ticket Current(int slot) const { const auto& s = sessions_[slot]; return {s.steam, s.generation, map_}; }
    bool Valid(int slot, const Ticket& ticket) const {
        return !unloading_ && menus_ && slot >= 0 && slot < 64
            && ticket.Matches(Steam(slot), sessions_[slot].generation, map_) && sessions_[slot].steam == ticket.steam;
    }
    void Clear(int slot) {
        auto& s = sessions_[slot];
        ++s.generation;
        auto menu = s.menu; s.menu = 0; s.steam = 0; s.courses.clear(); s.board.reset();
        s.loadingMessage.clear(); s.details.reset();
        if (menus_ && menu && menus_->GetActiveMenu(slot) == menu) menus_->CancelMenu(slot);
    }
    void ClearAll() { for (int slot = 0; slot < 64; ++slot) Clear(slot); }
    void Refresh() {
        auto* menus = static_cast<ICS2Menus*>(g_SMAPI->MetaFactory(CS2MENUS_INTERFACE, nullptr, nullptr));
        auto* kz = static_cast<ICS2KZ*>(g_SMAPI->MetaFactory(CS2KZ_INTERFACE, nullptr, nullptr));
        auto* sql = static_cast<ISQLInterface*>(g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr));
        if (menus != menus_) {
            // OnPluginUnload runs after the provider has gone. Never call its cached interface.
            menus_ = nullptr; live_.clear(); ClearAll(); menus_ = menus;
        }
        if (kz != kz_) { kz_ = kz; ClearAll(); }
        sql_ = sql; data_.RefreshSQL(sql);
    }
    void ErrorConsole(int slot, const std::string& text) {
        if (slot >= 0 && slot < 64) engine->ClientPrintf(CPlayerSlot(slot), ("[MapTop HUD] " + text + "\n").c_str());
        META_CONPRINTF("[MapTop HUD] %s\n", text.c_str());
    }
    MenuHandle Create(const std::string& title, MenuItemSelectFn callback = nullptr) {
        auto menu = menus_->CreateMenu(MenuType::Panorama, title.c_str(), std::move(callback));
        if (!menu) return 0;
        live_.insert(menu);
        menus_->SetCloseOnSelect(menu, false);
        menus_->SetMenuForceType(menu, true);
        menus_->SetMenuStyle(menu, MenuStyle::TitleColor, "#66d9c6");
        menus_->SetMenuKey(menu, MenuNavAction::Select, MenuButton::Use);
        menus_->SetMenuTextFeatures(menu, kMenuTextIndex);
        menus_->SetMenuEndCallback(menu, [this](MenuHandle ended, int slot, MenuEndReason) {
            live_.erase(ended);
            if (slot >= 0 && slot < 64 && sessions_[slot].menu == ended) { sessions_[slot].menu = 0; ++sessions_[slot].generation; }
            if (menus_) menus_->DestroyMenu(ended);
        });
        return menu;
    }
    bool Present(int slot, MenuHandle menu) {
        if (!menu) return false;
        sessions_[slot].menu = menu; // The old menu's end callback must not invalidate the new request.
        if (menus_->DisplayMenu(menu, slot, static_cast<float>(data_.menuSeconds))) return true;
        sessions_[slot].menu = 0; ++sessions_[slot].generation;
        live_.erase(menu); menus_->DestroyMenu(menu);
        ErrorConsole(slot, "Another menu currently owns the display. Close it and try !mtop.");
        return false;
    }
    std::string Title(int slot) const {
        const auto& f = sessions_[slot].filter;
        return "MAP TOP | " + Text(f.map) + " | " + (f.global ? "GLOBAL" : "LOCAL")
            + " | " + (f.mode == "classic" ? "CKZ" : "VNL") + " | " + (f.pro ? "PRO" : "OVERALL");
    }
    void Loading(int slot, const std::string& message) {
        sessions_[slot].loadingMessage = message;
        const auto ticket = Current(slot);
        auto menu = Create(Title(slot), [this, ticket](MenuHandle h, int s, int item) {
            if (!Valid(s, ticket)) return;
            const std::string info = menus_->GetItemInfo(h, item);
            if (info == "overall") ChangeCategory(s, false);
            else if (info == "pro") ChangeCategory(s, true);
            else if (info == "courses") Courses(s);
            else if (info == "refresh") Reload(s, true);
        });
        menus_->SetMenuLayout(menu, MenuLayout::Studio);
        BoardControls(menu, ticket, sessions_[slot].filter);
        CourseControl(menu, slot);
        const int status = menus_->AddItem(menu, Text(message, 220).c_str(), "", true);
        menus_->SetItemSpan(menu, status, 3);
        const int refresh = menus_->AddItem(menu, "Refresh", "refresh", false);
        menus_->SetItemSpan(menu, refresh, 3);
        Present(slot, menu);
    }
    void Reload(int slot, bool courses = false) {
        auto& s = sessions_[slot];
        s.details.reset();
        ++s.generation;
        s.board.reset();
        if (courses) s.courses.clear();
        auto ticket = Current(slot);
        Loading(slot, "Loading records...");
        if (!Valid(slot, ticket) || !s.menu) return;
        if (courses || s.courses.empty()) {
            data_.Courses(s.filter, [this, slot, ticket](std::vector<std::string> list, const std::string& error) {
                if (!Valid(slot, ticket)) return;
                auto& state = sessions_[slot];
                state.courses = std::move(list);
                if (!error.empty()) { Loading(slot, error); return; }
                if (state.courses.empty()) { Loading(slot, "No courses found for this map."); return; }
                if (std::find(state.courses.begin(), state.courses.end(), state.filter.course) == state.courses.end()) state.filter.course = state.courses.front();
                FetchRecords(slot, ticket);
            });
        } else FetchRecords(slot, ticket);
    }
    void FetchRecords(int slot, const Ticket& ticket) {
        data_.Records(sessions_[slot].filter, [this, slot, ticket](Page page, const std::string& error) {
            if (!Valid(slot, ticket)) return;
            if (!error.empty()) { Loading(slot, error); return; }
            Board(slot, std::move(page));
        });
    }
    void ChangeSource(int slot, bool global) {
        if (sessions_[slot].filter.global == global) return;
        auto& s = sessions_[slot]; s.filter.global = global; s.filter.offset = 0; Reload(slot, true);
    }
    void ChangeCategory(int slot, bool pro) {
        auto& state = sessions_[slot];
        if (state.filter.pro == pro) return;
        state.filter.pro = pro; state.filter.offset = 0;
        Reload(slot);
    }
    void ChangeMode(int slot, const std::string& mode) {
        if ((mode != "classic" && mode != "vanilla") || sessions_[slot].filter.mode == mode) return;
        auto& state = sessions_[slot];
        state.filter.mode = mode; state.filter.offset = 0;
        Reload(slot);
    }
    void BoardControls(MenuHandle menu, Ticket ticket, const Filter& filter) {
        // Native chips provide dropdowns in both Studio and Table without addon changes.
        const char* sources[] = {"Local", "Global"};
        const int source = menus_->AddMenuChip(menu, "Source", sources, 2, filter.global ? 1 : 0);
        const char* modes[] = {"VNL", "CKZ"};
        const char* types[] = {"Overall", "Pro"};
        const int mode = menus_->AddMenuChip(menu, "Mode", modes, 2, filter.mode == "vanilla" ? 0 : 1);
        const int type = menus_->AddMenuChip(menu, "Type", types, 2, filter.pro ? 1 : 0);
        menus_->SetMenuChipCallback(menu, [this, ticket, source, mode, type](MenuHandle, int s, int chip, int selected) {
            if (!Valid(s, ticket) || chip < 0) return;
            if ((chip == source || chip == mode || chip == type) && selected == -1) {
                // Selecting the current stock chip option clears it. These required filters may not be empty.
                // Rebuild without invalidating any record request already in flight.
                if (sessions_[s].board) Board(s, *sessions_[s].board);
                else Loading(s, "Loading records...");
            }
            else if (chip == source && (selected == 0 || selected == 1)) ChangeSource(s, selected == 1);
            else if (chip == mode && (selected == 0 || selected == 1)) ChangeMode(s, selected == 0 ? "vanilla" : "classic");
            else if (chip == type && (selected == 0 || selected == 1)) ChangeCategory(s, selected == 1);
        });
    }
    void BackToBoard(int slot) {
        sessions_[slot].details.reset();
        ++sessions_[slot].generation;
        if (sessions_[slot].board) Board(slot, *sessions_[slot].board);
        else Reload(slot);
    }
    void CourseControl(MenuHandle menu, int slot) {
        const auto& session = sessions_[slot];
        const auto course = Text(session.filter.course, 64);
        const int item = menus_->AddItem(menu, course.empty() ? "Select course" : course.c_str(), "courses", session.courses.empty());
        menus_->SetItemSubtext(menu, item, "Courses ▾");
        menus_->SetItemSpan(menu, item, 3);
    }
    void Board(int slot, Page page) {
        sessions_[slot].details.reset();
        const auto ticket = Current(slot);
        const auto filter = sessions_[slot].filter;
        sessions_[slot].board = page;
        auto menu = Create(Title(slot), [this, ticket, page, filter](MenuHandle h, int s, int item) {
            if (!Valid(s, ticket)) return;
            const std::string info = menus_->GetItemInfo(h, item);
            if (info == "courses") Courses(s);
            else if (info == "prev" && filter.offset > 0) {
                sessions_[s].filter.offset = std::max(0, filter.offset - PageSize); Reload(s);
            }
            else if (info == "next" && page.more) {
                sessions_[s].filter.offset = filter.offset + PageSize; Reload(s);
            }
            else if (info == "overall") ChangeCategory(s, false);
            else if (info == "pro") ChangeCategory(s, true);
            else if (info.rfind("p:", 0) == 0 || info.rfind("r:", 0) == 0) {
                const int index = std::atoi(info.c_str() + 2);
                if (index < 0 || index >= static_cast<int>(page.rows.size())) return;
                if (info[0] == 'r') StartReplay(s, page.rows[index], filter);
                else RunPopup(s, page.rows[index], filter);
            }
        });
        menus_->SetMenuLayout(menu, MenuLayout::Studio);
        menus_->SetMenuTextFeatures(menu, 0);
        BoardControls(menu, ticket, filter);
        CourseControl(menu, slot);
        for (size_t i = 0; i < page.rows.size(); ++i) {
            const auto& row = page.rows[i];
            const auto profile = "p:" + std::to_string(i);
            const auto label = "#" + std::to_string(row.rank) + " " + Text(row.name, 32);
            const int summary = menus_->AddItem(menu, label.c_str(), profile.c_str(), false);
            menus_->SetItemSpan(menu, summary, 2);
            menus_->SetItemSubtext(menu, summary, (RankingTime(row.time) + " | TP " + std::to_string(row.teleports)).c_str());
            menus_->SetItemHighlight(menu, summary, row.steam == sessions_[slot].steam);
            const int replay = menus_->AddItem(menu, "Replay ▶", ("r:" + std::to_string(i)).c_str(), !row.replay);
            if (!row.replay) menus_->SetItemSubtext(menu, replay, "Unavailable");
        }
        if (page.rows.empty()) {
            const int empty = menus_->AddItem(menu, "No records in this category.", "", true);
            menus_->SetItemSpan(menu, empty, 3);
        }
        // Use the same 138px buttons / 3px margins as record rows. Choice segments are 142px with no inner margins.
        const auto currentPage = "Page " + std::to_string(filter.offset / PageSize + 1);
        menus_->AddItem(menu, "<", "prev", filter.offset == 0);
        const int current = menus_->AddItem(menu, currentPage.c_str(), "page", false);
        menus_->SetItemHighlight(menu, current, true);
        menus_->AddItem(menu, ">", "next", !page.more);
        menus_->SetMenuRefreshCallback(menu, [this, ticket](MenuHandle, int s) { if (Valid(s, ticket)) Reload(s); });
        Present(slot, menu);
    }
    void Courses(int slot) {
        const auto courses = sessions_[slot].courses;
        if (courses.empty()) return;
        sessions_[slot].details.reset();
        ++sessions_[slot].generation;
        const auto ticket = Current(slot);
        auto menu = Create("MAP TOP | COURSES", [this, ticket, courses](MenuHandle, int s, int item) {
            if (!Valid(s, ticket) || item < 0 || item >= static_cast<int>(courses.size())) return;
            auto& f = sessions_[s].filter;
            if (f.course == courses[item]) { BackToBoard(s); return; }
            f.course = courses[item];
            f.offset = 0;
            Reload(s);
        });
        for (const auto& course : courses) menus_->AddItem(menu, ((course == sessions_[slot].filter.course ? "> " : "") + Text(course)).c_str(), "", false);
        Present(slot, menu);
    }
    void RunPopup(int slot, Record row, Filter filter) {
        auto details = std::make_shared<RunView>();
        details->row = std::move(row); details->filter = std::move(filter); details->ticket = Current(slot);
        if (!details->filter.global) details->global.state = GlobalRankState::Loading;
        sessions_[slot].details = details;
        ShowRunPopup(slot, details);
        if (!details->filter.global && ValidDetails(slot, details)) {
            data_.PlayerGlobalRank(details->filter, details->row.steam,
                [this, slot, details](GlobalRank rank) { ApplyGlobalRank(slot, details, rank); });
        }
    }
    bool ValidDetails(int slot, const std::shared_ptr<RunView>& details) const {
        return Valid(slot, details->ticket) && sessions_[slot].details == details
            && sessions_[slot].menu && menus_->GetActiveMenu(slot) == sessions_[slot].menu;
    }
    void ApplyGlobalRank(int slot, const std::shared_ptr<RunView>& details, GlobalRank rank) {
        if (!ValidDetails(slot, details)) return;
        details->global = rank;
        if (details->actions) ShowRunActions(slot, details);
        else ShowRunPopup(slot, details);
    }
    void ShowRunPopup(int slot, const std::shared_ptr<RunView>& details) {
        const auto revision = ++details->revision; // The replaced dialog answers its old callback as cancelled.
        const auto body = RunPopupBody(details->row, details->filter, details->global);
        if (!menus_->ShowMenuConfirm(slot, Text(details->row.name).c_str(), body.c_str(), "Back", "Actions", false,
                [this, details, revision](int s, bool selected) {
                    if (!ValidDetails(s, details) || details->revision != revision) return;
                    ++details->revision;
                    if (selected) RunActions(s, details);
                    else sessions_[s].details.reset();
                })) RunActions(slot, details);
    }
    void PrintProfileURL(int slot, const Record& row) {
        const auto url = CS2KZProfileURL(row.steam);
        if (url.empty()) return;
        engine->ClientPrintf(CPlayerSlot(slot), ("[MapTop HUD] CS2KZ profile\n" + url + "\nCopy this URL into your browser.\n").c_str());
        menus_->ShowMenuMessage(slot, "CS2KZ URL printed to your console.", MenuTone::Info, 4.0f);
    }
    void StartReplay(int slot, const Record& row, const Filter& filter) {
        if (!row.replay || !UUID(row.id)) return;
        if (!filter.global && !data_.LocalReplay(row.id)) {
            if (sessions_[slot].board) for (auto& record : sessions_[slot].board->rows) if (record.id == row.id) record.replay = false;
            BackToBoard(slot);
            if (!menus_->ShowMenuMessage(slot, "Replay is no longer available.", MenuTone::Warn, 4.0f)) ErrorConsole(slot, "Replay is no longer available.");
            return;
        }
        const auto id = row.id;
        Clear(slot);
        const char* args[] = {"kz_replay", id.c_str()};
        CCommand command(2, args);
        clients->ClientCommand(CPlayerSlot(slot), command);
    }
    void RunActions(int slot, const std::shared_ptr<RunView>& details) {
        ++sessions_[slot].generation;
        details->ticket = Current(slot); details->actions = true;
        ShowRunActions(slot, details);
    }
    void ShowRunActions(int slot, const std::shared_ptr<RunView>& details) {
        const auto& row = details->row;
        auto menu = Create("RUN DETAILS | " + Text(row.name), [this, details](MenuHandle, int s, int item) {
            if (!ValidDetails(s, details)) return;
            if (item == 0) BackToBoard(s);
            else if (item == 1) PrintProfileURL(s, details->row);
            else if (item == 2) StartReplay(s, details->row, details->filter);
        });
        menus_->AddItem(menu, "Back to rankings", "", false);
        menus_->AddItem(menu, "Show CS2KZ URL (console)", "", CS2KZProfileURL(row.steam).empty());
        menus_->AddItem(menu, "Replay", "", !row.replay);
        for (const auto& line : RunDetailLines(row, details->filter, details->global)) if (!line.empty()) menus_->AddItem(menu, Text(line, 180).c_str(), "", true);
        Present(slot, menu);
    }
    void Open(int slot) {
        const auto steam = Steam(slot);
        if (!menus_ || !kz_ || !steam) { ErrorConsole(slot, "Requires CS2Menus (ICS2Menus008), CS2KZ v0.0.189 and an authenticated in-game player."); return; }
        auto& s = sessions_[slot];
        auto now = std::chrono::steady_clock::now();
        if (now - s.lastOpen < std::chrono::milliseconds(800)) return;
        s.lastOpen = now;
        s.steam = steam; s.filter.map = map_; s.filter.offset = 0;
        KZTimerStatus status{};
        if (kz_->GetTimerStatus(slot, &status)) {
            s.filter.mode = Equal(status.modeShortName, "VNL") ? "vanilla" : "classic";
            s.filter.course = status.onCourse ? status.course.name : "";
        }
        Reload(slot, true);
    }
    KHook::Return<void> DispatchPre(ICvar*, ConCommandRef, const CCommandContext& ctx, const CCommand& args) {
        if (ChatCommand(args, false)) { Open(ctx.GetPlayerSlot().Get()); return {KHook::Action::Supersede}; }
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> DispatchPost(ICvar*, ConCommandRef, const CCommandContext& ctx, const CCommand& args) {
        if (Equal(args.Arg(0), "kz_reload_config") && ctx.GetPlayerSlot().Get() < 0) {
            if (data_.LoadConfig(g_SMAPI->GetBaseDir(), filesystem, false)) {
                ClearAll();
                META_CONPRINTF("[MapTop HUD] Reloaded CS2KZ API and HUD settings. DB settings take effect after server restart.\n");
            } else META_CONPRINTF("[MapTop HUD] CS2KZ config reload failed; keeping previous settings.\n");
        }
        if (ChatCommand(args, true)) Open(ctx.GetPlayerSlot().Get());
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> CommandPre(IServerGameClients*, CPlayerSlot slot, const CCommand& args) {
        if (Equal(args.Arg(0), "kz_maptop_hud") || Equal(args.Arg(0), "maptop_hud")) { Open(slot.Get()); return {KHook::Action::Supersede}; }
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> CommandPost(IServerGameClients*, CPlayerSlot slot, const CCommand& args) {
        if (Equal(args.Arg(0), "kz_maptop") && args.ArgC() == 1) Open(slot.Get());
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> HudClickPost(IServerGameClients*, CPlayerSlot player, int type, uint32 size, const void* buffer) {
        const int slot = player.Get();
        if (unloading_ || !menus_ || slot < 0 || slot >= 64 || !StudioBackgroundClick(type, buffer, size)) return {KHook::Action::Ignore};
        const auto& state = sessions_[slot];
        if (!state.menu || !live_.count(state.menu) || menus_->GetActiveMenu(slot) != state.menu
            || !Valid(slot, Current(slot)) || menus_->GetMenuLayout(state.menu) != MenuLayout::Studio) return {KHook::Action::Ignore};
        // CS2Menus handles cs_stage in its pre-hook and gives the cursor to the game.
        // Re-display only our own menu afterwards. Keep the generation so in-flight data remains valid.
        if (state.board) Board(slot, *state.board);
        else {
            const auto message = state.loadingMessage.empty() ? std::string("Loading records...") : state.loadingMessage;
            Loading(slot, message);
        }
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> Disconnect(IServerGameClients*, CPlayerSlot slot, ENetworkDisconnectionReason, const char*, uint64, const char*) {
        if (slot.Get() >= 0 && slot.Get() < 64) Clear(slot.Get());
        return {KHook::Action::Ignore};
    }
    KHook::Return<void> Frame(IServerGameDLL*, bool, bool, bool) {
        auto* game = network->GetIGameServer();
        std::string map = game && game->GetMapName() ? game->GetMapName() : "";
        if (map != map_) { ClearAll(); map_ = map; }
        for (int slot = 0; slot < 64; ++slot) if (sessions_[slot].steam && Steam(slot) != sessions_[slot].steam) Clear(slot);
        data_.Frame();
        return {KHook::Action::Ignore};
    }
public:
    bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool) override {
        PLUGIN_SAVEVARS();
        GET_V_IFACE_CURRENT(GetEngineFactory, g_engineCvar, ICvar, CVAR_INTERFACE_VERSION);
        // SDK CCommand::Tokenize() resolves its default character set through this global.
        // Bind it before installing any chat/command hooks, including on a late plugin load.
        ::g_pCVar = g_engineCvar;
        GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
        GET_V_IFACE_ANY(GetServerFactory, server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
        GET_V_IFACE_ANY(GetServerFactory, clients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
        GET_V_IFACE_ANY(GetEngineFactory, network, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
        GET_V_IFACE_CURRENT(GetFileSystemFactory, filesystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
        data_.LoadConfig(g_SMAPI->GetBaseDir(), filesystem);
        g_SMAPI->AddListener(this, this);
        dispatch_.Add(g_engineCvar); command_.Add(clients); hudClick_.Add(clients); frame_.Add(server); disconnect_.Add(clients);
        Refresh();
        META_CONPRINTF("[MapTop HUD] Loaded 0.1.13. Use !maptop or !mtop. Config: %s\n", data_.configError.empty() ? "OK" : data_.configError.c_str());
        return true;
    }
    void AllPluginsLoaded() override { Refresh(); }
    void OnPluginLoad(PluginId) override { Refresh(); }
    void OnPluginUnload(PluginId) override { Refresh(); }
    bool Unload(char*, size_t) override {
        unloading_ = true;
        // meta clear does not notify consumers; re-resolve before touching a cached provider.
        auto* current = static_cast<ICS2Menus*>(g_SMAPI->MetaFactory(CS2MENUS_INTERFACE, nullptr, nullptr));
        if (current != menus_) { menus_ = nullptr; live_.clear(); }
        ClearAll();
        if (menus_) for (auto handle : std::set<MenuHandle>(live_)) menus_->DestroyMenu(handle);
        live_.clear();
        data_.Shutdown(sql_ && g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr) == sql_);
        disconnect_.Remove(clients); frame_.Remove(server); hudClick_.Remove(clients); command_.Remove(clients); dispatch_.Remove(g_engineCvar);
        return true;
    }
    const char* GetAuthor() override { return "KZ MapTop HUD contributors"; }
    const char* GetName() override { return "KZ MapTop HUD"; }
    const char* GetDescription() override { return "Local/global CS2KZ leaderboard and replay/profile menus via CS2Menus"; }
    const char* GetURL() override { return "https://github.com/FemboyKZ/mm-cs2menus"; }
    const char* GetLicense() override { return "AGPLv3"; }
    const char* GetVersion() override { return "0.1.13"; }
    const char* GetDate() override { return __DATE__; }
    const char* GetLogTag() override { return "KZ-MAPTOP"; }
};
MapTopHUD plugin;
} // namespace
PLUGIN_EXPOSE(MapTopHUD, plugin);
