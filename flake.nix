# Mostly taken from https://github.com/NixOS/nixpkgs/blob/nixos-24.11/pkgs/by-name/sw/swayfx-unwrapped/package.nix
{
  description = "scroll flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        wlroots_0_19 = pkgs.wlroots.overrideAttrs (old: {
          version = "0.19.0";
          src = pkgs.fetchFromGitLab {
            domain = "gitlab.freedesktop.org";
            owner = "wlroots";
            repo = "wlroots";
            rev = "0.19.0-rc3";
            hash = "sha256-ZqchXPR/yfkAGwiY9oigif0Ef4OijHcGPGUXfHaL5v8=";
          };
        });
        scroll = pkgs.stdenv.mkDerivation {
          name = "scroll";
          # version = /* -- */
          src = ./.;

          # I'm not too sure what these do, in all honesty
          strictDeps = true;
          depsBuildBuild = with pkgs; [ pkg-config ];

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            scdoc
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            cairo
            gdk-pixbuf
            json_c
            libdrm
            libevdev
            libGL
            libinput
            librsvg
            libxkbcommon
            pango
            pcre2
            scenefx
            wayland
            wayland-protocols

            # In swayfx these 2 are conditionally added
            # Here I'll simply add them anyway
            (wlroots_0_19.override { enableXWayland = true; })
            # Interesting how it doesn't have the xorg in the swayfx package.
            # I wonder why
            xorg.xcbutilwm
          ];

          mesonFlags = [
            # It complained about the auto
            "-Dsd-bus-provider=libsystemd"
          ];
        };
      in
      {
        packages = {
          inherit scroll;
          default = self.packages.${system}.scroll;
        };
      }
    );
}
