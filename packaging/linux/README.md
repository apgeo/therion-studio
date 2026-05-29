# Linux Packaging

Therion Studio currently provides one Linux preview/tester artifact:

- `.deb` package for Debian/Ubuntu-family systems

It is built by the manual GitHub Actions workflow:

- `.github/workflows/linux-packages.yml`

## Workflow Inputs

- `source_ref`: branch/tag/commit to package
- `build_type`: `Release` or `RelWithDebInfo`

## Produced Artifacts

- `therion-studio-<package_label>-linux-x86_64.deb`
- `TherionStudio-Linux-artifacts-manifest.json`

The manifest includes file names, sizes, and SHA256 checksums.

After artifact creation, the workflow runs a Debian/Ubuntu forward-compatibility smoke check:

- launches an `ubuntu:26.04` container
- installs the generated `.deb`
- verifies expected installed paths
- performs an offscreen `.deb` launch sanity check

## Build Notes

- `.deb` generation uses CPack DEB configuration from `CMakeLists.txt`.
- The workflow runs install-layout smoke verification using
  `scripts/verify_install_layout.py` before packaging.
- Artifact naming and manifest generation are validated by
  `scripts/verify_linux_release_artifacts.py`.
- AppImage generation is deferred. Do not build production Linux artifacts from mutable
  `linuxdeployqt` `continuous` downloads or unmaintained Qt deployment plugins.

## Portable Linux Artifact Gap

The specification still requires production Linux releases to provide at least one broadly
portable package format. The current `.deb` workflow is suitable for Debian/Ubuntu preview
testing, but it does not close that production-release requirement. Candidate replacement
paths should be evaluated for maintenance status, reproducibility, runtime dependency
coverage, and CI smoke-testability before they are added back to release automation.
