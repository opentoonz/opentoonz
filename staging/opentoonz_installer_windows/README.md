# Warn when the Stuff folder is under Program Files

This directory stages a proposed change for:

`opentoonz/opentoonz_installer_windows`

It is not part of the OpenToonz application build.

## Why this should be considered

The Windows installer lets the user choose where the OpenToonz Stuff folder is installed. The Stuff folder contains writable OpenToonz state and configuration, including room/layout data, palettes, projects, profiles and other settings.

A crash report was received from an installation using:

`C:\Program Files\OpenToonz\OpenToonz stuff`

That location is not being identified as the demonstrated cause of the reported crash. The crash investigation did, however, expose an independently problematic installer configuration: Windows normally protects `Program Files` from ordinary application writes, while OpenToonz expects to modify files in the Stuff folder while running.

The installer currently defaults to the more appropriate writable location:

`C:\OpenToonz stuff`

## Proposed behavior

When the user advances from the Stuff-folder selection page, check whether the selected directory is under Program Files.

If it is, display an advisory warning explaining that OpenToonz needs to write to the Stuff folder and recommend a writable location such as `C:\OpenToonz stuff`.

The warning can be overridden. This preserves intentionally configured studio installations where permissions have been explicitly arranged.

## Warning text

> The OpenToonz Stuff folder contains settings and other files that OpenToonz needs to modify while running.
>
> Windows normally restricts write access inside Program Files. Using this location may prevent OpenToonz from reliably saving settings, room layouts, palettes, projects, and other writable data.
>
> A writable location such as `C:\OpenToonz stuff` is recommended.
>
> Continue with this location anyway?

## Test plan

1. Run the installer and keep the default `C:\OpenToonz stuff`. No warning should appear.
2. Select `C:\Program Files\OpenToonz\OpenToonz stuff`. The warning should appear.
3. Select another subdirectory under Program Files. The warning should appear.
4. Choose **No**. The installer should remain on the Stuff-folder selection page.
5. Choose **Yes**. Installation should continue normally.
6. Select a writable location outside Program Files. Installer behavior should remain unchanged.

## Upstream handling

This patch should be transferred to `opentoonz/opentoonz_installer_windows` after review. Shun Iwasawa will need to verify the installer behavior and merge the upstream PR.
