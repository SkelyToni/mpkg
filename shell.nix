{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    gnumake
    sqlite
    curl
    openssl
    libarchive
    pkg-config
  ];

  shellHook = ''
    echo "mpkg dev shell"
    echo "Build with: make"
    export PATH="$HOME/Development/mpkg:$PATH"
    export PATH="$HOME/.mpkg/profiles/bin:$PATH"
  '';
}