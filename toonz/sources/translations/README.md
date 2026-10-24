# Contributing and maintaining translations

OpenToonz translations have two kinds of files:

- `.ts` files are editable translation sources, grouped here by language and
  application module. Contributors normally share these files.
- `.qm` files are compiled catalogs loaded by OpenToonz. Generate them for
  testing and packaging; they do not replace the editable sources.

This guide describes the current workflow. Manual Qt Linguist, `lupdate`, and
`lrelease` workflows remain supported. Planned build-system repairs are not
required to contribute translations.

## For contributors: share a translation

1. Start with the latest catalog from the branch you intend to contribute to.
   Use official `master` for contributions to OpenToonz. Find your language directory and the relevant module:

   | Catalog | Typical content |
   | --- | --- |
   | `toonz.ts` | Main application, windows, menus, and dialogs |
   | `tnztools.ts` | Drawing and editing tools |
   | `toonzqt.ts` | Shared UI widgets and controls |
   | `toonzlib.ts` | Scene/level infrastructure and related messages |
   | `tnzcore.ts` | Core infrastructure messages |
   | `colorfx.ts` | Color styles |
   | `image.ts` | Image format options and messages |

2. Open the `.ts` file in [Qt Linguist](https://doc.qt.io/qt-5/linguist-translators.html).
   Edit the translation in its existing context. A repeated English phrase in
   another context is a separate entry. Mark entries finished only when reviewed.
   Preserve `%1`, `%2`, `%n`, and other placeholders; retain necessary line breaks
   and menu mnemonics (`&`, represented as `&amp;` in XML). Use Linguist's
   validation checks. Do not rename source strings or contexts to translate them.

3. Share the modified `.ts` files through a pull request. Name the language,
   affected UI, testing performed, and any terminology that needs review. Keep
   unrelated catalog reformatting and extraction changes out of the submission.
   Follow [CONTRIBUTING.md](../../../CONTRIBUTING.md) for repository/base selection.

   If a PR is impractical, attach the updated catalogs to an issue or discussion
   in the destination repository, using a ZIP if the site rejects `.ts` uploads.
   Include the original catalog version or commit, language, changed areas, and
   preferred contributor credit. A maintainer can integrate the files for you.

You do not need to compile OpenToonz to contribute wording. Qt Linguist's Release
action or `lrelease` can optionally generate a `.qm` for local testing. Submit
editable `.ts` sources; provide generated binaries only when a maintainer asks.

For a new language, ask maintainers to coordinate catalog creation, locale
metadata, release registration, and review. Adding a directory alone does not
enable a language in distributed builds.

## For maintainers: review and merge contributed wording

1. Confirm the destination branch and source catalog version. Compare attached
   files with the current sources instead of overwriting intervening work.
   Preserve the translator's attribution and any existing co-author credits.
2. Review the changed contexts, source strings, locale metadata, placeholders,
   mnemonics, and finished/unfinished status. Seek native-speaker review where
   needed. Avoid treating XML validity as proof of correct wording.
3. Compile the affected catalogs with the project's Qt 5 tools. For example,
   from the repository root, after creating a scratch output directory:

   ```sh
   lrelease toonz/sources/translations/spanish/toonz.ts -qm /PATH/TO/TEST/toonz.qm
   ```

   Replace the paths and module with the files under review. On Windows, use a
   Qt command prompt or invoke the full path to `lrelease.exe`. Review its
   diagnostics and exit status; an invalid catalog must not be packaged.
4. Merge the reviewed `.ts` changes through the repository's normal PR process.
   Regenerate and package the affected `.qm` files, then test that the application
   loads the new wording. Record the language, build, and checks performed.

## For maintainers: update catalogs when application strings change

Extraction is a maintenance action: `lupdate` scans source and adds/updates `.ts`
entries. It does not translate new messages. Review its diff before committing;
new entries may remain unfinished for translators to complete. Keep extraction
updates separate from unrelated wording changes where practical.

### Existing CMake / Visual Studio targets

Configure an application build with `WITH_TRANSLATION=ON` and the project's Qt 5
Linguist tools available. From the repository root, an example is:

```sh
cmake --build toonz/build --config Release --target translate_update_toonz
```

Use your actual build directory and configured build configuration. Other
module targets are `translate_update_tnztools`, `translate_update_toonzqt`,
`translate_update_toonzlib`, `translate_update_tnzcore`,
`translate_update_colorfx`, and `translate_update_image`. In Visual Studio,
build the corresponding `translate_update_*` project explicitly. These targets
can update all registered languages for that module, not just one language.
The current Qt/CMake update path also generates intermediate `.qm` outputs.

### Existing single-language shell script

With Bash and the intended Qt 5 `lupdate` on PATH, run from the repository root:

```sh
bash toonz/sources/translations/update.sh spanish
```

Use the source-directory name, such as `portuguese_brazil`, not the UI display
name. Always supply the language argument and check that the directory is the
one intended. The current script has limited argument/error checking and scans
module directories differently from CMake's source lists. Review every command's
output and the resulting diff; do not use its final exit status alone as proof
that every module updated successfully. Creating a new language through this
script still requires maintainer registration and locale checks.

Direct `lupdate` use remains available for experienced maintainers. Include the
correct source/header scope and translation context for the module. Do not
blindly run extraction over the entire repository or third-party libraries.

## Compile, package, and test the resulting catalogs

Current translation-enabled application targets run `lrelease` after linking.
A `.ts`-only edit may not relink the target, so an otherwise successful no-op
build is not proof that its `.qm` was refreshed. Until the dependency repair
lands, compile the affected catalogs explicitly with Linguist or `lrelease`,
or confirm a target rebuild generated the expected outputs.

The runtime location is the active OpenToonz configuration directory's
`loc/<UI language name>/<module>.qm`. For a conventional `toonz/build` tree,
application build rules write into repository `stuff/config/loc`. Source and
runtime directory names differ:

| Source directory | Runtime language directory |
| --- | --- |
| `japanese` | `日本語` |
| `italian` | `Italiano` |
| `french` | `Français` |
| `spanish` | `Español` |
| `chinese` | `中文` |
| `german` | `Deutsch` |
| `russian` | `Русский` |
| `korean` | `한국어` |
| `czech` | `Čeština` |
| `portuguese_brazil` | `Portuguese` |

Norwegian Bokmål sources are present but are not in the current release language
list. English uses source text rather than a separate English catalog.

Package the generated catalogs from the expected output location. For local
testing, back up the affected `.qm` before replacing it in the test build's
active configuration. An installed build and a portable build can use different
`stuff` directories; verify which one the executable actually loads. Select the
UI language, restart OpenToonz, and inspect the specific changed controls.
Check that filenames, saved data, and placeholders remain correct.

If wording stays in English, check the selected language, active configuration
path, module filename, catalog timestamp, exact source/context match, and
unfinished status. Qt-owned widget wording and native OS dialogs use separate
translation mechanisms and may behave differently from OpenToonz catalogs.

## Build-system repair boundary

The intended repair will separate explicit extraction from dependency-tracked
compilation, so normal builds consume committed `.ts` files without rewriting
them and translation-only changes regenerate `.qm` files reliably. Existing
manual workflows should remain usable. Update the build instructions here when
that behavior has been implemented and verified, rather than assuming it exists
today. The documented Xcode `WITH_TRANSLATION=OFF` workaround remains applicable
until translation-enabled Xcode builds are verified.
