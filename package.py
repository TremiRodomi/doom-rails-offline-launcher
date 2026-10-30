"""Package only the launcher and user-facing instructions."""
from pathlib import Path
import hashlib
import zipfile

root = Path(__file__).resolve().parent
output = root / "dist"
output.mkdir(exist_ok=True)
archive = output / "Doom-Rails-offline-launcher.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
    for source, name in [
        (root / "build/Play Doom Rails.exe", "Play Doom Rails.exe"),
        (root / "play-doom-rails-steam.sh", "play-doom-rails-steam.sh"),
        (root / "README.md", "README.md"),
    ]:
        bundle.write(source, name)
(output / "SHA256SUMS.txt").write_text(
    hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n"
)
print(archive)
