# mpkg

A small, Nix-inspired package manager written in C. Packages are installed into an immutable store and linked into a profile directory, keeping your system clean and making it easy to remove packages or garbage collect unused store entries.

## How it works

mpkg uses a store/profile separation inspired by the Nix package manager:

- **Store** — each package is extracted into its own isolated directory under `~/.mpkg/store/<hash>-<name>-<version>/`
- **Profile** — binaries from installed packages are symlinked into `~/.mpkg/profiles/bin/`, which you add to your `PATH`
- **Database** — a SQLite database at `~/.mpkg/var/db/packages.db` tracks installed packages
- **Config** — a config file at `~/.config/mpkg/config` points to the root directory

This means removing a package only removes its symlinks from the profile — the store entry stays until you run `mpkg gc`, which deletes any store entries not referenced by the database.

## Directory structure
```
~/.mpkg/
    store/          # extracted packages
    profiles/
        bin/        # symlinks to package binaries
    var/
        db/         # sqlite package database
        log/        # logs

~/.config/mpkg/
    config          # contains root= path
```

## Building

Enter the dev shell and build:
```bash
nix develop -f shell.nix
make
```

## Usage

### Initialize
```bash
mpkg init
# or specify a custom root directory
mpkg init /path/to/root
```

This creates the directory structure, initializes the database, and adds `~/.mpkg/profiles/bin` to your shell's PATH.

### Install a package
```bash
mpkg install path/to/package.mpkg
```

Packages can be fetched from the network or installed from a local file — see the `.mpkg file format` section below.

### List installed packages
```bash
mpkg list
```

### Remove a package
```bash
mpkg remove <package-name>
```

This removes the package's symlinks from the profile and removes it from the database. The store entry is kept until you run `gc`.

### Garbage collect
```bash
mpkg gc
```

Deletes any store entries not referenced by the database, freeing disk space.

### Help
```bash
mpkg help
```

## .mpkg file format

Packages are described by a simple key=value file:
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
- **Local file** — package is loaded from the local filesystem:
```
  url=file:///home/user/packages/hello-1.0.tar.gz
```

The archive is expected to contain a `bin/` directory with the package's executables. Top-level directories inside the archive are automatically stripped during extraction.

## Dependencies

- libcurl — downloading packages
- openssl — SHA256 checksum verification
- libarchive — extracting archives (tar.gz, tar.xz, zip, etc.)
- sqlite3 — package database

## License

GPL v3