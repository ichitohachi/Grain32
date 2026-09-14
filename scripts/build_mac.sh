#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
sdk_dir="${AE_SDK_ROOT:-$project_dir/sdk}"
if [[ ! -f "$sdk_dir/Examples/Headers/AE_Effect.h" ]]; then
    echo 'Set AE_SDK_ROOT to the extracted Adobe SDK folder.' >&2
    exit 1
fi
bundle="$project_dir/dist/Grain32.plugin"
mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
includes=(-I "$sdk_dir/Examples/Headers" -I "$sdk_dir/Examples/Headers/SP" -I "$sdk_dir/Examples/Util" -I "$sdk_dir/Examples/Resources")
xcrun clang++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-multichar \
    -Wno-missing-field-initializers \
    -fvisibility=hidden -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 -bundle \
    "${includes[@]}" "$project_dir/src/Grain32.cpp" "$project_dir/src/Grain32_UI.cpp" \
    -o "$bundle/Contents/MacOS/Grain32"
xcrun Rez -d __MACH__ -d __aarch64__ "${includes[@]}" \
    "$project_dir/resources/Grain32PiPL.r" -useDF -o "$bundle/Contents/Resources/Grain32.rsrc"
cp "$project_dir/resources/Info.plist" "$bundle/Contents/Info.plist"
printf 'eFKTFXTC' > "$bundle/Contents/PkgInfo"
codesign --force --sign - "$bundle"
codesign --verify --strict "$bundle"
echo "Built: $bundle"
