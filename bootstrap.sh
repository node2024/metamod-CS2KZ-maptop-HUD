#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
sdk_revision=d4a8b46676ded25b9d956287ec40ef3c9e12c14c
mms_revision=170058a2f0dbd58106b7c76bfe88a94672295356
utils_revision=6d659b448ad283cb3cb794ffec45b5d85c2ede78
kz_revision=da535d95714b40de625134973a6effe5c20325e4
sql_revision=78e1e2c5bb59ddbac44c16b935edeca346e7fca5
fetch_checkout() {
  local url="$1" target="$2" revision="$3"
  if [[ ! -d "$target/.git" ]]; then
    mkdir -p "$target"
    git -C "$target" init
    git -C "$target" remote add origin "$url"
  fi
  git -C "$target" fetch --depth 1 origin "$revision"
  git -C "$target" checkout --detach "$revision"
}
fetch_checkout https://github.com/alliedmodders/s2sdk.git deps/s2sdk "$sdk_revision"
fetch_checkout https://github.com/alliedmodders/metamod-source.git deps/metamod-source "$mms_revision"
git -C deps/metamod-source submodule update --init --depth 1 third_party/khook
mkdir -p deps/api deps/json/nlohmann
curl -fsSL "https://raw.githubusercontent.com/FemboyKZ/mm-utils/$utils_revision/interfaces/cs2menus/ics2menus.h" -o deps/api/ics2menus.h
curl -fsSL "https://raw.githubusercontent.com/KZGlobalTeam/cs2kz-metamod/$kz_revision/src/public/ics2kz.h" -o deps/api/ics2kz.h
for header in sql_mm.h mysql_mm.h sqlite_mm.h; do
  curl -fsSL "https://raw.githubusercontent.com/zer0k-z/sql_mm/$sql_revision/src/public/$header" -o "deps/api/$header"
done
curl -fsSL https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp -o deps/json/nlohmann/json.hpp
printf '%s\n' 'aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63  deps/json/nlohmann/json.hpp' | sha256sum -c -
