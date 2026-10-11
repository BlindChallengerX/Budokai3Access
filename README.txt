Budokai 3 Access 1.0.2

A screen reader mod for Dragon Ball Z Budokai 3 on PC. It speaks through
your screen reader (NVDA, JAWS and others), or the Windows voice if none is
running.


WHAT YOU NEED

- The Budokai 3 HD PC port by NovaPowers. Tested with versions 1.4.3.1 and
  1.4.2.3:
  https://github.com/novapowers0/DBZ-Budokai-3-HD-Collection/releases/tag/v1.4.3.1
- Your own copy of the game, USA version, set up as the port explains.
  Get the game running first, then add this mod. SETUP-GUIDE.txt on the
  mod's GitHub page explains the whole setup in English.


INSTALL

1. Unzip Budokai3Access-1.0.2.zip into the game folder, the folder with dbz3.exe.
2. Run "Play Budokai 3 Access.exe" from that folder.

If Windows says "Windows protected your PC", choose "More info", then
"Run anyway". This shows for new programs that are not from a big company.

That's it. Always start the game with "Play Budokai 3 Access.exe". The port's
launcher opens; press Enter to play. You should hear "Budokai 3 Access loaded".


PLAYING WITH A KEYBOARD

No controller is needed. "Play Budokai 3 Access.exe" turns on the port's
keyboard controls (a controller still works too). The keys:

Move (left stick)        W A S D
Punch (X button)         L
Kick (A button)          Space
Guard (B button)         Backspace
Ki (Y button)            P
D-pad                    Shift + arrow keys
Right stick              Arrow keys
LB / RB                  1 / 3
LT / RT                  Q / E
Start                    Enter
Back                     Z
Left stick click (L3)    F
Right stick click (R3)   K

In menus, Space (A) picks and P (Y) goes back. Where the mod names
controller buttons, for example in Dragon Rush, X is L, Y is P, A is Space
and B is Backspace. You can change the keys in the port's launcher.


PLAYSTATION CONTROLLERS

PS4 and PS5 controllers work. The port normally reads only Xbox
controllers, so the first time you run "Play Budokai 3 Access.exe" it
switches the port to its SDL controller support, which reads PlayStation
and Xbox controllers. Where the mod names Xbox buttons: A is Cross, B is
Circle, X is Square, Y is Triangle, LB/RB are L1/R1, Back is Share or
Create, and L3/R3 are the stick clicks.

To set it by hand, close the game, open dbz3_user.toml in the game folder
in Notepad and make sure these two lines read:
input_backend = "sdl"
dbz3_input_backend = "sdl"
If you want the old Xbox-only mode back, choose XInput as the controller
backend in the port's launcher; the mod will not switch it again.


STARTING THE GAME

"Play Budokai 3 Access.exe" checks the mod is installed, repairs it if a port
update replaced its loader, and starts the game. Make a desktop shortcut to
it if you like. On the first menu the mod says New Game, Load Game or
Continue as you move (Continue goes back to the main menu).


WHAT SHOULD SPEAK

- Text on screen: menu descriptions, the character on the select wheel,
  messages such as "Load successful", option values in the pause menu, skill
  names when a fighter uses a skill, and story and tutorial text. Budokai 3
  draws all its text as pictures, so the mod carries the words of every text
  picture on the disc, read with text recognition. A few may have small
  mistakes; please report any you hear.
- Stage select: the stage under the cursor and its place in the list, for
  example "Plains, 5 of 6"; "Random stage" for the question mark. Only
  stages you have unlocked are in the list.
- Character select: "Character select" when it opens, the fighter you scroll
  to, and in the Normal, Custom, Edit Skills choice the item and its
  position, for example "Custom, 2 of 3".
- Fights: Ready, Fight, K O, Time up, Ring out, guard breaks, counters and
  other fight messages. A warning when your health drops below half and
  below a quarter.
- Main menu: the mode under the cursor and its place, for example
  "Dueling, 2 of 6" (7 once Dragon Arena is unlocked). Goku's comment on
  each mode is not read.
- New Game / Load Game / Continue (after the title): the item and its place,
  without the comments.
- World Tournament: the course and its rounds, for example "Adept, 4 rounds,
  locked, 2 of 3" (Cell Games joins the list once unlocked), then "Number of
  players: 1". The announcer's lines are not read.
- Options: the category or setting under the cursor and its value, for
  example "COM Level: Weak" or "Music Volume: 100 percent", and the new value
  when you change it with left or right. F5 repeats it.
- Controller (in Options): the row and its value ("Config: Custom",
  "Vibration: Off"), and the whole button layout when the screen opens, for
  example "Y Kick, B Ki, A Guard, X Punch". In Custom, hold a button and press
  the D-pad: you hear that button's new action, for example "X: Guard". F5
  reads the row and the whole layout again.
- Menus: Dueling (who plays), the Edit Skills menu, the Skill Shop (tab, then
  each skill with its price and position, for example "Masenko, 1500 zenie,
  1 of 3"), the pause menu in every mode, the menu after a match, and the
  Exit yes or no question. In the Practice pause menu each option says its
  setting, for example "COM action: Guard 1, 2 of 11", and the new setting
  when you change it.
- Skill list in the pause menu (Refer to Skills, Skill List): each move with
  its cost and its button command, for example "Kamehameha. 1 Ki gauge
  consumed. forward kick, punch, punch, punch, ki. 3 of 5". Commands use
  punch, kick, guard and ki for the four attack buttons, and forward, back,
  up and down for the stick or D-pad.
- Dragon Universe character select: the character under the cursor and
  their level, for example "Krillin, level 0". F5 adds experience, what is
  needed for the next level, and the seven stat levels.
- Dragon Universe Bonus screen (Dragon Universe title menu, Bonus): "Bonus"
  when it opens, then the item as you press left or right, for example
  "Memories of Goku, 1 of 41". There are 41 items, the "Memories of"
  capsules plus one more at the end. Items you have not unlocked say
  "Locked", the same as the question marks on screen.


ULTIMATE ATTACKS

When an Ultimate attack starts you hear "Ultimate! Press as the tone tops
out, 3 presses". Each side has a meter that sweeps up and starts over,
almost twice a second, for 4 seconds. Your score is where the meter is when
you press, so the best press is just before it starts over. A tone climbs
with your meter and a soft low beat marks each start: press as the tone
reaches the top, just before the beat. Too late and the meter has already
started over, which scores almost nothing. You get 3 presses; each press
clicks. At the end you hear both scores and who won (100 per press is
perfect). The distance sound is quiet for the whole Ultimate.


DRAGON RUSH

When a Dragon Rush starts you hear "Dragon Rush! You attack" or "You
defend". Each exchange then says what you can press, for example
"Attack 2: Y A B" or "Guess 1: X Y A B" (a button the attacker has already
used is left out, because no one can pick it again). You have about 3
seconds. A short click means your button counted. After both have picked
you hear both buttons and the result, for example "You X, opponent B.
Hit!". The defender stops the rush by guessing the same button.

DISTANCE SOUND

During a fight a steady tone plays, the same kind as in Tekken 3 Access. The
closer the opponent, the higher the tone: high when you are touching, two
octaves lower when far apart. It comes from the opponent's side of the
screen, and it is reedy when the opponent is on your left and smooth when on
your right. When the opponent is above you a soft chirp slides up every so
often; when below, it slides down. The tone starts at "Fight!", stops when
the round ends, and is quiet while the game is paused.

In World Tournament you are sometimes player 2. The mod checks which side
you control in every match, so the tone, F6, F8, the health warnings, the
Ultimate meter and messages like "first attack" are always about you.

Ring edge (World Tournament Stage only, the stage with a ring): a short, loud
knock from the side of the nearest edge of the ring, once you are within
about half the ring's width of it. It knocks faster and louder the closer you
are, from about twice a second up to a rapid rattle right at the edge. A hard
hit from inside that range can knock you out of the ring. F6 tells the edge
in words.


DRAGON UNIVERSE (STORY MAP)

Flying: left stick up flies forward (it keeps some momentum). Right stick
left and right turns you; right stick up and down tilts the view. Y opens
the map and Dragon Radar, Back shows help, Start opens the field menu
(Edit Skills, Check Status, Save or Quit), which the mod reads.

Places and story events are markers. To enter one, get within about 3000
units, press RB for Ki Search, and turn until it is in front of you. The
game shows its name, the mod says it with the distance, for example
"Plains. 692 units, A to go there". Press A to go in. Some markers start a
story scene or battle; others are cities and landmarks.

Hidden spots ("???" on screen) hold Dragon Balls, the Dragon Radar,
capsules and Zenie. Fly towards one; within about 1000 units, while it is
in view, the mod says "Hidden spot here". Press RB for Ki Search, face it,
and when you hear "Hidden spot, ... units, A to take it", press A. If you
fly right over it, it drops out of view; back off a little and face it.
The marker list calls them "Hidden spot", or "Dragon Ball spot" for the
ones that look like Dragon Balls. A spot leaves the list once collected,
and each one only lasts for its part of the story.

The mod keeps a guide target, at first the story event (the red dot on the
game's map); its name starts with "Story". A beep repeats from the target's
side: high when it is ahead of you, low when it is behind you, brighter when
you are heading straight at it, and faster as you get closer. Turn until the
beep is high, bright and centred, then fly forward. When it is close enough
you hear "Guide target in Ki Search range".

After a story battle: the Battle Estimation is spoken as one summary (base
experience, each scored item with its points, and the total); F5 repeats
it. Then Level Up: you spend Z points on Health, Ki, Attack, Guard, Arts,
Ability and Com. Up and down choose a stat, right on the stick adds a
point, A finishes, then Yes or No confirms. Each move says the stat, the
points added to it and how many remain; F5 lists them all.

Field menu (Start): Edit Skills, Check Status, Save or Quit are read as you
move. Check Status says your level, experience, how much to the next level
and the points in each stat. Edit Skills opens the Custom Tray: 7 slots,
each capsule takes 1 or more. On the tray, up and down move between slots
("Slot 2: Kaioken, 1 slot"), A opens the capsule list for that slot, B
removes the capsule, X shows its skill card, Y goes back. In the list, left
and right change the tab (Ability, Equipment, Item), up and down move, and
each capsule says its slots and "in tray" if it is already there; A puts it
in. Adding or removing says how many of the 7 slots are used. F5 repeats
where you are with the controls. Skill cards read their button command in
words, for example "After punch, punch, punch, punch: ki."

The map wraps around: fly far enough one way and you come back from the
other side. Distances and directions take the short way round.

In Dragon Universe the keys do this:
F5          Next marker as the guide target, nearest first (Shift+F5:
            previous). It says the name, distance and which way to turn,
            for example "2 of 10, Kame House, 5189 units, turn left 40".
F6          Where the guide target is now.
F8          The place you are in and every marker within 3000 units.
Shift+F6    Guide beep off or on (the same switch as the fight tone).

On the controller: clicking the right stick (R3) does F5 everywhere (with
LB held, Shift+F5), and clicking the left stick (L3) does F6.


SOUND DELAY

The port keeps a small queue of game sound. "Play Budokai 3 Access.exe" sets it
shorter (the line "audio_maxqframes = 3" in dbz3_user.toml), which takes
about 25 ms off every game sound: a menu click now comes about 105 ms after
the key press instead of 130. Most of what is left is the game itself
reacting to the button. The mod's own sounds come out right away. If you
ever hear crackling, change the line to "audio_maxqframes = 8" (the port's
own setting); the launcher keeps any value you write there.

KEYS

F2          Read all text on screen again.
F5          Read the highlighted menu item (where the mod knows the menu).
F6          Distance to the opponent in words, for example
            "mid range, to your right, 27 units, above you".
Shift+F6    Turn the distance sound off or on.
F8          Health and ki of both fighters, for example
            "You: health 77 percent, ki 3.0 bars. Opponent: health 100
            percent, ki 3.3 bars."
F12         Repeat the last thing spoken.
Page Up     Mod sounds louder (distance tone, edge, guide beep, meters), 10
            percent a step up to 200 percent. Remembered between sessions.
Page Down   Mod sounds quieter, down to 10 percent.

The game port uses F1 (quick menu), F3 (frame rate), F4 (settings),
F7 (achievements) and F10 (developer overlay), so the mod stays off those.


NOT DONE YET

- The Dragon Radar and the map screen are not read yet.
- A sound for dodging.


ORIGINAL MUSIC (OPTIONAL, NOT INCLUDED)

The HD Collection replaced the original American soundtrack from the PS2
release. If you own the PS2 version of Budokai 3 (USA), you can put the old
music back yourself with the build_og_music.py script in the tools folder of
the source: https://github.com/BlindChallengerX/Budokai3Access
The music is not included here and the mod works the same without it.


REMOVING THE MOD

In the game folder, delete rexgpu-xenos.dll, then rename
rexgpu-xenos-original.dll to rexgpu-xenos.dll. You can also delete the
Budokai3Access folder and "Play Budokai 3 Access.exe".


IF SOMETHING GOES WRONG

Port updates put the port's own rexgpu-xenos.dll back. That is expected:
"Play Budokai 3 Access.exe" puts the mod back each time you start the game with
it.

If a port update changes things the mod relies on, the mod switches itself
off and says so, and the game keeps running without it. Then look for a mod
update.

The mod's log is Budokai3Access\Budokai3Access.log. Please include it when you
report a problem.