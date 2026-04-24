# Flicky's Flock — NetLink Edition

> 💝 **Huge thanks to [Slinga](https://github.com/slinga-homebrew)** for the original Flicky's Flock — the Sonic-themed Flappy Bird clone, the 12-player chaos, the sprites, the sound, the entire Saturn homebrew foundation this fork builds on. This port only *adds* a networking layer; every line of the original game and every asset is Slinga's. If you enjoy this online port, go star the upstream repo: https://github.com/slinga-homebrew/Flickys-Flock

---

## Online Multiplayer (NetLink)
Flicky's Flock now supports online multiplayer via the Sega Saturn NetLink modem. Up to 12 players can connect to a central server and play cooperatively or competitively over the internet.

### Features
- **Server-authoritative gameplay**: Pipe spawning, powerup spawning, collision detection, scoring, and game-over conditions are all managed by the server to ensure consistency
- **Custom name entry**: Grid-based character entry screen for choosing your online handle (saved to Saturn backup RAM across power cycles)
- **Online lobby**: See connected players with bird sprites, toggle ready status, start the game when players are ready
- **Persistent leaderboard**: Server tracks wins, best score, and games played per player across sessions
- **Z-button stats overlay**: Hold Z in the lobby to view last game results and the all-time online leaderboard (alternates every 3 seconds)
- **Server bots**: Add/remove AI-controlled bot players from the lobby using Up/Down on the D-pad
- **Couch co-op online**: Connect a second controller locally and both players join the online game from the same Saturn
- **Bird selection**: Use L/R triggers in the lobby to choose your bird color
- **Progressive difficulty**: Pipe scroll speed increases slightly with each gate passed
- **Score screen with names**: The end-of-game ranking screen displays player names in online mode
- **Delta-compressed input**: Inputs are only transmitted when they change, minimizing bandwidth on the 14,400 baud modem link
- **Player state sync**: Remote player positions are periodically corrected by the server for smooth rendering
- **Name persistence**: Your name is saved to Saturn backup RAM and automatically loaded on the next power-on
- **NetLink LED activity**: The NetLink modem LED blinks during online gameplay to indicate network activity
- **Custom sprite font**: All online screens use a styled pixel font matching the game's visual aesthetic

### How to Connect
1. Select **ONLINE** from the title screen (only visible when a NetLink modem is detected)
2. Enter your name on the character grid (D-pad to move, A/C to select, B to cancel)
3. The Saturn dials out via the NetLink modem to the game server
4. Once in the lobby, press **A** to toggle ready; press **Start** to start the game when players are ready

### Lobby Controls
- **A/C**: Toggle ready status
- **Start**: Start game (when players are ready)
- **Up/Down**: Add/remove bots
- **L/R**: Change bird color
- **Z** (hold): View last game results and all-time leaderboard
- **B**: Return to title screen (stays connected for quick rejoin)
- **Y**: Disconnect and return to title screen

### Online Setup - DreamPi

If you have a [DreamPi](https://github.com/Kazade/dreampi) (Raspberry Pi with USB modem for retro online gaming), you only need to update the config file to route Flicky's Flock's dial code to the game server.

1. Copy `tools/dreampi/netlink_config.ini` to your DreamPi, replacing the existing config:
   ```bash
   sudo cp tools/dreampi/netlink_config.ini /opt/dreampi/netlink_config.ini
   ```
2. Restart DreamPi:
   ```bash
   sudo systemctl restart dreampi
   ```

The `netlink_config.ini` maps the dial code  to the Flicky's Flock server. No bridge script is needed - DreamPi's default `netlink.py` handles the connection.

### Online Setup - PC (eaudnord's NetLink script)

If you're using a PC with eaudnord's NetLink script instead of a DreamPi:

1. Replace your `netlink_config.ini` with the one from `tools/dreampi/netlink_config.ini`
2. The config maps dial code  to 
3. Connect your USB modem to your PC and your Saturn NetLink via phone cable
4. Run the NetLink script as normal

### Server Setup

The Python game server is in `tools/flock_server/`:

```bash
cd tools/flock_server
python fserver.py
```

Options:
- `--bots N` - Number of AI bot players to add at startup (default: 0)
- `--verbose` - Enable debug logging

The server stores a persistent leaderboard in `leaderboard.json` next to `fserver.py`.

# Flicky's Flock
Flicky's Flock is a 12-player Sonic the Hedgehog themed Flappy Bird clone for the Sega Saturn. Requires two [6 Player Adaptors](https://segaretro.org/Saturn_6_Player_Adaptor) for full twelve player support. Requires a modded Saturn or another method to get code running on actual hardware. Build the code with Jo Engine or grab an ISO from [releases](https://github.com/slinga-homebrew/Flickys-Flock/releases). Note: The release ISO does not contain a sound track. You must supply your own. Suggestions are provided.

The resolution of the game has been changed to support wide screen televisions:
- Use the zoom feature (not 16:9) in your television
- The top and bottom of the screen will be cut off but the game area will take up the entire TV
- The aspect ratio looks correct

Flicky's Flock was my entry to the [Sega Saturn 26th Anniversary Game Competition](https://segaxtreme.net/threads/sega-saturn-26th-anniversary-game-competition.24626/).

## Screenshots
![Sega Saturn Multiplayer Task Force](screenshots/ssmtf.png)
![Twelve Snakes Title](screenshots/title.png)
![Multiplayer](screenshots/multiplayer.png)
![Solo](screenshots/solo.png)
![Score](screenshots/score.png)

## Videos
* ![Razor & Zenon Sonic Videos](https://www.youtube.com/watch?v=rHCEwnGYncY)
* ![Sega Pirate Channel](https://www.youtube.com/watch?v=WbvJkOMGJsg)

## How to Play
* On the title screen you can select the number of lives per player (1, 3, 5, 9, or infinite) as well as the starting position of the Flickies (fixed or random). During gameplay the order of the Flickies do not change.
* Once the game starts all twelve birds will spawn. Press A, B, or C to start flapping your character
* You can change your Flicky by pressing Left or Right Trigger while alive.
* You score 1 point for each pipe you traverse
* If you touch the ground or a pipe your character dies. Dying loses you 1 point
* To respawn after dying simply flap again (A, B, or C button). You can keep respawning until you run out of lives
* The game ends when a player scores 100 points or if no player is playing for 5 seconds
* The game starts off easy but gets harder every 10 points scored

## Power-Ups/Power-Downs
![Power-Ups](screenshots/powerups.png)
There are five power-ups/power-downs that spawn randomly. Going after and acquiring them is a risk/reward trade-off.

* Flicky - extra life
* Robotnik - instant death
* Lightning - all players shrink and have floatier jumps for 10 seconds
* Reverse Gravity - gravity reverses for all players for 10 seconds
* Stone Shoes - all players have higher gravity for 10 seconds

## Player One Special Commands
Only player one can:
- interact with the menus
- pause/display the score with the Start button
- clear scores with the Z button (at the pause screen)
- press ABC + Start to reset the game

## Score
When player one hit starts or the game ends, the score is displayed. The fields mean the following:

### R
The player's rank among the other players. Can be 1-12. The players are ranked via their score.
### C
The Flicky
### S
Total score. Score = number of pipes - number of deaths. Score cannot be negative.
### P
Number of pipes the player has cleared.
### D
Number of times the player has died.

## Recommended Music Tracks
You must supply your own music tracks when burning the ISO. Here are some recommendations:
1) Track 1 - title screen music
2) Track 2 - extra life music
3) Track 3 - gameplay music
4) Track 4 - game over music

## Burning
On Linux I was able to burn the ISO/CUE + WAV with: cdrdao write --force "START GAME.CUE".

**Important**: When running in an emulator, always load `START GAME.CUE` (not `game.iso` directly) to get CD audio music playback. The ISO alone contains only the data track.

## Issues
- Slow startup time
- Sound track not included
- Not as crazy as [Twelve Snakes](https://github.com/slinga-homebrew/Twelve-Snakes)

## Building
Requires Jo Engine to build. Run `build.bat` (Windows) to compile, create the ISO, generate the CUE sheet, and package everything into the `build/` folder. The build generates `game.iso` and `START GAME.CUE` which references the audio tracks.

## Credits
Thank you to [Ponut](https://github.com/ponut64) for performance and PCM help
Thank you to [Emerald Nova](www.emeraldnova.com) for organizing the Saturn Dev contest
[SegaXtreme](http://www.segaxtreme.net/) - The best Sega Saturn development forum on the web. Thank you for all the advice from all the great posters on the forum.
[Sega Saturn Multiplayer Task Force](http://vieille.merde.free.fr/) - Other great Sega Saturn games with source code
[Jo Engine](https://github.com/johannes-fetz/joengine) - Sega Saturn dev environment
