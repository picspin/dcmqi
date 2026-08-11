# Releasing dcmqi

The project **version is derived from the git tag** (`dcmqiVersionFromGit` in
`CMakeLists.txt`) — there is no version number to edit by hand. Binary packaging is
automated: the `cmake-linux`, `cmake-macos` and `cmake-win` workflows trigger on the
GitHub **release** event and upload platform packages.

Release notes are **changelog-first**: the section is written to `CHANGELOG.md` *before*
tagging, and a workflow copies it onto the published release. `CHANGELOG.md` is the
source of truth.

## Steps

1. **Author the changelog section** for the upcoming version. Draft it from the changes
   since the last tag, review/edit, and insert it at the top of
   [`CHANGELOG.md`](CHANGELOG.md) (just below the header, above the previous release):

   ```bash
   python tools/changelog/generate_changelog.py --next vX.Y.Z
   ```

   `--next` works before the tag exists (it diffs the latest tag against `HEAD`). To
   emphasize headline changes, add a bullet list to the `HIGHLIGHTS` dict in
   [`tools/changelog/generate_changelog.py`](tools/changelog/generate_changelog.py) and
   re-run. Commit the changelog change to `master`.

2. **Tag the release** on that commit and push the tag:

   ```bash
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```

   The version flows from the tag into the build automatically.

3. **Publish the GitHub release** for the tag (Releases → *Draft a new release* → select
   the tag → *Publish*). Two things then happen automatically:
   - the `cmake-*` workflows build and attach the platform packages;
   - the **Release notes from CHANGELOG** workflow extracts `vX.Y.Z`'s section from
     `CHANGELOG.md` and sets it as the release body. (If no matching section exists, that
     workflow fails loudly — go back to step 1.)

   Six packages are attached, one per platform/architecture:

   | Asset | Platform | Built by |
   | --- | --- | --- |
   | `dcmqi-X.Y.Z-linux.tar.gz` | Linux x86_64 | `cmake-linux` |
   | `dcmqi-X.Y.Z-linux-arm64.tar.gz` | Linux arm64 | `cmake-linux` |
   | `dcmqi-X.Y.Z-mac-x86_64.tar.gz` | macOS x86_64 | `cmake-macos` |
   | `dcmqi-X.Y.Z-mac-arm64.tar.gz` | macOS arm64 | `cmake-macos` |
   | `dcmqi-X.Y.Z-win64.zip` | Windows x86_64 | `cmake-win` |
   | `dcmqi-X.Y.Z-win-arm64.zip` | Windows arm64 | `cmake-win` |

   The two x86_64 names are unqualified for historical reasons and must stay that way so
   existing download URLs keep working; see `CPACK_SYSTEM_NAME` in `CMakeLists.txt`.

You can leave the release body empty when publishing; the workflow fills it in. To
preview what it will post: `python tools/changelog/generate_changelog.py --extract vX.Y.Z`.

## Rebuilding the whole changelog

`CHANGELOG.md` can be regenerated end-to-end at any time (idempotent, self-healing):

```bash
python tools/changelog/generate_changelog.py
```

See [`tools/changelog/README.md`](tools/changelog/README.md) for how the generator works
and how to tune its noise filter and categorization.
