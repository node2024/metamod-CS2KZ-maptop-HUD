"""Package the release files explicitly, including corresponding source and notices."""
import argparse
import hashlib
import json
import pathlib
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("binary", type=pathlib.Path)
parser.add_argument("--out", type=pathlib.Path, default=root / "release" / "KZ-MapTop-HUD-0.1.15-linux.zip")
args = parser.parse_args()
binary = args.binary.read_bytes()
if binary[:5] != b"\x7fELF\x02" or int.from_bytes(binary[18:20], "little") != 62:
    raise SystemExit("Expected a Linux x86_64 ELF binary")

payload = {}
deploy_files = (
    "addons/metamod/kz_maptop_hud.vdf",
    "addons/kz_maptop_hud/config.json",
)
for name in deploy_files:
    payload["game/csgo/" + name] = (root / "deploy" / name).read_bytes()
payload["game/csgo/addons/kz_maptop_hud/bin/linuxsteamrt64/kz_maptop_hud.so"] = binary
for name in ("README.md", "README-ja.md", "LICENSE", "NOTICE.md"):
    payload[name] = (root / name).read_bytes()
source_root = "source/kz-maptop-hud/"
for name in ("CMakeLists.txt", "bootstrap.sh", ".gitignore", "README.md", "README-ja.md", "LICENSE", "NOTICE.md"):
    payload[source_root + name] = (root / name).read_bytes()
for name in deploy_files:
    payload[source_root + "deploy/" + name] = (root / "deploy" / name).read_bytes()
payload[source_root + "tools/package.py"] = (root / "tools/package.py").read_bytes()
for pattern in ("src/*.cpp", "src/*.h", "tests/*_test.cpp", "tests/*_test.py",
                "deps/api/*.h", "deps/json/nlohmann/json.hpp", "THIRD_PARTY/*-LICENSE"):
    for path in sorted(root.glob(pattern)):
        if path.is_file():
            payload[source_root + path.relative_to(root).as_posix()] = path.read_bytes()
for path in sorted((root / "THIRD_PARTY").glob("*-LICENSE")):
    if path.is_file():
        payload["THIRD_PARTY/" + path.name] = path.read_bytes()
manifest = {
    "name": "KZ MapTop HUD", "version": "0.1.15", "date_jst": "2026-10-29",
    "platform": "linux-x86_64", "metamod_api": 18,
    "display_modes": ["studio", "table"], "default_display_mode": "studio",
    "records_per_page": {"studio": 5, "table": 15},
    "cs2kz_version": "v0.0.189", "cs2kz_interface": "ICS2KZ002", "cs2menus_interface": "ICS2Menus008",
    "config_source": "cfg/cs2kz-server-config.txt",
    "sdk_revision": "d4a8b46676ded25b9d956287ec40ef3c9e12c14c",
    "metamod_revision": "170058a2f0dbd58106b7c76bfe88a94672295356",
    "build_environment": "Steam Runtime Sniper SDK / GCC 10.3",
    "in_game_validation": "not performed",
    "binary_sha256": hashlib.sha256(binary).hexdigest(),
    "cs2menus_changes": False,
    "workshop_changes": False,
}
payload["release-manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
payload["SHA256SUMS.txt"] = "".join(
    f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(payload.items())
).encode()
args.out.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, data in sorted(payload.items()):
        entry = zipfile.ZipInfo(name, date_time=(2026, 10, 29, 0, 0, 0))
        entry.create_system = 3
        entry.compress_type = zipfile.ZIP_DEFLATED
        entry.external_attr = (0o100755 if name.endswith((".so", ".sh")) else 0o100644) << 16
        archive.writestr(entry, data)
checksum = args.out.with_name(args.out.name + ".sha256")
checksum.write_bytes(
    f"{hashlib.sha256(args.out.read_bytes()).hexdigest()}  {args.out.name}\n".encode("utf-8")
)
print(f"Created {args.out}: {len(payload)} files, {args.out.stat().st_size} bytes")
print(f"Created {checksum}")
