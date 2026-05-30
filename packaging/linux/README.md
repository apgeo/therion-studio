# Linux Packaging

Therion Studio currently provides two Linux artifacts:

- `.deb` package for Debian/Ubuntu-family systems
- AppImage as the portable Linux channel

They are built by the manual GitHub Actions workflow:

- `.github/workflows/linux-packages.yml`

## Workflow Inputs

- `source_ref`: branch/tag/commit to package
- `build_type`: `Release` or `RelWithDebInfo`

## Produced Artifacts

- `therion-studio-<package_label>-linux-x86_64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-Linux-artifacts-manifest.json`

The manifest includes file names, sizes, and SHA256 checksums.

The `.deb` artifact intentionally uses a distro-neutral `linux-x86_64` suffix. Do not add an
`ubuntu` suffix unless the package becomes tied to a specific Ubuntu release baseline.

After artifact creation, the workflow runs Debian/Ubuntu forward-compatibility smoke checks:

- launches an `ubuntu:26.04` container
- launches a `debian:13` container
- installs the generated `.deb`
- verifies expected installed paths
- performs an offscreen `.deb` launch sanity check
- performs an offscreen AppImage launch sanity check

Ubuntu 26.04 and Debian 13 are considered tested `.deb` targets for a given artifact only when the
corresponding workflow smoke jobs pass.

## Build Notes

- `.deb` generation uses CPack DEB configuration from `CMakeLists.txt`.
- AppImage generation uses a separate CMake build with `THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON`
  so Qt's generated Linux deployment script populates the AppDir during install.
- The final AppImage is produced with pinned `appimagetool` 1.9.1 and pinned
  `type2-runtime` 20251108 `runtime-x86_64`, both SHA256-verified by the workflow before use.
- The workflow runs install-layout smoke verification using
  `scripts/verify_install_layout.py` before packaging.
- Artifact naming and manifest generation are validated by
  `scripts/verify_linux_release_artifacts.py`.
- Do not build production Linux artifacts from mutable
  `linuxdeployqt` `continuous` downloads or unmaintained Qt deployment plugins.
