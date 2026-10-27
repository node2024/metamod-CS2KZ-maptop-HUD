#pragma once
#include <keyvalues.h>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace maptop {
// Read with Valve's own KeyValues parser, just as CS2KZ v0.0.189 does.
// Own the strings: the KeyValues tree is destroyed immediately after loading.
struct Connections {
    std::string api, driver, database, host, user, password;
    int port = 3306, timeout = 60;
};
inline Connections ReadConnections(KeyValues& root) {
    Connections out;
    out.api = root.GetString("apiUrl", "https://api.cs2kz.org");
    while (!out.api.empty() && out.api.back() == '/') out.api.pop_back();
    if (!out.api.empty() && out.api.rfind("https://", 0) != 0 && out.api.rfind("http://", 0) != 0)
        throw std::runtime_error("CS2KZ apiUrl must use HTTP or HTTPS.");
    auto* db = root.FindKey("db");
    if (!db) return out; // CS2KZ also disables local storage without a db section.
    out.driver = db->GetString("driver");
    std::transform(out.driver.begin(), out.driver.end(), out.driver.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    out.database = db->GetString("database");
    if (out.driver == "sqlite") out.database = "addons/cs2kz/data/" + out.database + ".sqlite3";
    out.host = db->GetString("host"); out.user = db->GetString("user"); out.password = db->GetString("pass");
    out.port = db->GetInt("port", 3306); out.timeout = db->GetInt("timeout", 60);
    return out;
}
} // namespace maptop
