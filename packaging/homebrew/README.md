# Homebrew Packaging Draft

This directory contains draft Homebrew packaging files that can be copied to a separate tap repository.

Recommended tap layout:

```text
homebrew-therion-studio/
  Formula/
    therion-studio.rb
```

Suggested user-facing install flow after publishing the tap:

```sh
brew install ladislavb/therion-studio/therion-studio
```

or:

```sh
brew tap ladislavb/therion-studio
brew install therion-studio
```

The draft formula builds from source and installs `TherionStudio.app` into the Homebrew prefix. It declares Homebrew Qt dependencies and intentionally does not bundle Qt frameworks into the app bundle.

Before publishing:

- replace the GitHub owner/repository placeholders if needed
- replace `REPLACE_WITH_SOURCE_TARBALL_SHA256` with the release tarball checksum
- verify the `GPL-3.0-or-later` license metadata
- tag the source release, e.g. `v0.1.0`
- run `brew audit --strict --online --new --formula therion-studio`
- run `brew install --build-from-source ./Formula/therion-studio.rb`
