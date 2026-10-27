#include "config.h"
#include <cassert>
#include <iostream>
using namespace maptop;

int main() {
    KeyValues sqlite("ServerConfig");
    assert(sqlite.LoadFromBuffer("fixture", R"cfg(
        "ServerCfg" {
            // "apiUrl" "https://wrong.example"
            "apiUrl" "https://api.example.test/kz/"
            "db" { "driver" "SQLite" "database" "custom_records" }
            "Other service" { "apiUrl" "https://unrelated.example" }
        }
    )cfg"));
    auto one = ReadConnections(sqlite);
    assert(one.api == "https://api.example.test/kz");
    assert(one.driver == "sqlite" && one.database == "addons/cs2kz/data/custom_records.sqlite3");

    KeyValues mysql("ServerConfig");
    assert(mysql.LoadFromBuffer("fixture", R"cfg(
        "ServerCfg" {
            "db" { "driver" "mysql" "database" "local_kz" "host" "db.internal"
                "user" "reader" "pass" "literal//p%ss'word" "port" "3307" "timeout" "45" }
            "apiUrl" "http://127.0.0.1:8080/"
            "apiKey" "this-must-not-be-used"
        }
    )cfg"));
    auto two = ReadConnections(mysql);
    assert(two.api == "http://127.0.0.1:8080");
    assert(two.database == "local_kz" && two.host == "db.internal" && two.user == "reader");
    assert(two.password == "literal//p%ss'word" && two.port == 3307 && two.timeout == 45);

    KeyValues disabled("ServerConfig");
    assert(disabled.LoadFromBuffer("fixture", R"("ServerCfg" { "apiUrl" "" })"));
    auto off = ReadConnections(disabled);
    assert(off.api.empty() && off.driver.empty());

    KeyValues defaults("ServerConfig");
    assert(defaults.LoadFromBuffer("fixture", R"("ServerCfg" { "db" { "driver" "mysql" "database" "kz" } })"));
    auto standard = ReadConnections(defaults);
    assert(standard.api == "https://api.cs2kz.org" && standard.port == 3306 && standard.timeout == 60);
    std::cout << "Valve KeyValues / CS2KZ cfg / SQLite / MySQL / API / disabled / defaults: OK\n";
}
