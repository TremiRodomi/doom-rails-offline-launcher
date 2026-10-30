# Doom Rails offline launcher

An offline launcher for the GamersGate standalone download and Steam release of
**Doom Rails 1.1.0**.
Uses your existing game installation. No game executable, assets, or license keys
are distributed in this repository or its release downloads.

## Download and install

Download `Doom-Rails-offline-launcher.zip` from the
[latest release](https://github.com/TremiRodomi/doom-rails-offline-launcher/releases/latest)
and extract it into the game directory, beside the original `Doom Rails.exe`.

### Steam on Linux

1. Open **Doom Rails → Properties → Compatibility**. Enable **Force the use of a
   specific Steam Play compatibility tool** and select **Proton 10.0-4**.
2. Under **General → Launch Options**, enter the following, replacing the path
   with your actual game installation directory:

   ```text
   "/path/to/SteamLibrary/steamapps/common/Doom Rails/play-doom-rails-steam.sh" %command%
   ```

3. Press **Play** in Steam.

If your archive extractor does not preserve executable permissions, run
`chmod +x play-doom-rails-steam.sh` in the game directory. Running the script
without arguments prints the launch option for its current location.

The wrapper uses Steam's selected Proton runtime and existing game prefix.
It does not require this source checkout, Python, a compiler, or a separate
Wine prefix after installation. Keep the extracted launcher beside the game.

**Known compatibility issue:** Proton Experimental
`experimental-11.0-20261027b-x86_64` exits inside the game's `CDemo::CDemo()`
constructor before registration. Proton 10 was verified working with default
DXVK on the desktop and WineD3D in a virtual display. This finding is specific
to the tested runtimes; future Experimental versions may differ.

### Windows

Run `Play Doom Rails.exe` beside the original `Doom Rails.exe` each time.
Native Windows has not been tested.

### Command-line options

```text
"Play Doom Rails.exe" [--game-dir PATH] [--windowed] [--wait]
```

- `--game-dir PATH`: select a game directory other than the launcher's folder.
- `--windowed`: enable the game's per-user windowed setting.
- `--wait`: keep the launcher running until the game exits and return its exit code.

## Supported executables

The tested standalone copy came from the GamersGate download and is essentially
the same as the commonly circulated “demo” version. Other demo copies are
supported only if their executable matches the hash below.

Both tested builds display version 1.1.0 but have different executable layouts. Exact SHA-256
matching selects the patch profile; unknown builds are rejected before launch.

| Build | Original executable SHA-256 |
| --- | --- |
| GamersGate standalone / matching demo | `a09b550234339bac087768fc5f95e9a026ab8b23f4195c257f8242dc69aa339f` |
| Steam, build ID 21247 | `fbba9d10ff5da3eb006fe6f9ca0214d41bcfb42a343e2f81d99ad47deec6d8e9` |

## How it works

The helper starts the original executable and completes its registration dialog
in process memory. It then validates and changes two multiplayer startup branches
(Listen and Connect), checking both signatures before writing and verifying the
results. The standalone addresses are `0x4929e0` / `0x494d18`; the Steam addresses
are `0x494340` / `0x496678`.

The original executable remains unchanged. No activation server or key is needed
for this startup path. This does not restore the game's online services or prove
that every protected path is unrestricted.

## Verification and limitations

Local verification, October 2026:

- GamersGate standalone under Wine 11.0 Staging: title screen, selection, briefing, and active
  single-player gameplay. Trial-state samples stayed zero during an eight-minute
  monitored session; this was not a completed level or continuous driving test.
- Steam under Wine and Proton 10: rendered title/login screen, successful launcher
  exit, both multiplayer branch patches present, and trial-state flag zero.
- CLI integration: external game directory, wait-for-game lifetime, and game exit
  code propagation verified under Wine.
- Patch regression test: mismatched signatures leave both sites unchanged;
  successful changes are verified, protections restored, and repeat application works.

Joining another player, a complete network match, and native Windows remain
unverified. Local game copies, process dumps, screenshots, and Wine/Proton prefixes
are intentionally not part of this repository.

## Build from source

Requires GNU Make and the 32-bit MinGW-w64 GCC cross-compiler:

```sh
make
make test-build
make package
```

The helper is written to `build/Play Doom Rails.exe`. Packaging also requires
Python 3 and produces a standalone ZIP and SHA-256 checksum in `dist/`.

Run the isolated memory-patch regression test in a configured Wine environment:

```sh
wine build/test_multiplayer_patch.exe
```

The optional `tests/test_steam_launcher.c` integration harness needs your own
supported Steam executable. Pass the built helper and game directory as Windows
paths. It launches a test game, checks the patches and wait behavior, and terminates
**that test game** with exit code 42 to check exit-code forwarding.

## Uninstall

Clear Steam Launch Options and remove `Play Doom Rails.exe`,
`play-doom-rails-steam.sh`, and this README from the game folder. The original game
files do not need restoring. The optional windowed setting remains a normal game
preference.
