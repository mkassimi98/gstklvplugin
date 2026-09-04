# Release Workflow

This project uses a small GitFlow model. Releases are immutable Git tags;
version numbers are not maintained as long-lived branches.

## Branch Roles

| Branch | Purpose |
|---|---|
| `main` | Stable code matching the latest published release |
| `devel` | Integration branch for the next release |
| `feature/*` | New work created from `devel` |
| `fix/*` | Non-urgent fixes created from `devel` |
| `release/X.Y.Z` | Temporary release preparation created from `devel` |
| `hotfix/*` | Urgent production fixes created from `main` |

`main` is the default GitHub branch. Feature and fix branches are deleted after
their pull requests merge. Release state is identified by annotated tags such
as `v1.0.1`, never by branches named after versions.

## Feature And Fix Workflow

Create work from the latest `devel`:

```bash
git switch devel
git pull --ff-only origin devel
git switch -c fix/short-description
```

Run `./scripts/check_all.sh`, push the branch, and open a pull request against
`devel`. Link its issue in the pull request description. Delete the local and
remote topic branch after merge.

## Prepare A Release

Choose the next version according to Semantic Versioning, then create a
temporary release branch:

```bash
git switch devel
git pull --ff-only origin devel
git switch -c release/1.0.1
```

Update every project version source:

- `meson.build`
- `CMakeLists.txt`
- `src/meson.build`
- fallback `PACKAGE_VERSION` in `src/plugins/plugin.c`

Move completed entries from `[Unreleased]` into a dated release section in
`CHANGELOG.md`. Add comparison links for the new version and next unreleased
range. Update versioned commands and package filenames in user documentation.

## Validate And Package

Run the canonical validation bundle and build the Debian package from the
release branch:

```bash
./scripts/check_all.sh
./packaging/deb/build_deb.sh --run-checks
dpkg-deb --info packaging/out/gstklvplugin_1.0.1_amd64.deb
sha256sum packaging/out/gstklvplugin_1.0.1_amd64.deb \
  > packaging/out/gstklvplugin_1.0.1_amd64.deb.sha256
```

Architecture suffix varies with the build host. Test installation in a clean
environment before publishing when release contents or dependencies change.

## Merge And Tag

Open the release pull request against `main`. After approval, merge it without
squashing so release history remains explicit. Create an annotated tag on the
resulting `main` commit:

```bash
git switch main
git pull --ff-only origin main
git tag -a v1.0.1 -m "gstklvplugin v1.0.1"
git push origin v1.0.1
```

Merge `main` back into `devel` so version and changelog changes remain in the
integration branch:

```bash
git switch devel
git pull --ff-only origin devel
git merge --no-ff main
git push origin devel
```

## Publish On GitHub

Create a non-prerelease GitHub Release from the new tag. Use the corresponding
`CHANGELOG.md` section as release notes and attach the generated `.deb` plus its
SHA-256 checksum. Verify source archives point to the tagged commit.

With GitHub CLI:

```bash
gh release create v1.0.1 \
  packaging/out/gstklvplugin_1.0.1_amd64.deb \
  packaging/out/gstklvplugin_1.0.1_amd64.deb.sha256 \
  --title "gstklvplugin v1.0.1" \
  --notes-file release-notes-v1.0.1.md
```

Delete `release/1.0.1` locally and remotely after publication. Never move or
reuse a published tag; publish a new patch version for corrections.

## Hotfix Workflow

Create urgent production fixes from `main`, merge them into `main`, tag a new
patch release, then merge `main` back into `devel`. Use the same validation,
packaging, and publication checks as a normal release.

## Release Checklist

- `main` and `devel` contain intended commits.
- Meson, CMake, plugin, documentation, and package versions agree.
- `CHANGELOG.md` has release date and comparison links.
- `./scripts/check_all.sh` passes.
- Debian package metadata reports expected version and architecture.
- Annotated tag points at released `main` commit.
- GitHub Release uses that tag and contains package plus checksum.
- `main` is merged back into `devel`.
- Temporary release branch is deleted.
