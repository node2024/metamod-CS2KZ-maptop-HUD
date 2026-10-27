// Exercise the production Load() and chat parser with the real SDK convar.cpp.
// Stop Load at the next missing interface, before any hooks or server resources are needed.
#ifndef MAPTOP_PLUGIN_SOURCE
#define MAPTOP_PLUGIN_SOURCE "../src/plugin.cpp"
#endif
#include MAPTOP_PLUGIN_SOURCE
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace {
// Minimal ABI stubs for the pinned Linux x86_64 SDK. Only the supplied virtual methods may run.
// All other vtable entries remain null so unexpected calls fail instead of silently succeeding.
template<class Method>
size_t VirtualSlot(Method method) {
    struct MemberPointer { std::ptrdiff_t offset, adjustment; } pointer{};
    static_assert(sizeof(pointer) == sizeof(method));
    std::memcpy(&pointer, &method, sizeof(method));
    assert(pointer.adjustment == 0 && (pointer.offset & 1));
    return static_cast<size_t>(pointer.offset - 1) / sizeof(void*);
}
struct Stub {
    std::array<void*, 128> methods{};
    struct Object { void** vtable; } object{methods.data()};
    template<class Method, class Function> void Set(Method method, Function function) {
        methods.at(VirtualSlot(method)) = reinterpret_cast<void*>(function);
    }
};
characterset_t breakSet{};
ICvar* expectedCvar = nullptr;
int characterSetReads = 0;
characterset_t* GetCharacterSet(void*) { ++characterSetReads; return &breakSet; }
void* GetDetour(void*, PluginId) { return nullptr; }
CreateInterfaceFn GetEngineFactory(void*, bool) { return nullptr; }
void* MatchInterface(void*, CreateInterfaceFn, const char* name, int) {
    return std::strcmp(name, CVAR_INTERFACE_VERSION) == 0 ? expectedCvar : nullptr;
}
bool ParseChat(const char* message, bool shared = false, const char* channel = "say") {
    const char* args[] = {channel, message};
    CCommand input(2, args);
    return ChatCommand(input, shared);
}
}

int main() {
    Stub cvarStub, apiStub;
    cvarStub.Set(&ICvar::GetCharacterSet, GetCharacterSet);
    expectedCvar = reinterpret_cast<ICvar*>(&cvarStub.object);
    apiStub.Set(&ISmmAPI::GetDetourInterface, GetDetour);
    apiStub.Set(&ISmmAPI::GetEngineFactory, GetEngineFactory);
    apiStub.Set(&ISmmAPI::VInterfaceMatch, MatchInterface);
    auto* api = reinterpret_cast<ISmmAPI*>(&apiStub.object);

    ::g_pCVar = nullptr;
    assert(!plugin.Load(1, api, nullptr, 0, false)); // deliberately missing the next engine interface
    assert(::g_pCVar == expectedCvar); // 0.1.0 fails here: SDK global was never initialized
    CharacterSetBuild(&breakSet, "{}()':");
    assert(ParseChat("!mtop"));
    assert(ParseChat("/mtop", false, "say_team"));
    assert(ParseChat("!maptop_hud"));
    assert(ParseChat("!maptop", true));
    assert(!ParseChat("!mtop_extra"));
    assert(!ParseChat("hello !mtop"));
    assert(!ParseChat("!maptop another_map", true));
    CCommand replay;
    assert(replay.Tokenize("kz_replay 019b618d-60f0-75c3-9e82-82ed4c19954a"));
    assert(replay.ArgC() == 2 && std::strcmp(replay.Arg(0), "kz_replay") == 0);
    assert(characterSetReads >= 8);

    ::g_pCVar = nullptr;
    assert(!plugin.Load(2, api, nullptr, 0, true)); // late loading must bind the SDK global as well
    assert(::g_pCVar == expectedCvar);
    assert(ParseChat("!mtop"));
    ::g_pCVar = nullptr; // the test's interface stubs are about to leave scope
    std::cout << "production Load / SDK g_pCVar / chat tokenization / replay tokenization / late load: OK\n";
}
