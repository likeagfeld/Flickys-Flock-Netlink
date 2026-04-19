#pragma once

/* Game states - each has its own input/update/draw routines */
#define GAMESTATE_UNINITIALIZED  0
#define GAMESTATE_SSMTF_LOGO     1
#define GAMESTATE_TITLE_SCREEN   2
#define GAMESTATE_NAME_ENTRY     3
#define GAMESTATE_CONNECTING     4
#define GAMESTATE_LOBBY          5
#define GAMESTATE_GAMEPLAY       6
#define GAMESTATE_PAUSED         7
#define GAMESTATE_GAME_OVER      8
#define GAMESTATE_VICTORY        9
