import sys
import json
import os
import hashlib
import zlib

if len(sys.argv) < 3:
    print("Usage: python update_manifest.py <version> <filename>")
    sys.exit(1)

version = sys.argv[1]
filename = sys.argv[2]
filepath = os.path.join("deploy_repo", filename)
manifest_path = os.path.join("deploy_repo", "manifest.json")

size = os.path.getsize(filepath)

with open(filepath, "rb") as f:
    data = f.read()
    sha256 = hashlib.sha256(data).hexdigest()
    crc32_val = zlib.crc32(data) & 0xFFFFFFFF
    crc32_hex = f"{crc32_val:08x}"

with open(manifest_path, "r") as f:
    manifest = json.load(f)

manifest["version"] = version
manifest["file_size"] = size
manifest["image"] = "/" + filename
manifest["sha256"] = sha256
manifest["crc32"] = crc32_hex

with open(manifest_path, "w") as f:
    json.dump(manifest, f, indent=4)

print(f"Manifest automatically updated to v{version}!")
print(f"SHA256: {sha256}")
print(f"CRC32:  {crc32_hex}")
