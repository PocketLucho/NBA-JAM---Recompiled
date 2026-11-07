NBA JAM Recomp - Midway's NBA JAM saga recompiled from its source code
=====================================================================
Created by Pocket_lucho with Claude (Anthropic).
Project: https://github.com/PocketLucho/NBA-JAM---Recompiled
If you want to support me, go to https://buymeacoffee.com/pocket_lucho

Run NBAJamRecomp.exe (64-bit Windows 10/11, no installation). A menu lets you
pick the game (arrows + Enter, or a gamepad). Esc goes back to the menu. The
language (castellano / English) is chosen in the menu.

(Castellano: ver LEEME.txt)


IMPORTANT: YOU PROVIDE THE GAME DATA
------------------------------------
The program does NOT include any Midway graphics, sound, music or code: they
are copyrighted and cannot be distributed. Copy your own files into the roms\
folder. The program finds them by itself (by CRC32, whatever the file name,
inside .zip files too) and extracts what it needs when a game starts; there is
nothing to unpack.

  Game                           What to put in roms\
  -----------------------------  ---------------------------------------------
  NBA JAM (1993)                 MAME romset nbajam.zip
  NBA JAM Tournament Edition     MAME romset nbajamte.zip, or the ZIP of the
                                 historicalsource/nba-jam-tournament-edition repo
                                 (without nbajamte.zip it uses the SE sound ROMs)
  NBA JAM T.E. Special Edition   the ZIP of the Asure/nba-jam-tournament-edition-SE repo
  NBA Hangtime                   MAME romset nbahangt.zip (ver L1.3 or L1.1)
  NBA Maximum Hangtime           ver 1.3: ZIP of the historicalsource/nba-hangtime
                                 repo + nbahangt.zip (graphics and DCS sound);
                                 or ver 1.03: MAME romset nbamht.zip

Repository ZIPs are downloaded on GitHub with "Code -> Download ZIP":
  https://github.com/historicalsource/nba-jam-tournament-edition
  https://github.com/Asure/nba-jam-tournament-edition-SE
  https://github.com/historicalsource/nba-hangtime

A game with missing files is shown in grey; selecting it tells you what is
missing.

The versions tagged SRC (T.E. 4.0, Special Edition 5.02 and Maximum Hangtime
1.3) were compiled from the published source code and the result is
byte-identical to the original ROMs, so it does not matter where the data
comes from. To build them yourself use tools/build_roms.sh from the source
code: its output in data\ is used when the ROMs are not found.


CONTROLS (keyboard)
-------------------
              Up Down Left Right     Shoot Pass  Turbo  Start  Coin
  Player 1    arrows                  Z     X     C      1      5
  Player 2    W   S   A   D           F     G     V      2      6
  Player 3    numpad 8/2/4/6          0     .   Enter    3      7
  Player 4    I   K   J   L           H     ;     U      4      8

  Gamepads: each pad controls one player (A shoot, B pass, X/trigger turbo,
  Start, Back = coin).

  F1   on-screen help                F2  TEST switch (service menu)
  F3   reset
  F4   video: Sharp -> Smooth -> Scanlines -> CRT -> Curved CRT
  F5   integer scaling (pixel perfect) on/off
  F6   scanline strength
  F7   aspect: 4:3 original -> 16:9 widescreen -> 16:10 -> fill the window
  F11  fullscreen (or Alt+Enter)
  F12  screenshot (PNG in the screenshots\ folder; Print Screen works too)
  P    pause                         - / +  volume
  9    service button (credit)
  Esc  back to the game menu

Settings (language, scale, keys, volume, DIPs) are kept in nbajam_recomp.ini
next to the .exe. Each game's high scores and settings (NVRAM/CMOS) are kept
in saves\.


VIDEO (shaders)
---------------
  The picture is drawn with OpenGL shaders. With integer scaling (F5) every
  game line covers the same number of screen rows, so scanlines are perfect.
    Sharp        clean pixels at any size
    Smooth       bilinear filtering
    Scanlines    dark gaps between lines; F6 changes the strength
    CRT          gaussian beam per line, RGB aperture mask and gamma
    Curved CRT   the same with tube curvature and vignette
  F7 stretches the picture to widescreen (somewhat distorted) or to the whole
  window. Without OpenGL 2.1 a basic mode is used (opengl=0 in the .ini forces
  it).


ONLINE PLAY
-----------
  Select the game in the menu and press N (or Y on the pad).
  * The host chooses "Host a game": they are player 1 and wait in the lobby.
    Press Enter to start.
  * The others choose "Join", type the host's IP (Ctrl+V pastes) and press
    Enter. They become players 2, 3 and 4 in order of arrival.
  * Everybody plays with their player 1 controls (P1 keys or first pad).
  * Everybody needs the same ROMs; the app tells you if they differ.
  * Input delay: 3-4 on a LAN, 5-7 over the internet. If the game stalls now
    and then ("Waiting for the network..."), raise the delay.
  * Over the internet the host must forward UDP port 7845 on the router, or
    use a virtual LAN (Tailscale, ZeroTier, Radmin VPN).
  * If "DESYNC" appears, leave (Esc) and host the game again.


FOLDERS
-------
  roms\         your ROMs / ZIPs (see above)
  saves\        each game's NVRAM
  screenshots\  screenshots (F12)
  data\         (optional) games you built with tools/build_roms.sh


HOW IT IS MADE
--------------
  * Emulation: MAME CPU cores (TMS34010, M6809, ADSP-2105), YM2151 through
    ymfm, and the Midway T-Unit and Wolf Unit boards on a minimal framework.
  * Build: a TI GSPA/GSPLNK compatible assembler/linker written for this
    project (tools/gspasm) and Midway's original LOAD2.EXE run under emu2.
    See README.md in the source code.

NBA JAM, NBA Hangtime and Midway are trademarks of their owners. Non-profit
fan project, not affiliated with them.
