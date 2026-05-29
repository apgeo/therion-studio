# Linux Packaging

Therion Studio provides two Linux artifact forms:

- `.deb` package as the primary installer channel
- `AppImage` as a portable supplemental channel

Both are built by the manual GitHub Actions workflow:

- `.github/workflows/linux-packages.yml`

## Workflow Inputs

- `source_ref`: branch/tag/commit to package
- `build_type`: `Release` or `RelWithDebInfo`

## Produced Artifacts

- `therion-studio-<package_label>-linux-x86_64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-Linux-artifacts-manifest.json`

The manifest includes file names, sizes, and SHA256 checksums.

After artifact creation, the workflow runs a Debian/Ubuntu forward-compatibility smoke check:

- launches an `ubuntu:26.04` container
- installs the generated `.deb`
- verifies expected installed paths
- performs an offscreen `.deb` launch sanity check
- performs an offscreen `AppImage` launch sanity check

## Build Notes

- `.deb` generation uses CPack DEB configuration from `CMakeLists.txt`.
- AppImage generation uses `linuxdeployqt` in AppDir mode from a staged install tree.
- The workflow runs install-layout smoke verification using
  `scripts/verify_install_layout.py` before packaging.
- Artifact naming and manifest generation are validated by
  `scripts/verify_linux_release_artifacts.py`.
