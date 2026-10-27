// Exercise the production block board and its callbacks through the public API's Linux ABI.
// This checks menu construction/routing, not engine rendering or Workshop mounting.
#include "../src/plugin.cpp"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <map>

namespace {
template<class Method> size_t VirtualSlot(Method method) {
    struct MemberPointer { std::ptrdiff_t offset, adjustment; } pointer{};
    static_assert(sizeof(pointer) == sizeof(method));
    std::memcpy(&pointer, &method, sizeof(method));
    assert(pointer.adjustment == 0 && (pointer.offset & 1));
    return static_cast<size_t>(pointer.offset - 1) / sizeof(void*);
}
struct Stub {
    std::array<void*, 256> methods{};
    struct Object { void** vtable; } object{methods.data()};
    template<class Method, class Function> void Set(Method method, Function function) {
        methods.at(VirtualSlot(method)) = reinterpret_cast<void*>(function);
    }
};
struct Item {
    std::string text, info, sub;
    bool disabled = false, highlight = false;
    int span = 1;
    MenuItemRole role = MenuItemRole::Button;
    std::vector<std::string> details;
    std::vector<std::string> options;
};
struct Chip {
    std::string label;
    std::vector<std::string> options;
    int selected = 0;
};
struct Menu {
    std::string title;
    MenuLayout layout = MenuLayout::List;
    std::vector<Item> items;
    std::vector<Chip> chips;
    std::vector<std::string> heads;
    MenuItemSelectFn select;
    MenuChipFn chip;
    MenuEndFn end;
    MenuRefreshFn refresh;
    MenuItemChangeFn change;
    int pinned = -1;
};
std::map<MenuHandle, Menu> menus;
MenuHandle active = 0, nextHandle = 0;
int shownMessages = 0;
bool turning = false;
uint64_t playerSteam = SteamBase + 2;
struct Dialog { std::string title, body, confirm; MenuConfirmFn done; } dialog;
bool dialogAvailable = true;
int shownDialogs = 0;
std::vector<std::string> consoleLines, replayCommands;
MenuHandle CreateMenu(void*, PluginId, MenuType type, const char* title, MenuItemSelectFn select) {
    assert(type == MenuType::Panorama);
    Menu menu; menu.title = title; menu.select = std::move(select);
    const auto handle = ++nextHandle;
    menus.emplace(handle, std::move(menu));
    return handle;
}
void Destroy(void*, MenuHandle h) { menus.erase(h); }
bool Display(void*, MenuHandle h, int slot, float) {
    assert(slot == 0);
    const auto old = active;
    active = h;
    turning = false; // a new DisplayMenu clears Studio's turning state in the stock provider
    if (old && menus.count(old)) {
        const auto callback = menus.at(old).end;
        if (callback) callback(old, slot, MenuEndReason::Cancelled);
    }
    return true;
}
MenuHandle Active(void*, int) { return active; }
void Cancel(void*, int slot) {
    const auto old = active; active = 0;
    if (old && menus.count(old)) { const auto callback = menus.at(old).end; if (callback) callback(old, slot, MenuEndReason::Cancelled); }
}
int AddItem(void*, MenuHandle h, const char* text, const char* info, bool disabled) {
    Item item; item.text = text; item.info = info; item.disabled = disabled;
    menus.at(h).items.push_back(std::move(item));
    return static_cast<int>(menus.at(h).items.size()) - 1;
}
const char* ItemInfo(void*, MenuHandle h, int item) { return menus.at(h).items.at(item).info.c_str(); }
void Layout(void*, MenuHandle h, MenuLayout layout) { menus.at(h).layout = layout; }
MenuLayout GetLayout(void*, MenuHandle h) { return menus.at(h).layout; }
void Role(void*, MenuHandle h, int i, MenuItemRole role) { menus.at(h).items.at(i).role = role; }
void Subtext(void*, MenuHandle h, int i, const char* text) { menus.at(h).items.at(i).sub = text; }
void Span(void*, MenuHandle h, int i, int span) { menus.at(h).items.at(i).span = span; }
void Highlight(void*, MenuHandle h, int i, bool on) { menus.at(h).items.at(i).highlight = on; }
void Pinned(void*, MenuHandle h, int i) { menus.at(h).pinned = i; }
void ChangeCallback(void*, MenuHandle h, MenuItemChangeFn callback) { menus.at(h).change = std::move(callback); }
int ChoiceAdd(void* p, MenuHandle h, const char* label, const char* const* options, int count, int selected, const char* info) {
    assert(selected == 1);
    const int i = AddItem(p, h, label, info, false);
    for (int option = 0; option < count; ++option) menus.at(h).items.at(i).options.emplace_back(options[option]);
    return i;
}
bool Confirm(void*, PluginId, int slot, const char* title, const char* body, const char*, const char* confirm, bool, MenuConfirmFn callback) {
    assert(slot == 0);
    if (!dialogAvailable) return false;
    ++shownDialogs;
    const auto previous = dialog.done;
    // Match the pinned provider's panorama_hud::StripColors, including its removal of LF.
    // Testing the unfiltered input hid the broken popup formatting in 0.1.11.
    std::string displayed;
    for (const unsigned char c : std::string(body)) if (c > 0x10) displayed += static_cast<char>(c);
    dialog = {title, displayed, confirm, std::move(callback)};
    if (previous) previous(slot, false); // ShowMenuConfirm answers a replaced dialog as cancelled
    return true;
}
void Print(void*, CPlayerSlot slot, const char* text) { assert(slot.Get() == 0); consoleLines.emplace_back(text); }
void Replay(void*, CPlayerSlot slot, const CCommand& args) {
    assert(slot.Get() == 0 && args.ArgC() == 2 && std::string(args.Arg(0)) == "kz_replay");
    replayCommands.emplace_back(args.Arg(1));
}
int ColumnAdd(void*, MenuHandle h, const char* label, int cells, int) {
    assert(cells == 0); // the time is never split into 26px cells
    menus.at(h).heads.emplace_back(label);
    return static_cast<int>(menus.at(h).heads.size()) - 1;
}
void Details(void*, MenuHandle h, int i, const char* const* lines, int count) {
    auto& details = menus.at(h).items.at(i).details;
    details.clear();
    for (int line = 0; line < count; ++line) details.emplace_back(lines[line]);
}
int ChipAdd(void*, MenuHandle h, const char* label, const char* const* options, int count, int selected) {
    Chip chip; chip.label = label; chip.selected = selected;
    for (int i = 0; i < count; ++i) chip.options.emplace_back(options[i]);
    menus.at(h).chips.push_back(std::move(chip));
    return static_cast<int>(menus.at(h).chips.size()) - 1;
}
int ActionAdd(void* p, MenuHandle h, const char* label, const char* const* options, int count, bool on) { return ChipAdd(p, h, label, options, count, on ? 1 : 0); }
int NoteAdd(void* p, MenuHandle h, const char* label, const char*) { return ChipAdd(p, h, label, nullptr, 0, 0); }
void ChipCallback(void*, MenuHandle h, MenuChipFn callback) { menus.at(h).chip = std::move(callback); }
void EndCallback(void*, MenuHandle h, MenuEndFn callback) { menus.at(h).end = std::move(callback); }
void RefreshCallback(void*, MenuHandle h, MenuRefreshFn callback) { menus.at(h).refresh = std::move(callback); }
void BoolSetting(void*, MenuHandle, bool) {}
void Style(void*, MenuHandle, MenuStyle, const char*) {}
void Key(void*, MenuHandle, MenuNavAction, MenuButton) {}
void Features(void*, MenuHandle, int) {}
void HideHint(void*, int) {}
bool Message(void*, int, const char*, MenuTone, float) { ++shownMessages; return true; }
bool ValidPlayer(void*, int slot) { return slot == 0; }
uint64_t SteamPlayer(void*, int) { return playerSteam; }

// The friend only supplies dependencies and drives the real production methods.
struct MapTopHUDUITest {
    static void Run(ICS2Menus* provider, ICS2KZ* kz) {
        MapTopHUD hud;
        hud.menus_ = provider; hud.kz_ = kz; hud.map_ = "kz_test";
        auto& session = hud.sessions_[0];
        session.steam = playerSteam; session.generation = 1;
        session.filter.map = hud.map_; session.filter.course = "Main";
        session.courses = {"Main", "Bonus"};
        Page page; page.more = true;
        for (int i = 0; i < PageSize; ++i) {
            Record row;
            row.id = "019b618d-60f0-75c3-9e82-82ed4c19954" + std::to_string(i);
            row.name = i == 0 ? "日本語 " + std::string(200, 'X') : "Player " + std::to_string(i);
            row.rank = i + 1; row.time = 314.453125 + i;
            row.steam = i == 0 ? playerSteam : playerSteam + i;
            row.teleports = i; row.replay = i != 1;
            row.rankedPlayers = 27; row.created = "2026-10-24 12:34:56";
            row.courseStats = CourseStats(R"({"splitZoneTimes":[30,60],"cpZoneTimes":[-1,75]})", row.time);
            page.rows.push_back(row);
        }
        auto board = [&] {
            // Simulate a completed course fetch before displaying the fixture records.
            if (session.courses.empty()) session.courses = {"Main", "Bonus"};
            hud.Board(0, page); return active;
        };
        auto pick = [&](int item) {
            const auto handle = active;
            const auto callback = menus.at(handle).select;
            callback(handle, 0, item);
        };
        auto pickProfile = [&](int row) { pick(1 + row * 2); };
        auto pickReplay = [&](int row) { pick(2 + row * 2); };
        const std::string background = "\x08\x01\x12\x08" "cs_stage";
        auto backgroundPost = [&](const std::string& packet = "\x08\x01\x12\x08" "cs_stage", int slot = 0) {
            hud.HudClickPost(nullptr, CPlayerSlot(slot), 390, static_cast<uint32>(packet.size()), packet.data());
        };
        auto chip = [&](const std::string& label, int selected) {
            const auto handle = active;
            const auto callback = menus.at(handle).chip;
            const auto& chips = menus.at(handle).chips;
            const auto found = std::find_if(chips.begin(), chips.end(), [&](const Chip& c) { return c.label == label; });
            assert(found != chips.end());
            callback(handle, 0, static_cast<int>(found - chips.begin()), selected);
        };
        auto actions = [&](int row) {
            pickProfile(row);
            if (dialogAvailable) { auto callback = dialog.done; callback(0, true); }
            assert(menus.at(active).layout == MenuLayout::List);
        };
        auto pager = [&](int button) { pick(static_cast<int>(menus.at(active).items.size()) - 3 + button); };
        const auto original = board();
        const auto initial = menus.at(original);
        assert(initial.layout == MenuLayout::Studio && initial.heads.empty());
        assert(initial.chips.at(0).label == "Source");
        assert((initial.chips.at(0).options == std::vector<std::string>{"Local", "Global"}));
        assert(initial.chips.at(0).selected == 0);
        assert((initial.chips.at(1).options == std::vector<std::string>{"VNL", "CKZ"}));
        assert(initial.chips.at(1).label == "Mode" && initial.chips.at(1).selected == 1);
        assert((initial.chips.at(2).options == std::vector<std::string>{"Overall", "Pro"}));
        assert(initial.chips.at(2).label == "Type" && initial.chips.at(2).selected == 0);
        assert(PageSize == 5 && initial.items.size() == 14 && initial.chips.size() == 3);
        int slots = 0;
        for (const auto& item : initial.items) slots += item.options.empty() ? 1 : static_cast<int>(item.options.size());
        assert(initial.items.size() <= 18 && slots <= 24); // stock Studio native page limits
        assert(initial.items.front().text == "Main" && initial.items.front().info == "courses");
        assert(initial.items.front().span == 3 && initial.items.front().sub == "Courses ▾" && !initial.items.front().disabled);
        assert(std::none_of(initial.items.begin(), initial.items.end(), [](const Item& item) { return item.text.find("Filters") != std::string::npos; }));
        assert(std::none_of(initial.items.begin(), initial.items.end(), [](const Item& item) { return item.text == "Profile"; }));
        const auto pagerStart = initial.items.size() - 3;
        assert(initial.items.at(pagerStart).text == "<" && initial.items.at(pagerStart).info == "prev" && initial.items.at(pagerStart).disabled);
        assert(initial.items.at(pagerStart + 1).text == "Page 1" && initial.items.at(pagerStart + 1).highlight);
        assert(initial.items.back().text == ">" && initial.items.back().info == "next" && !initial.items.back().disabled);
        for (size_t i = pagerStart; i < initial.items.size(); ++i) assert(initial.items.at(i).span == 1 && initial.items.at(i).options.empty());
        assert(!initial.change); // separate normal buttons, not the differently sized Choice segments
        for (int i = 0; i < PageSize; ++i) {
            const auto& item = initial.items.at(1 + i * 2);
            assert(item.info == "p:" + std::to_string(i));
            assert(!item.disabled); // a missing replay must never disable the player's profile
            assert(item.text == "#" + std::to_string(i + 1) + " " + Text(page.rows[i].name, 32));
            assert(item.sub == RankingTime(page.rows[i].time) + " | TP " + std::to_string(i));
            assert(item.highlight == (i == 0));
            assert(item.span == 2);
            const auto& replay = initial.items.at(2 + i * 2);
            assert(replay.text == "Replay ▶" && replay.info == "r:" + std::to_string(i));
            assert(replay.disabled == (i == 1));
            assert(replay.span == 1 && item.span + replay.span == 3);
        }
        assert(initial.items.at(1).sub == "05:14.453 | TP 0");
        pickProfile(1); // the wide details button remains usable without a replay
        assert(active == original && dialog.title == "Player 1");
        assert(dialog.body.find("05:15.453") != std::string::npos && dialog.confirm == "Actions");
        assert(dialog.body.find(PopupLineBreak) != std::string::npos && dialog.body.find('\n') == std::string::npos);
        assert(dialog.body.find("Overall #2 / 27") != std::string::npos);
        assert(dialog.body.find("CKZ   |   Local") != std::string::npos);
        assert(dialog.body.find("MAP / COURSE") != std::string::npos && dialog.body.find("kz_test") != std::string::npos);
        assert(dialog.body.find("RECORDED") != std::string::npos);
        assert(dialog.body.find("Splits: 2/2 | 01:00.000") != std::string::npos);
        assert(dialog.body.find("Not stored") == std::string::npos && dialog.body.find("Sync / Air Time") == std::string::npos);
        assert(dialog.body == RunPopupBody(page.rows[1], session.filter, session.details->global));
        assert(dialog.body.find("Local Overall #2 / 27") != std::string::npos);
        assert(dialog.body.find("Global PB: Loading...") != std::string::npos);
        const auto detailRequest = session.details;
        const auto replacedCallback = dialog.done;
        hud.ApplyGlobalRank(0, detailRequest, {GlobalRankState::Ready, 123, 0, 300});
        assert(session.details == detailRequest && !detailRequest->actions);
        assert(dialog.body.find("Global PB Overall #123") != std::string::npos && dialog.body.find("PB: 05:00.000 | TP 0") != std::string::npos);
        replacedCallback(0, false); // replacing the loading popup must not close the new one
        assert(session.details == detailRequest);
        assert(dialog.body.find("SteamID64:") == std::string::npos && dialog.body.find("Run ID:") == std::string::npos);
        auto close = dialog.done; close(0, false);
        assert(active == original);
        const auto afterClose = shownDialogs;
        hud.ApplyGlobalRank(0, detailRequest, {GlobalRankState::Ready, 456, 0, 299});
        assert(shownDialogs == afterClose && !session.details); // never reopen a dismissed popup
        actions(2);
        const auto actionRequest = session.details;
        hud.ApplyGlobalRank(0, actionRequest, {GlobalRankState::Ready, 88, 0, 298});
        assert(menus.at(active).layout == MenuLayout::List);
        assert(std::any_of(menus.at(active).items.begin(), menus.at(active).items.end(), [](const Item& item) { return item.text == "Global PB Overall #88"; }));
        pick(1);
        assert(consoleLines.size() == 1 && consoleLines.back().find(CS2KZProfileURL(page.rows[2].steam)) != std::string::npos);
        assert(replayCommands.empty());
        const auto staleDialog = dialog.done;
        pick(0);
        session.filter.offset = PageSize;
        board();
        pickProfile(1);
        const auto staleRank = session.details;
        pickProfile(2);
        const auto currentDetail = session.details;
        hud.ApplyGlobalRank(0, staleRank, {GlobalRankState::Ready, 999, 0, 1});
        assert(session.details == currentDetail && dialog.body.find("#999") == std::string::npos);
        ++playerSteam; hud.ApplyGlobalRank(0, currentDetail, {GlobalRankState::Ready, 777, 0, 1}); --playerSteam;
        assert(dialog.body.find("#777") == std::string::npos);
        auto dismiss = dialog.done; dismiss(0, false);
        const auto beforeBackground = active;
        const auto backgroundTicket = hud.Current(0);
        turning = true; // CS2Menus has already handled the event in its pre-hook
        backgroundPost();
        assert(!turning && active != beforeBackground && hud.Valid(0, backgroundTicket));
        assert(session.filter.offset == PageSize && session.filter.course == "Main" && !session.filter.global && !session.filter.pro);
        assert(session.board && session.board->rows.size() == PageSize);
        assert(menus.at(active).items.at(pagerStart + 1).text == "Page 2");
        const auto afterBackground = active;
        backgroundPost(background.substr(0, 3));
        backgroundPost("\x08\x01\x12\x08" "cs_item0");
        backgroundPost(background, -1); backgroundPost(background, 64);
        ++playerSteam; backgroundPost(); --playerSteam;
        assert(active == afterBackground);
        active = 0x7fffffff; // another plugin owns the current display
        backgroundPost();
        assert(active == 0x7fffffff);
        active = afterBackground;
        pick(0);
        const auto coursesMenu = active;
        const auto staleCourseSelect = menus.at(coursesMenu).select;
        backgroundPost(); assert(active == coursesMenu); // never take over a List-based picker
        assert(menus.at(coursesMenu).title == "MAP TOP | COURSES");
        assert(menus.at(coursesMenu).items.size() == 2); // only courses, no source/mode/type/back rows
        assert(menus.at(coursesMenu).items.at(0).text == "> Main" && menus.at(coursesMenu).items.at(1).text == "Bonus");
        pick(0); // picking the current course returns to the same cached page
        assert(session.filter.course == "Main" && session.filter.offset == PageSize && session.board);
        assert(menus.at(active).items.front().info == "courses");
        pick(0); pick(1);
        assert(session.filter.course == "Bonus" && session.filter.offset == 0 && !session.board);
        assert(!session.filter.global && !session.filter.pro && session.filter.mode == "classic");
        assert(menus.at(active).items.front().info == "courses" && menus.at(active).items.front().text == "Bonus");
        staleCourseSelect(coursesMenu, 0, 0);
        assert(session.filter.course == "Bonus");
        board();
        chip("Mode", -1); // reselecting a required filter may not leave it empty
        assert(session.filter.mode == "classic" && menus.at(active).chips.at(1).selected == 1);
        const auto staleSelect = menus.at(active).select;
        session.filter.offset = 30;
        chip("Mode", 0);
        hud.ApplyGlobalRank(0, currentDetail, {GlobalRankState::Ready, 666, 0, 1});
        assert(!session.details && dialog.body.find("#666") == std::string::npos);
        assert(session.filter.mode == "vanilla" && session.filter.offset == 0 && !session.filter.global && !session.filter.pro);
        const auto loadingGeneration = session.generation;
        const auto inFlight = hud.Current(0);
        const auto beforeLoadingBackground = active;
        turning = true; backgroundPost();
        assert(!turning && active != beforeLoadingBackground && hud.Valid(0, inFlight) && !session.board);
        assert(session.generation == loadingGeneration);
        hud.Loading(0, "Global API temporarily unavailable.");
        backgroundPost();
        assert(menus.at(active).items.at(1).text == "Global API temporarily unavailable.");
        chip("Mode", -1); // restoring a loading dropdown must keep its in-flight request valid
        assert(session.generation == loadingGeneration && menus.at(active).chips.at(1).selected == 0);
        staleDialog(0, true);
        assert(consoleLines.size() == 1);
        auto current = active;
        staleSelect(original, 0, 0);
        assert(active == current);
        chip("Type", 1);
        assert(session.filter.pro && session.filter.mode == "vanilla");
        chip("Source", 1);
        assert(session.filter.global && session.filter.pro && session.filter.mode == "vanilla");
        assert(session.courses.empty() && menus.at(active).items.front().disabled); // never offer the old source's course list
        session.filter.offset = PageSize;
        board();
        assert(menus.at(active).items.at(pagerStart + 1).text == "Page 2");
        assert(!menus.at(active).items.at(pagerStart).disabled && !menus.at(active).items.back().disabled);
        pager(2);
        assert(session.filter.offset == PageSize * 2);
        board(); pager(0);
        assert(session.filter.offset == PageSize);
        board(); chip("Type", 0);
        assert(!session.filter.pro && session.filter.offset == 0 && session.filter.global);
        board();
        const auto beforeEdge = session.generation;
        pager(0); pager(1); // first-page back and current-page picks are inert
        assert(session.filter.offset == 0 && session.generation == beforeEdge);
        actions(1);
        assert(session.details->global.state == GlobalRankState::Hidden); // Global view already has its rank
        assert(menus.at(active).items.at(2).disabled);
        pick(2); // a forged selection of a disabled replay must also be rejected
        assert(replayCommands.empty());
        pick(0);
        dialogAvailable = false; // the HTML/chat fallback still exposes useful actions
        pickProfile(2);
        assert(menus.at(active).title == "RUN DETAILS | Player 2");
        assert(menus.at(active).items.at(0).text == "Back to rankings");
        assert(menus.at(active).items.at(1).text == "Show CS2KZ URL (console)");
        assert(menus.at(active).items.at(2).text == "Replay");
        pick(0);
        const auto restored = active;
        hud.data_.Frame(); // late profile response must not take over the restored board
        assert(active == restored);
        dialogAvailable = true;
        pickReplay(0); // direct record button immediately dispatches, without a profile or confirmation
        assert(replayCommands.size() == 1 && replayCommands.back() == page.rows[0].id);
        assert(active == 0 && !session.board);
        session.steam = playerSteam; session.filter.global = false;
        board(); pickReplay(0); // missing local file disables only that replay and leaves the profile usable
        assert(session.board && !session.board->rows.at(0).replay && session.board->rows.at(2).replay);
        assert(!menus.at(active).items.at(1).disabled);
        assert(menus.at(active).items.at(2).disabled);
        pickProfile(0); assert(dialog.title == Text(page.rows[0].name));
        assert(replayCommands.size() == 1);
        page.rows[0].id = "bad;quit";
        board(); pickReplay(0);
        assert(replayCommands.size() == 1 && active);
        board();
        pickProfile(2); assert(dialog.title == "Player 2");
        Page empty;
        hud.Board(0, empty);
        assert(menus.at(active).items.at(0).info == "courses" && menus.at(active).items.at(1).disabled);
        assert(menus.at(active).items.at(1).span == 3); // empty state must leave the pager at a complete row boundary
        assert(menus.at(active).items.at(menus.at(active).items.size() - 3).disabled && menus.at(active).items.back().disabled);
        pager(2); assert(session.filter.offset == 0 && !session.board->more);
        assert(menus.at(active).chips.at(0).options.size() == 2); // can switch out of an empty category
        chip("Mode", 1);
        assert(session.filter.mode == "classic" && session.filter.offset == 0);
        hud.ClearAll();
        backgroundPost(); assert(active == 0); // never reopen a closed display
        hud.data_.Frame(); // queued callbacks for old filter generations are inert
        assert(active == 0 && !session.board);
        assert(menus.empty());
    }
};
}

int main() {
    Stub menuStub, kzStub, engineStub, clientsStub;
    menuStub.Set(&ICS2Menus::CreateMenuFor, CreateMenu);
    menuStub.Set(&ICS2Menus::DestroyMenu, Destroy);
    menuStub.Set(&ICS2Menus::DisplayMenu, Display);
    menuStub.Set(&ICS2Menus::GetActiveMenu, Active);
    menuStub.Set(&ICS2Menus::CancelMenu, Cancel);
    menuStub.Set(&ICS2Menus::AddItem, AddItem);
    menuStub.Set(&ICS2Menus::GetItemInfo, ItemInfo);
    menuStub.Set(&ICS2Menus::SetMenuLayout, Layout);
    menuStub.Set(&ICS2Menus::GetMenuLayout, GetLayout);
    menuStub.Set(&ICS2Menus::SetItemRole, Role);
    menuStub.Set(&ICS2Menus::SetItemSubtext, Subtext);
    menuStub.Set(&ICS2Menus::SetItemSpan, Span);
    menuStub.Set(&ICS2Menus::SetItemHighlight, Highlight);
    menuStub.Set(&ICS2Menus::SetMenuPinnedItem, Pinned);
    // Leave table corner methods unbound: using them would regress stock-only compatibility.
    menuStub.Set(&ICS2Menus::SetMenuChangeCallback, ChangeCallback);
    menuStub.Set(&ICS2Menus::AddChoice, ChoiceAdd);
    menuStub.Set(&ICS2Menus::ShowMenuConfirmFor, Confirm);
    menuStub.Set(&ICS2Menus::AddMenuColumn, ColumnAdd);
    menuStub.Set(&ICS2Menus::SetItemDetails, Details);
    menuStub.Set(&ICS2Menus::AddMenuChip, ChipAdd);
    menuStub.Set(&ICS2Menus::AddMenuAction, ActionAdd);
    menuStub.Set(&ICS2Menus::AddMenuNote, NoteAdd);
    menuStub.Set(&ICS2Menus::SetMenuChipCallback, ChipCallback);
    menuStub.Set(&ICS2Menus::SetMenuEndCallback, EndCallback);
    menuStub.Set(&ICS2Menus::SetMenuRefreshCallback, RefreshCallback);
    menuStub.Set(&ICS2Menus::SetCloseOnSelect, BoolSetting);
    menuStub.Set(&ICS2Menus::SetMenuForceType, BoolSetting);
    menuStub.Set(&ICS2Menus::SetMenuStyle, Style);
    menuStub.Set(&ICS2Menus::SetMenuKey, Key);
    menuStub.Set(&ICS2Menus::SetMenuTextFeatures, Features);
    menuStub.Set(&ICS2Menus::HideMenuHint, HideHint);
    menuStub.Set(&ICS2Menus::ShowMenuMessage, Message);
    kzStub.Set(&ICS2KZ::IsValidPlayer, ValidPlayer);
    kzStub.Set(&ICS2KZ::GetSteamID64, SteamPlayer);
    engineStub.Set(&IVEngineServer::ClientPrintf, Print);
    clientsStub.Set(&IServerGameClients::ClientCommand, Replay);
    engine = reinterpret_cast<IVEngineServer*>(&engineStub.object);
    clients = reinterpret_cast<IServerGameClients*>(&clientsStub.object);
    MapTopHUDUITest::Run(reinterpret_cast<ICS2Menus*>(&menuStub.object), reinterpret_cast<ICS2KZ*>(&kzStub.object));
    engine = nullptr; clients = nullptr;
    std::cout << "production stock blocks: background-click recovery, preserved page/loading tickets, foreign-menu isolation, courses-only picker, controls, replay, stale callbacks: OK\n";
}
