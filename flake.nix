{
  description = "Standalone AC-3 / dependent E-AC-3 encoder with channel-aware layouts";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;

      mkPackage = pkgs: withSndfile: pkgs.stdenv.mkDerivation {
        pname = if withSndfile then "eac371enc" else "eac371enc-raw";
        version = "0.1.6-TEST";
        src = ./.;

        nativeBuildInputs = with pkgs;
          [ cmake python3 ]
          ++ pkgs.lib.optionals withSndfile [ pkg-config ];

        buildInputs = pkgs.lib.optionals withSndfile [ pkgs.libsndfile ];

        cmakeFlags = [
          "-DEAC371_ENABLE_SNDFILE_INPUT=${if withSndfile then "ON" else "OFF"}"
          "-DEAC371_BUILD_TESTS=ON"
        ];

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          ctest --output-on-failure
          runHook postCheck
        '';

        meta = with pkgs.lib; {
          description = "Encode <=5.1 to AC-3 and 6.0-7.1 to AC-3 core plus E-AC-3 extension";
          license = licenses.lgpl21Plus;
          platforms = systems;
          mainProgram = "eac371enc";
        };
      };
    in {
      packages = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; }; in {
          default = mkPackage pkgs true;
          raw-pcm = mkPackage pkgs false;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/eac371enc";
        };
      });

      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; }; in {
          default = pkgs.mkShell {
            packages = with pkgs; [ cmake pkg-config python3 libsndfile ];
          };
        });
    };
}
