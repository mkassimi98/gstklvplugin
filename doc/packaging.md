# Packaging And Distribution

This document describes the distribution workflow for `gstklvplugin` when you
want to hand the project to third parties as an installable package instead of
asking them to build from a source checkout.

The current repository ships a Debian-family packaging helper. That makes it a
good fit for:

- Debian
- Ubuntu
- Raspberry Pi OS
- other Debian-derived ARM and x86 Linux distributions

---

## 1. What Is Included

The packaged runtime currently contains:

- `gstklvplugin.so`
- `stanag4609_tags.ini`
- basic project documentation under `/usr/share/doc/gstklvplugin/`

The package is built from the Meson install layout, so the runtime contents
match the normal system installation path.

---

## 2. Build A `.deb` Package

From the repository root:

```bash
./packaging/deb/build_deb.sh
```

The resulting package is written to:

```text
packaging/out/
```

Typical filename:

```text
packaging/out/gstklvplugin_1.0.0_amd64.deb
```

With validation enabled:

```bash
./packaging/deb/build_deb.sh --run-checks
```

If you want to skip Doxygen during that validation pass:

```bash
./packaging/deb/build_deb.sh --run-checks --skip-doxygen
```

---

## 3. Install The Package

On Debian-family systems:

```bash
sudo apt install ./packaging/out/gstklvplugin_1.0.0_amd64.deb
```

Or with `dpkg`:

```bash
sudo dpkg -i ./packaging/out/gstklvplugin_1.0.0_amd64.deb
```

Then verify:

```bash
gst-inspect-1.0 --plugin klvplugin
gst-inspect-1.0 --exists klvmetaenc
```

---

## 4. Raspberry Pi Notes

For a native build on a Raspberry Pi, no project-side Meson changes are needed.

That is because:

- Meson already installs the plugin under `libdir/gstreamer-1.0`
- on Debian-family ARM systems, `libdir` resolves to the correct multiarch path
- the package script reads the Debian architecture dynamically

So the same command works on Raspberry Pi OS:

```bash
./packaging/deb/build_deb.sh
```

The generated package architecture depends on the OS image:

- Raspberry Pi OS 32-bit: typically `armhf`
- Raspberry Pi OS 64-bit: typically `arm64`

The CMake install path was also updated to use `${CMAKE_INSTALL_LIBDIR}`, so
manual CMake installs now follow the correct multiarch path on ARM as well.

Important distinction:

- native build on the Raspberry Pi: supported directly
- cross-compiling from another machine to Raspberry Pi: possible, but that
  still needs a target sysroot and a Meson cross file that this repo does not
  currently ship

---

## 5. Package Metadata

The Debian package helper:

- detects the architecture with `dpkg-architecture`
- computes shared-library dependencies with `dpkg-shlibdeps`
- stages the Meson install tree before building the final `.deb`

That means the output package is architecture-aware and suitable for the
machine that built it, including ARM boards.

---

## 6. Related Documentation

- [doc/installation.md](installation.md) — manual and automatic installation workflows
- [doc/tests.md](tests.md) — unit and smoke tests
- `README.md` (repository root) — top-level project overview
