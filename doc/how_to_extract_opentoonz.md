This document is assuming you're on windows, but can be modified to work on other operating systems as well.

First, download [innotextract](https://constexpr.org/innoextract/) from the website.

Make a temporary folder where you downloaded it. Ensure the innoextract binary is in the same location as the OpenToonz installer.
Type the following in your terminal or command prompt:
.\innoextract.exe OpenToonzSetup.exe --output-dir <folder you made>

cd to the extracted folder.

You should see two directories, app and one (most likely) called code$GetGeneraldir. Rename that to portablestuff.
Move portablestuff into the app folder. You can now rename app as OpenToonz or whatever you like and move it anywhere you desire.
