{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell rec {

  shellHook=''
  export PKG_CONFIG_PATH="${pkgs.glib.dev}/lib/pkgconfig:$PKG_CONFIG_PATH";
  export PKG_CONFIG_PATH="${pkgs.poppler_gi.dev}/lib/pkgconfig:$PKG_CONFIG_PATH";
  export PKG_CONFIG_PATH="${pkgs.SDL.dev}/lib/pkgconfig:$PKG_CONFIG_PATH";
  export PKG_CONFIG_PATH="${pkgs.cairo.dev}/lib/pkgconfig:$PKG_CONFIG_PATH";
  '';

}
