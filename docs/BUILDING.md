# Release maintenance

Both platforms build from the same source revision. Keep `PF_VERSION`, the PiPL integer version, Info.plist and package names in sync.

The manual **Windows beta build** workflow requires repository secret `AE_SDK_WINDOWS_URL`: a current signed download URL obtained from Adobe's SDK page for SDK 25.6 (61), Windows. It checks the archive's SHA-256 before using the extraction tools bundled by Adobe. The URL expires; refresh the secret when needed. Do not put it in commits, workflow inputs or logs. The SDK is extracted only into the ephemeral runner directory and is never uploaded as an artifact.

Only the plugin package is uploaded. A successful workflow confirms compilation and core tests, not AE host operation. Run the host checklist separately. Before publishing a release, download and inspect both OS packages, verify exports/architecture/version, attach SHA256SUMS, and record the source commit and test status.
