# mpkg

A small, Nix-inspired package manager written in C. Packages are installed into an immutable store and linked into a profile directory, keeping your system clean and making it easy to remove packages or garbage collect unused store entries.

## How it works

mpkg uses a store/profile separation inspired by the Nix package manager. Rather than installing packages directly into shared system directories like `/usr/bin`, mpkg extracts each package into its own isolated directory in the store, identified by a SHA256 hash of the package archive. Binaries are then symlinked into a profile directory that you add to your `PATH`.

This design has a few important properties. First, packages never interfere with each other since each lives in its own store path. Second, removing a package is instant — only the symlinks are removed from the profile, while the store entry remains untouched until you explicitly run garbage collection. Third, the SHA256-addressed store means the same package downloaded twice will land in the same store path, avoiding duplication.

- **Store** — each package is extracted into its own isolated directory under `~/.mpkg/store/<hash>-<name>-<version>/`
- **Profile** — binaries from installed packages are symlinked into `~/.mpkg/profiles/bin/`, which you add to your `PATH`
- **Database** — a SQLite database at `~/.mpkg/var/db/packages.db` tracks installed packages and their store paths
- **Config** — a config file at `~/.config/mpkg/config` points to the root directory, making it easy to relocate the store

## Directory structure
```
~/.mpkg/
    store/          # extracted packages, one directory per package
    profiles/
        bin/        # symlinks to package binaries
    var/
        db/         # sqlite package database
        log/        # logs

~/.config/mpkg/
    config          # contains root= path
```

## Building

mpkg is written in C and depends on a few libraries. On NixOS, a `shell.nix` is provided that sets up the full build environment automatically:
```bash
nix develop -f shell.nix
make
```

On other Linux distributions, install `libcurl`, `openssl`, `libarchive`, and `sqlite3` development headers, then:
```bash
make
```

## Usage

### Initialize
```bash
mpkg init
# or specify a custom root directory
mpkg init /path/to/root
```

This creates the full directory structure, initializes the SQLite package database, and automatically adds the profile bin directory to your shell's PATH by appending an export line to your shell's rc file (`.bashrc`, `.zshrc`, or `config.fish`). Run `source ~/.bashrc` or restart your shell after initializing.

By default mpkg initializes under `~/.mpkg` and stores its config at `~/.config/mpkg/config`. If a custom root is specified, the config file will point there instead, allowing multiple isolated mpkg environments on the same machine.

### Install a package
```bash
mpkg install path/to/package.mpkg
```

mpkg reads the manifest file, downloads or copies the package archive, verifies its SHA256 checksum, extracts it into the store, symlinks any binaries into the profile, and records the installation in the database. If the checksum doesn't match, installation is aborted. Packages can be fetched from the network or installed from a local file — see the `.mpkg file format` section below.

### Update a package
```bash
mpkg update path/to/package.mpkg
```

Removes the currently installed version from the profile and database, then installs the new version from the manifest. The old store entry is kept on disk until you run `mpkg gc`, following the same Nix-inspired philosophy as `remove` — nothing is deleted until you explicitly ask for it.

### List installed packages
```bash
mpkg list
```

Prints a table of all installed packages with their name, version, a short hash, store path, and installation timestamp.

### Remove a package
```bash
mpkg remove <package-name>
```

Removes the package's symlinks from the profile and removes it from the database. The store entry is kept on disk until you run `mpkg gc`. This makes remove instant and safe — if something goes wrong you can always run `mpkg gc` selectively.

### Garbage collect
```bash
mpkg gc
```

Walks the store directory and deletes any entries not referenced by the database. This is the only operation that actually frees disk space. The separation between `remove` and `gc` is intentional — it mirrors Nix's approach and gives you a window to recover a removed package before it's gone for good.

### Help
```bash
mpkg help
```

## Demo
```
$ mpkg init
Initialized mpkg under /home/user/.mpkg
Config file stored under /home/user/.config/mpkg

$ mpkg install hello.mpkg
Parsing...
Fetching...
Fetched.
Checksumming...
Checksum OK.
Storing...

$ mpkg list
Name                 Version    Hash         Install Path                                  Installed At
----                 -------    ----         ------------                                  ------------
hello                1.0        21eec261fb   /home/user/.mpkg/store/21eec261f-hello-1.0   2026-03-25 22:05:45

$ hello
Hello, world!

$ mpkg update hello-2.0.mpkg
Removing old package version from profile....
Installing the new package version...

$ mpkg remove hello
Removed 'hello' from profile
Run 'mpkg gc' to free disk space

$ mpkg gc
Removing /home/user/.mpkg/store/21eec261f-hello-1.0
```

## .mpkg file format

Packages are described by a simple `key=value` manifest file with four fields:
```
name=hello
version=1.0
url=https://example.com/hello-1.0.tar.gz
sha256sum=abc123...
```

The `url` field supports both remote and local sources:

- **HTTPS** — package is downloaded from the network:
```
  url=https://example.com/hello-1.0.tar.gz
```
- **Local file** — package is loaded from the local filesystem, useful for testing or offline installation:
```
  url=file:///home/user/packages/hello-1.0.tar.gz
```

The archive is expected to contain a `bin/` directory with the package's executables. Top-level directories inside the archive are automatically stripped during extraction, so standard release tarballs work without any repacking.

## Dependencies

- libcurl — downloading packages from the network
- openssl — SHA256 checksum verification
- libarchive — extracting archives (tar.gz, tar.xz, zip, and more)
- sqlite3 — package database

## License

GPL v3