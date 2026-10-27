# KZ MapTop HUD

New consumer plugin using the public APIs of CS2Menus, CS2KZ and SQLMM.
Licensed under AGPL-3.0; see LICENSE. This is an independent, unofficial plugin.

Referenced upstream sources:

- CS2KZ public REST record contract, KZGlobalTeam:
  https://github.com/KZGlobalTeam/cs2kz-api/blob/master/crates/cs2kz-api/src/records.rs
  Global PB lookup uses top/player/map/course/mode/has_teleports and the
  nub_rank / pro_rank fields; the player-filtered total is not a rank denominator.
- CS2KZ v0.0.189, KZGlobalTeam / zer0.k and contributors, AGPL-3.0:
  https://github.com/KZGlobalTeam/cs2kz-metamod/tree/da535d95714b40de625134973a6effe5c20325e4
  Public ICS2KZ002 contract; local ranking query adapted from src/kz/db/queries/course_top.h;
  cfg path, KeyValues interpretation and restart-only DB settings follow src/kz/option and src/kz/db/setup_database.cpp;
  replay UUID / path / client-command behaviour follows src/kz/replays.
  Record details read the existing Times.Metadata / Created columns; the zone-time
  fields follow GetCurrentRunMetadata in src/kz/timer/kz_timer.cpp at the same pin.
- CS2Menus, FemboyKZ / jvnipers and contributors, GPL-3.0:
  https://github.com/FemboyKZ/mm-cs2menus/tree/acebda9109db79ef18bc0166c0522b9567593e00
  Studio background-click behavior and the cs_stage button id follow src/menu/manager.cpp
  and src/render/panorama_hud.cpp. MapTop observes the SDK message in its own post-hook
  and re-displays only its own active menu through the public API; CS2Menus is not modified.
  Popup formatting accounts for panorama_hud::StripColors removing ASCII LF;
  the UI test's text filter reproduces that upstream behavior at the same revision.
- mm-utils CS2Menus008 public API header, GPL-3.0:
  https://github.com/FemboyKZ/mm-utils/tree/6d659b448ad283cb3cb794ffec45b5d85c2ede78
- SQLMM public headers, zer0k-z / Source2ZE / AlliedModders and contributors, GPL-3.0:
  https://github.com/zer0k-z/sql_mm/tree/78e1e2c5bb59ddbac44c16b935edeca346e7fca5
- Metamod:Source, AlliedModders, API18 / KHook:
  https://github.com/alliedmodders/metamod-source/tree/170058a2f0dbd58106b7c76bfe88a94672295356
- S2SDK CS2 engine headers, libraries and per-project sources, Valve / AlliedModders:
  https://github.com/alliedmodders/s2sdk/tree/d4a8b46676ded25b9d956287ec40ef3c9e12c14c
  SDK-provided protobuf runtime is linked statically; its BSD license is included in THIRD_PARTY.
- nlohmann/json v3.12.0, Niels Lohmann and contributors, MIT:
  https://github.com/nlohmann/json/tree/v3.12.0

This release uses the unmodified CS2Menus public API. Only the MapTop HUD plugin is
built and deployed. No dependency plugin binary, Workshop asset, server credentials
or database contents are included in the release. Referenced public headers and
their license notices are included with the corresponding MapTop HUD source.
