BUDOKAI 3 ORIGINAL MUSIC 1.0
by BlindChallengerX

The HD Collection replaced the original American soundtrack of Budokai 3.
This program puts the PS2 music back into the PC port of the DBZ Budokai 3
HD Collection, using your own PS2 disc. No music is included in this
download; it is built from your disc on your computer.

It swaps the menu and story music, the stage music and the jingle for the
PS2 versions, and the opening movie for the PS2 one. Voices and sound
effects stay as they are. Your game files and your disc image are not
changed: the music goes into the port's mods folder as a mod called
og_music.

It works with or without Budokai 3 Access, and doesn't need Python or
anything else.


WHAT YOU NEED

- The PC port of DBZ Budokai 3 HD Collection with the USA version of the
  game.
- The PS2 Budokai 3 disc, USA version, as a disc image: a .iso file, or
  the .7z or .zip it came in. A .7z needs 7-Zip installed
  (https://www.7-zip.org). The files from the disc work too: point it at
  ADX_USA.AFS from the USR folder of the disc.
- About 800 MB of free space. If the game plays straight from the HD
  Collection ISO, about 3 GB (see below).


HOW TO USE IT

1. Close the game.
2. Put "Budokai 3 Original Music.exe" in your game folder, next to
   dbz3.exe.
3. Run it. It's a text window that asks simple questions: type Y or N
   and press Enter.
4. It looks for your PS2 disc image in the game folder, the folder above
   it, Downloads, Desktop and Documents. If it finds one, it asks if it
   should use it. If not, a normal Windows Open window comes up so you can
   pick the file.
5. It builds the music, saying the progress in steps of 10 percent. Then
   it says Done. Press Enter to close it, and start the game the way you
   always do.

You can also drag your disc image onto the program to skip the search.

It takes a minute or two. Unpacking a .7z first takes a few minutes more;
the unpacked copy is deleted afterwards.


IF YOUR GAME PLAYS STRAIGHT FROM THE ISO

The port only loads mods when the game plays from extracted files, not
straight from the HD Collection .iso. If yours plays from the .iso, the
program tells you and offers to copy the Budokai 3 files out of it into
the game folder (about 2.1 GB: default.xex and a us folder). It also takes
the ISO line out of dbz3_user.toml so the game uses those files, and keeps
the old settings as dbz3_user.toml.bak-music. Your .iso isn't changed. It
asks before doing any of this.


GOING BACK TO THE HD MUSIC

Run the program again. It says the music is already installed. Type R and
press Enter to remove it. You can also turn the mod off in the Mods tab of
the port's launcher, or delete the folder mods\og_music.

To build it again, for example after a port update, run it and type B.


IF SOMETHING GOES WRONG

- "isn't the USA PS2 release": only the American PS2 disc fits the
  American HD game's music slots.
- "I can't find the game": the program has to be in the folder with
  dbz3.exe.
- "The game is running": close the game first.

Source code: https://github.com/BlindChallengerX/Budokai3Access
(music folder). Free to use and share, see LICENSE.txt.
