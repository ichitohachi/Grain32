# Grain32

CPU film grain for Adobe After Effects, with 8/16/32-bpc processing, animated grain, and an editable response curve.

**Version 0.15.2 — beta testing.** [日本語](README.ja.md) · [Downloads](https://github.com/ichitohachi/Grain32/releases) · [Report a bug](https://github.com/ichitohachi/Grain32/issues/new/choose)

Grain32 is an independent effect. It is not affiliated with Adobe or Element Supply, and does not promise pixel-identical results to Fast Grain.

## Download and install

Open **Releases** and download your OS package from **Assets**. The automatically generated **Source code** archives are for developers, not installable plugins. See the release validation notes for the platforms actually tested.

Save your project and quit AE before installation. Back up any older Grain32 outside all plugin folders and keep only one installed copy. Test with a copy of your project.

### macOS

1. Unzip `Grain32-v0.15.2-macOS-universal.zip`.
2. Copy the entire `Grain32.plugin` bundle into a `Grain32` folder under:

   ```text
   /Applications/Adobe After Effects 2026/Plug-ins/
   ```

3. Restart AE. Find **Grain32** in **Effect > Noise & Grain**, or search Effects & Presets.

The binary contains Apple Silicon and Intel code. The build deployment target is macOS 12; AE itself may require a newer OS. Intel execution needs beta testing. This beta is ad-hoc signed, not Apple-notarized. If macOS blocks loading, verify the download came from this repository and follow the macOS Privacy & Security approval flow. Report the exact message if approval fails; do not disable system-wide security protections.

### Windows

1. Unzip `Grain32-v0.15.2-Windows-x64.zip`.
2. Copy `Grain32.aex` into a `Grain32` folder under:

   ```text
   C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\
   ```

3. Restart AE and find **Grain32** in **Effect > Noise & Grain**.

Copying into Program Files may need administrator permission. This build targets Windows x64, not ARM64, and is not Authenticode-signed. Windows AE host testing is requested from beta testers.

For AE 2025, use its corresponding application folder. AE 2026 on Apple Silicon is the development host; other combinations require testing. To uninstall, quit AE and remove only the Grain32 bundle/file you installed.

## Controls

| Control | Default / range | Behavior |
| --- | --- | --- |
| Intensity | 22 / 0–1000 | Grain strength. Above 100 increases noise amplitude without extrapolating the whole source image. |
| Size | 1.25 / 0.5–5 | Grain scale. Compare at Full resolution and the same viewer zoom. |
| Softness | 0 / 0–100 | Smooths grain only. Nonzero values require more processing. |
| Grain Color | 18% / 0–100% | Blends monochrome and color grain. |
| Frame Rate | 24 / 0–30 | Updates per second at Speed 1. Zero freezes grain. |
| Animation Speed | 1 / 0–10 | Update-rate multiplier: 0 freezes, 0.5 is half speed, 2 is double speed. |
| Grain Response | Editable curve | Controls grain using the selected input range. |
| Range Mode | Luminance | Luminance, Lightness, Hue, Saturation, Red, Green, Blue or Alpha. |
| Invert Range | Off | Inverts the response. |
| Response Opacity | Off | Off: curve controls noise amplitude. On: curve controls blend coverage. |
| Blend Mode | Overlay | Combines the grain texture with the source. |
| Blend Opacity | 100% | Overall blend amount. Zero returns the source. |

### Curve editing

Expand **Response > Grain Response**. Drag the four range boundaries to adjust the plateau and falloffs. Drag the center plateau handle horizontally to move it, or vertically to change its height. Drag the square Bézier handles to shape the falloffs; cyan lines connect them to their endpoints. The histogram shows the selected input range.

The default response is full strength from 0 to 0.25, then a linear falloff to zero at 1. **Reset Curve** restores boundaries, height, Bézier handles and Invert Range. It keeps Range Mode, Response Opacity and other grain controls. On animated controls it changes values at the current time, rather than deleting all keyframes.

### Blend and animation notes

Normal at Intensity 100 and Blend Opacity 100% displays the grain texture. With Response Opacity off, zero-response areas approach neutral gray, rather than revealing the source. Difference calculates the absolute per-channel difference between source and grain. Different modes produce different highlight behavior.

32-bpc processing does not globally clamp input RGB to 0–1. This retains extended-range inputs for processing; it does not mean every mode preserves original brightness. Integer output must fit its pixel format.

Animation Speed changes update frequency without interpolation between patterns. Keyframed speed uses current time multiplied by current speed, not an accumulated speed integral, and may cause phase jumps. Version 0.15.2 fixes AE's time-dependency declaration so grain updates on still footage with constant parameters.

## Beta testing

Start with [the checklist](docs/TESTING.md). Include plugin version, AE version, OS/CPU, color settings, bit depth, exact parameters and reproduction steps in an issue. English and Japanese reports are welcome. Share only material you have permission to disclose.

Multi-Frame Rendering support is implemented. Automated tests check deterministic serial/parallel core processing; they do not certify AE host stability. Please test MFR on and off, especially on Windows. Future versions may change rendered results; retain the version used for existing projects.

## Build from source

Obtain the [Adobe After Effects SDK](https://developer.adobe.com/after-effects/) separately. SDK files are not included. Release builds target SDK 25.6 (61). Set `AE_SDK_ROOT` to the folder containing `Examples`.

macOS requires Apple's command-line build tools, including `clang++` and `Rez`:

```sh
export AE_SDK_ROOT="/path/to/ae-sdk"
bash scripts/build_mac.sh
clang++ -std=c++17 -O2 -Wall -Wextra -Werror tests/grain_core_tests.cpp -o /tmp/grain32-tests
/tmp/grain32-tests
```

Windows requires Visual Studio 2022 C++ build tools and Windows SDK. In an **x64 developer PowerShell**:

```powershell
$env:AE_SDK_ROOT = 'C:\path\to\ae-sdk'
.\scripts\build_windows.ps1
```

Output is `dist/Grain32.plugin` or `dist/Grain32.aex`. Both use the same C++ source. See release notes for build verification. Adobe SDK terms apply separately; this repository does not grant permission to redistribute the SDK.
