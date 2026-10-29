# KZ MapTop HUD 0.1.15

English | [日本語](README-ja.md)

An independent, unofficial Linux x86_64 Metamod plugin for **CS2KZ v0.0.189**.
Open the current map's leaderboard with `!mtop`, browse local or global records,
inspect a run, and start its replay. Rendering and input use the public
**CS2Menus `ICS2Menus008`** interface.

The release contains the MapTop HUD binary, installation files, documentation,
licenses, and corresponding source. It uses the standard CS2Menus server plugin
and Workshop addon. Existing CS2KZ and RTV HUD files are kept intact.

**Live CS2 server loading, in-game rendering, mouse input, and replay playback
still need validation.** Automated checks and the original build validation are
described below; they do not establish that the plugin works on a live server.

## Features

- **Local / Global** record sources, **Overall / Pro (0 TP)** categories,
  **VNL / CKZ** modes, and course selection.
- The current map, player mode, and course are selected initially. If the player
  is outside a course, the first available course is used.
- Five records per page in the standard **Studio** layout. Each record has a
  wide information button spanning two columns and a separate **Replay** button.
- Optional **Table** layout with up to **15 records per page**, configured only
  through the existing HUD JSON file. Both layouts share the same filters,
  run details, Global PB lookup, and replay actions.
- Run details include time, teleports, rank, mode, source, map, and course.
  Global records can show the server name and points. Local records can show
  their date and split, checkpoint, and stage information stored in Metadata.
- Local run details load the matching player's **Global PB rank** separately.
  If that PB differs from the selected local run, its time and TP count are
  shown explicitly. A player-filtered API `total: 1` is not used as the ranking
  population. `No ranked record` and `Unavailable` distinguish missing rank data
  from disabled or failed API access.
- Popup text uses Unicode line separators to retain section boundaries after
  CS2Menus strips ordinary line feeds. Empty sections are omitted.
- Background clicks restore MapTop's own active Studio leaderboard while
  retaining its page, filters, records, and loading or error state.
- Responses that arrive after a map change, disconnect, closed menu, changed
  filter, or replaced request are discarded.
- The player's own record is highlighted using the standard green style.
  Player names are passed as plain text.

Global PB data is fetched only when local details are opened. Global details use
the rank already present in the fetched record. Late PB responses do not reopen
closed details or replace another record's screen.

## Requirements

- A **Linux x86_64 CS2 server**.
- **Metamod API18 / KHook**. An API17 build cannot load this plugin.
- **CS2KZ v0.0.189**, exposing `ICS2KZ002`.
- Standard **CS2Menus**, exposing `ICS2Menus008`.
- **MultiAddonManager** and the CS2Menus Workshop addon **3801628903** for
  Panorama rendering and mouse input.
- Compatible **SQLMM** and an existing CS2KZ database for local records.
- Steam HTTP support and HTTP(S) access to CS2KZ's configured `apiUrl` for global
  records.

When `Panorama` / `MountAddon` is `1` in `cfg/cs2menus/core.cfg`, CS2Menus mounts
its addon through MultiAddonManager. Retain existing CS2KZ and RTV HUD addon IDs.

These are the release's target interfaces and dependencies. Compatibility with
other versions requires separate validation. Windows-native DLLs and dependency
plugin binaries are not included.

## Installation

1. Stop a test server and copy the **contents of `game/csgo/` inside the ZIP** into
   the server's `game/csgo/` directory.
2. Keep the existing **`cfg/cs2kz-server-config.txt`**. MapTop reads its `db` and
   `apiUrl` values using the same Valve KeyValues parser as CS2KZ.
3. The existing `addons/kz_maptop_hud/config.json` controls the menu timeout and
   server-wide display mode:

   ```json
   {
     "menu_seconds": 120,
     "display_mode": "studio"
   }
   ```

4. Start the server with the required plugins and run `meta list` in the server
   console. Confirm that **KZ MapTop HUD 0.1.15** loads successfully.
5. Join the server, run `!mtop`, and check both Local and Global records, course
   selection, paging, run details, and replay playback.

The deployment files are:

```text
game/csgo/
└── addons/
    ├── metamod/kz_maptop_hud.vdf
    └── kz_maptop_hud/
        ├── config.json
        └── bin/linuxsteamrt64/kz_maptop_hud.so
```

The ZIP also contains documentation, license notices, `release-manifest.json`,
`SHA256SUMS.txt`, and `source/kz-maptop-hud/`. Only its `game/csgo/` contents need
to be copied to the server.

### Updating an existing installation

Stop the server, replace
`game/csgo/addons/kz_maptop_hud/bin/linuxsteamrt64/kz_maptop_hud.so`, and restart.
Keep existing settings. This version does not require changes to the CS2KZ cfg,
database, or Workshop addon. `kz_reload_config` cannot replace a loaded binary.

If you installed the modified CS2Menus binary bundled with MapTop HUD 0.1.5,
restore your backed-up standard `cs2menus.so`.

The SDK `g_pCVar` initialization fix introduced in 0.1.1 is retained. It addresses
the null-pointer crash in `CCommand::Tokenize()` when parsing chat commands in
0.1.0.

### Verifying the download

Download the ZIP and its matching `.zip.sha256` file into the same directory:

```bash
sha256sum -c KZ-MapTop-HUD-0.1.15-linux.zip.sha256
```

After extracting the ZIP, run `sha256sum -c SHA256SUMS.txt` from its root to verify
the individual packaged files. The internal list excludes the checksum file
itself.

## Configuration

### Display mode

Set `display_mode` in `addons/kz_maptop_hud/config.json` to `"studio"` (default)
or `"table"`. There is no in-game display-mode selector or per-player override.
Existing configurations without this key keep the Studio layout.

```json
{
  "menu_seconds": 120,
  "display_mode": "table"
}
```

Run `kz_reload_config` in the server console, then reopen `!mtop`. Reloading
closes the current HUD and discards old requests. Invalid HUD JSON or an invalid
mode retains the previous HUD settings; a fresh process defaults to Studio.

Table places **`< n >` immediately after Type** in the header. The middle number
is informational. The arrows do nothing at the corresponding page boundary or
while records are loading.

The main column shows **rank / time / player**, with the full time such as
`05:14.453` in one uninterrupted label. Time is no longer split between tiny
numeric cells. Separate columns show TP count and the Replay expander/status.

Click a record, including its Replay column, to expand **Details** and **Replay**
directly beneath it. Click it again to collapse it; opening another record
collapses the previous one. The stock API delivers a row-wide click, so the
expander is also accessible from the rest of that row. **Details** opens the
existing run popup; **Replay** starts playback directly. An unavailable replay
is disabled while Details stays usable. The profile URL remains under the
detail popup's **Actions** screen.

The 18-row capacity allows one course selector, **15 records**, and two action
rows for the expanded record. All filters, course selection, own-record
highlighting, and asynchronous Global PB details remain available. TP counts
that exceed their column width end in `..`; details retain the complete value.

Studio keeps its five-record layout and bottom pager. HTML/chat fallbacks keep
the continuous time, inline actions, and bottom paging rows because those
renderers do not show the header controls.

### Database and API

Connection settings come from `cfg/cs2kz-server-config.txt`; do not copy database
credentials into the HUD JSON file.

| CS2KZ setting | Use |
|---|---|
| `db / driver` | `sqlite` or `mysql`; local records are disabled without a `db` section |
| `db / database` | SQLite name resolves to `addons/cs2kz/data/<value>.sqlite3`; MySQL uses the database name |
| `db / host`, `user`, `pass` | MySQL connection and authentication |
| `db / port`, `timeout` | MySQL settings; defaults are 3306 and 60 seconds |
| `apiUrl` | Global record API; omitted uses the CS2KZ default, empty disables it |

The plugin does not use `apiKey`. Replay downloads and `replayDownloadUrl` are
handled by CS2KZ. KeyValues comments, quoting, nesting, and includes use the
engine's parser.

SQLite connections require an existing database file to avoid creating an empty
database at a wrong path. Queries are read-only `SELECT` statements; the plugin
does not migrate or update CS2KZ records. Failed local database connections are
retried every five seconds.

Use **`kz_reload_config` in the server console** to reload the API URL and menu
timeout. Database connection changes take effect after a **server restart**.
An invalid reload retains the working configuration. Database credentials are
not displayed in the HUD.

## Commands and controls

| Input | Action |
|---|---|
| `!maptop` / `/maptop` | Open the HUD after the standard command runs |
| `!mtop` / `/mtop` | Open the HUD directly |
| `!maptop_hud` / `/maptop_hud` | Open the HUD directly |
| Client console: `kz_maptop_hud` | Open the HUD directly |
| Client console: `kz_maptop` without arguments | Open the HUD after the standard command runs |

`!maptop` retains its standard chat and console output. Commands with arguments
are left to CS2KZ. Use `!mtop` to open only the HUD. The leaderboard targets the
current map.

- Use the top **Source / Mode / Type** dropdowns to select Local or Global,
  VNL or CKZ, and Overall or Pro. A change resets paging while keeping the other
  selections. The API mode names are `vanilla` and `classic`.
- The **Courses** row above the records opens course selection. Selecting a
  different course starts at page 1; selecting the current course retains the
  page. Use the standard close control to cancel.
- In Studio, select a record's information button to open its details. **Actions** offers
  **Replay** and **Show CS2KZ URL (console)**. If a detail popup is unavailable,
  the Actions screen opens directly.
- In Table, expand a record's Replay row, then choose **Details** or **Replay**.
  The separate **Replay** button starts playback immediately. There is no
  confirmation dialog. Unavailable or missing replays cannot be started.
- Table uses **`< n >` next to Type**; Studio and HTML/chat fallbacks use the
  bottom **`< Page n >`** controls. Neither advances beyond the available records.

The profile action prints `https://cs2kz.org/profile/<SteamID64>` to the client
console for manual copying. It does not open a browser or copy to the clipboard.

CS2KZ's replay bot is shared, so starting playback can affect other spectators
and interrupt your own run. MapTop executes `kz_replay <validated UUID>` for the
requesting player. Use CS2KZ's standard replay controls for pause and speed.

## Rendering and data limitations

If Panorama is unavailable, CS2Menus falls back to HTML and then chat. In HTML,
use **W/S** to navigate and **E** to select. Mouse input, the top dropdowns, and
detail popups require Panorama. HTML and chat show the record blocks, Courses,
Replay, and paging as ordinary rows.

The standard Studio addon controls fonts, colors, and button sizes. MapTop does
not inject HTML or CSS. Its built-in back, forward, refresh, and collapse icons
remain present; controls without an available action are disabled.

Local records do not store the originating server name. The popup omits that
section, and Actions shows `Not stored in local DB`. Empty or invalid Metadata
omits only the affected statistics.

Sync, Air Time, Perfect Bhops, and similar movement statistics are not present
in the referenced record schema and stored Metadata. Actions reports
`Sync / Air Time: unavailable`; unrelated player or jump statistics are not
substituted. Global API failures do not prevent local run details from opening.

## Building and packaging

Use Linux with Git, curl, CMake, a C++20 compiler, and Python 3. `bootstrap.sh`
fetches the pinned SDK, Metamod, API headers, and JSON dependency listed in
[NOTICE.md](NOTICE.md).

```bash
bash bootstrap.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 tools/package.py build/kz_maptop_hud.so
```

The plugin is written to `build/kz_maptop_hud.so`. Packaging creates:

```text
release/KZ-MapTop-HUD-0.1.15-linux.zip
release/KZ-MapTop-HUD-0.1.15-linux.zip.sha256
```

The packaging script includes an explicit list of deployment files, both
READMEs, licenses, corresponding source, required public headers, and JSON.
Build directories, logs, database contents, server connection settings, backups,
and dependency plugin binaries are excluded. The ZIP includes per-file SHA-256
hashes and a manifest with the plugin binary's hash and target interfaces.

The existing distribution binary was built with **GCC 10.3 in the Steam Runtime
Sniper SDK**. Build within Docker or WSL when working on Windows. Stop the server
before compiling on the same host and account for compiler memory usage.

Logic tests can run without the SDK, but still require
`deps/json/nlohmann/json.hpp`:

```bash
cmake -S . -B build-tests -DMAPTOP_BUILD_PLUGIN=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Validation status

Version **0.1.15** covers header paging, uninterrupted time labels, inline
Details/Replay expansion, stale expanded-row callbacks, loading/page boundaries,
and HTML/chat paging fallbacks in the production menu tests. Studio behavior is
covered by the existing regression cases.

On **2026-10-29**, the plugin was rebuilt with GCC 10.3 in the Steam Runtime
Sniper SDK. All six CTest checks passed, including loading the actual binary
for the API18 check. Table cases cover 15-record retrieval and paging, filters,
course selection, record details, replay availability, playback routing, and
discarding stale callbacks. These tests use API stubs for menu input and do not
establish live game rendering or playback.

The original validation record is dated **2026-10-24** and is detailed in
[README-ja.md](README-ja.md). It covers:

- `core`: identifiers, time formatting, record parsing, Global PB matching,
  metadata, request lifetime, paging, and background-click event parsing.
- `local_ranking`: generated SQL executed against SQLite, including filtering,
  ties, personal bests, rank population, metadata, and page boundaries.
- `binary_api18`: loading the actual shared library and checking
  `CreateInterface("ISmmPlugin")` and `GetApiVersion() == 18`.
- `cs2kz_config`: Valve KeyValues parsing, SQLite and MySQL settings, and API
  configuration.
- `load_cvar_tokenize`: plugin loading and command parsing using the SDK
  tokenizer, including SDK-side `g_pCVar` initialization.
- `board_controls_actions`: menu construction and callbacks using public API
  stubs, including filters, details, popup separators, late responses, replays,
  and paging. This does not test actual Panorama rendering.
- Public API response parsing for leaderboards and player-specific Global PBs.

Before production use, validate plugin load order, Workshop mounting, layout,
mouse input, MySQL connectivity, fallback menus, replay playback, concurrent RTV
use, disconnects, map changes, and dependency reloads on a real server. In-game
line-separator rendering remains unverified.

## License and attribution

MapTop HUD is licensed under **AGPL-3.0**. The release includes corresponding
source, referenced public headers, and third-party notices. See [LICENSE](LICENSE),
[NOTICE.md](NOTICE.md), and `THIRD_PARTY/` for the licenses and pinned upstream
revisions.
