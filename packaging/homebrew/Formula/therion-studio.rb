# Draft Homebrew formula for a future external tap, e.g.
# https://github.com/ladislavb/homebrew-therion-studio
#
# Before publishing:
# - replace homepage/url with the final public repository URL
# - replace sha256 with: curl -L <url> | shasum -a 256

class TherionStudio < Formula
  desc "Qt desktop editor for Therion cave-survey projects"
  homepage "https://github.com/ladislavb/therion-studio"
  url "https://github.com/ladislavb/therion-studio/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "REPLACE_WITH_SOURCE_TARBALL_SHA256"
  license "GPL-3.0-or-later"

  depends_on "cmake" => :build
  depends_on "pkgconf" => :build
  depends_on "qtbase"
  depends_on "qtsvg"
  depends_on macos: :monterey

  def install
    qt_prefixes = [Formula["qtbase"].opt_prefix, Formula["qtsvg"].opt_prefix].join(";")

    system "cmake", "-S", ".", "-B", "build",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DCMAKE_PREFIX_PATH=#{qt_prefixes}",
           *std_cmake_args
    system "cmake", "--build", "build", "--target", "TherionStudio"
    system "cmake", "--install", "build"

    (bin/"therion-studio").write <<~SH
      #!/bin/sh
      exec "#{opt_prefix}/TherionStudio.app/Contents/MacOS/TherionStudio" "$@"
    SH
    chmod 0755, bin/"therion-studio"
  end

  def caveats
    <<~EOS
      Therion Studio.app was installed to:
        #{opt_prefix}/TherionStudio.app

      To expose it in Finder/Launchpad-style workflows, create a symlink:
        ln -sfn "#{opt_prefix}/TherionStudio.app" "/Applications/Therion Studio.app"

      The external Therion command-line executable is not bundled.
      Install it separately if needed:
        brew install therion
    EOS
  end

  test do
    assert_path_exists prefix/"TherionStudio.app/Contents/MacOS/TherionStudio"
    bundle_id = shell_output("/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' #{prefix}/TherionStudio.app/Contents/Info.plist").strip
    assert_equal "com.lblazek.therionstudio", bundle_id
  end
end
