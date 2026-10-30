#!/usr/bin/env bash
# Steam launch options: "/absolute/path/play-doom-rails-steam.sh" %command%
set -euo pipefail
launcher_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
helper="$launcher_root/Play Doom Rails.exe"
if [[ ! -f "$helper" ]]; then
    printf 'Missing launcher: %s\n' "$helper" >&2
    exit 1
fi

if (( $# == 0 )); then
    printf 'Set Steam → Doom Rails → Properties → Launch Options to:\n\n"%s/play-doom-rails-steam.sh" %%command%%\n\nThen launch the game from Steam.\n' "$launcher_root"
    exit 0
fi

# Preserve Steam's selected Proton/runtime and all argument boundaries.
command_args=("$@")
found=0
for i in "${!command_args[@]}"; do
    candidate="${command_args[i]}"
    if [[ "${candidate##*/}" == 'Doom Rails.exe' && -f "$candidate" ]]; then
        game_dir="$(cd -- "$(dirname -- "$candidate")" && pwd)"
        command_args[i]="$helper"
        found=$((found + 1))
    fi
done
if (( found != 1 )); then
    printf 'Expected one Doom Rails.exe in Steam command; found %s. Use this script followed by %%command%%.\n' "$found" >&2
    exit 2
fi
exec "${command_args[@]}" --game-dir "Z:$game_dir" --windowed --wait
