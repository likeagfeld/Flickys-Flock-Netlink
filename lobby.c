/**
 * lobby.c - Online Lobby Screen
 *
 * Shows up to 12 connected players with ready states, and handles game start.
 * Network messages are processed by fnet_tick() in the main loop.
 *
 * Adapted from Disasteroids lobby.c for Flicky's Flock.
 */

#include <jo/jo.h>
#include "main.h"
#include "lobby.h"
#include "net/flock_net.h"

/*============================================================================
 * Z-button overlay state
 *============================================================================*/

static bool g_z_held = false;
static bool g_z_was_held = false;
static int  g_z_page_timer = 0;
static int  g_z_page = 0;
#define Z_PAGE_INTERVAL 180  /* 3 seconds at 60fps */

/* L/R trigger state for sprite selection */
static bool g_ltrig_pressed = false;
static bool g_rtrig_pressed = false;

/* External sprite data for drawing bird icons */
extern FLICKY_SPRITES g_FlickySprites[MAX_FLICKY_SPRITES];

/*============================================================================
 * Callbacks
 *============================================================================*/

void lobby_init(void)
{
    g_z_held = false;
    g_z_was_held = false;
    g_z_page_timer = 0;
    g_z_page = 0;
    g_ltrig_pressed = false;
    g_rtrig_pressed = false;

    /* Clear stale log text from previous gameplay */
    fnet_clear_log();

    /* Clear VDP2 text plane */
    jo_clear_screen();

    /* Start lobby music (moodmode track, looping) */
    jo_audio_stop_cd();
    jo_audio_play_cd_track(TITLE_TRACK, TITLE_TRACK, true);

    /* Request leaderboard data from server */
    fnet_request_leaderboard();
}

void lobby_input(void)
{
    if (g_Game.gameState != GAMESTATE_LOBBY) return;

    /* A/C = toggle ready */
    if (jo_is_pad1_key_pressed(JO_KEY_A) ||
        jo_is_pad1_key_pressed(JO_KEY_C)) {
        if (g_Game.input.pressedABC == false) {
            fnet_send_ready();
        }
        g_Game.input.pressedABC = true;
    } else {
        g_Game.input.pressedABC = false;
    }

    /* START = request game start (auto-ready if not already).
     * Check authoritative server roster for our ready state rather
     * than relying on the local mirror, which can go stale after
     * a game ends and the server resets ready flags. Otherwise we
     * risk toggling ourselves back to NOT READY on the START press. */
    if (jo_is_pad1_key_pressed(JO_KEY_START)) {
        if (g_Game.input.pressedStart == false) {
            const fnet_state_data_t* nd = fnet_get_data();
            bool server_says_ready = false;
            int k, j;

            /* Find ourselves in the lobby roster by name */
            for (k = 0; k < nd->lobby_count && k < FNET_MAX_PLAYERS; k++) {
                if (!nd->lobby_players[k].active) continue;
                for (j = 0; j < FNET_MAX_NAME; j++) {
                    if (g_Game.playerName[j] != nd->lobby_players[k].name[j]) break;
                    if (g_Game.playerName[j] == '\0') break;
                }
                if (g_Game.playerName[j] == nd->lobby_players[k].name[j]) {
                    server_says_ready = nd->lobby_players[k].ready;
                    break;
                }
            }

            if (!server_says_ready && !fnet_is_ready()) {
                fnet_send_ready();
            }
            fnet_send_start_game();
        }
        g_Game.input.pressedStart = true;
    } else {
        g_Game.input.pressedStart = false;
    }

    /* B = return to title (stay connected for quick rejoin) */
    if (jo_is_pad1_key_pressed(JO_KEY_B)) {
        if (g_Game.input.pressedLT == false) {
            jo_clear_screen();
            jo_audio_stop_cd();
            g_Game.input.pressedABC = true; /* block title screen from re-processing B */
            g_Game.titleScreenChoice = 2;   /* reset cursor away from ONLINE */
            g_Game.gameState = GAMESTATE_TITLE_SCREEN;
            jo_audio_play_cd_track(TITLE_TRACK, TITLE_TRACK, true);
        }
        g_Game.input.pressedLT = true;
    } else {
        g_Game.input.pressedLT = false;
    }

    /* Y = fully disconnect and return to title */
    if (jo_is_pad1_key_pressed(JO_KEY_Y)) {
        if (g_Game.input.pressedRT == false) {
            fnet_send_disconnect();
            jo_clear_screen();
            jo_audio_stop_cd();
            g_Game.input.pressedABC = true;
            g_Game.titleScreenChoice = 2;
            g_Game.gameState = GAMESTATE_TITLE_SCREEN;
            jo_audio_play_cd_track(TITLE_TRACK, TITLE_TRACK, true);
        }
        g_Game.input.pressedRT = true;
    } else {
        g_Game.input.pressedRT = false;
    }

    /* UP = add bot */
    if (jo_is_pad1_key_pressed(JO_KEY_UP)) {
        if (g_Game.input.pressedUp == false) {
            fnet_send_bot_add();
        }
        g_Game.input.pressedUp = true;
    } else {
        g_Game.input.pressedUp = false;
    }

    /* DOWN = remove bot */
    if (jo_is_pad1_key_pressed(JO_KEY_DOWN)) {
        if (g_Game.input.pressedDown == false) {
            fnet_send_bot_remove();
        }
        g_Game.input.pressedDown = true;
    } else {
        g_Game.input.pressedDown = false;
    }

    /* L trigger = previous bird sprite */
    if (jo_is_pad1_key_pressed(JO_KEY_L)) {
        if (!g_ltrig_pressed) {
            const fnet_state_data_t* nd = fnet_get_data();
            uint8_t cur = nd->my_sprite;
            uint8_t next = (cur == 0) ? (MAX_FLICKY_SPRITES - 1) : (cur - 1);
            fnet_send_sprite_select(next);
        }
        g_ltrig_pressed = true;
    } else {
        g_ltrig_pressed = false;
    }

    /* R trigger = next bird sprite */
    if (jo_is_pad1_key_pressed(JO_KEY_R)) {
        if (!g_rtrig_pressed) {
            const fnet_state_data_t* nd = fnet_get_data();
            uint8_t cur = nd->my_sprite;
            uint8_t next = (cur + 1) % MAX_FLICKY_SPRITES;
            fnet_send_sprite_select(next);
        }
        g_rtrig_pressed = true;
    } else {
        g_rtrig_pressed = false;
    }

    /* Z = hold for results/leaderboard overlay */
    g_z_held = jo_is_pad1_key_pressed(JO_KEY_Z) ? true : false;
}

void lobby_update(void)
{
    if (g_Game.gameState != GAMESTATE_LOBBY) return;

    /* P2 controller hot-plug detection */
    if (!g_Game.hasSecondLocal && getP2Port() >= 0) {
        /* Controller 2 just plugged in -- register P2 */
        int i, p2len;
        g_Game.hasSecondLocal = true;
        p2len = 0;
        while (g_Game.playerName[p2len] && p2len < FNET_MAX_NAME) p2len++;
        for (i = 0; i < p2len; i++)
            g_Game.playerName2[i] = g_Game.playerName[i];
        if (p2len < FNET_MAX_NAME) {
            g_Game.playerName2[p2len] = '2';
            g_Game.playerName2[p2len + 1] = '\0';
        } else {
            g_Game.playerName2[FNET_MAX_NAME - 1] = '2';
            g_Game.playerName2[FNET_MAX_NAME] = '\0';
        }
        g_Game.myPlayerID2 = 0xFF;
        fnet_send_add_local_player(g_Game.playerName2);
    } else if (g_Game.hasSecondLocal && getP2Port() < 0) {
        /* Controller 2 unplugged -- remove P2 */
        g_Game.hasSecondLocal = false;
        g_Game.myPlayerID2 = 0xFF;
        g_Game.playerName2[0] = '\0';
        fnet_send_remove_local_player();
    }

    /* Check state transitions */
    if (fnet_get_state() == FNET_STATE_PLAYING) {
        const fnet_state_data_t* nd = fnet_get_data();

        /* Configure game from server settings */
        g_Game.isOnlineMode = true;
        g_Game.myPlayerID = nd->my_player_id;

        /* Map server num_lives to numLivesChoice index */
        switch (nd->num_lives) {
            case 0:  g_Game.numLivesChoice = 0; break; /* infinite */
            case 1:  g_Game.numLivesChoice = 1; break;
            case 3:  g_Game.numLivesChoice = 2; break;
            case 5:  g_Game.numLivesChoice = 3; break;
            case 9:  g_Game.numLivesChoice = 4; break;
            default: g_Game.numLivesChoice = 2; break; /* default 3 lives */
        }
        g_Game.startingPositionChoice = nd->start_pos;

        jo_clear_screen();
        transitionToGameplay(true);
    }

    if (fnet_get_state() == FNET_STATE_DISCONNECTED) {
        jo_clear_screen();
        g_Game.gameState = GAMESTATE_TITLE_SCREEN;
    }
}

/* Look up a player name by game_player_id from game roster */
static const char* lobbyGetPlayerName(int id)
{
    const fnet_state_data_t* nd = fnet_get_data();
    int i;
    for (i = 0; i < nd->game_roster_count && i < FNET_MAX_PLAYERS; i++) {
        if (nd->game_roster[i].active && nd->game_roster[i].id == (uint8_t)id)
            return nd->game_roster[i].name;
    }
    return "";
}

/* Draw Z-overlay: results or leaderboard */
static void draw_z_overlay(const fnet_state_data_t* nd)
{
    int i;

    /* Advance page timer */
    g_z_page_timer++;
    if (g_z_page_timer >= Z_PAGE_INTERVAL) {
        g_z_page_timer = 0;
        g_z_page = 1 - g_z_page;
    }

    if (g_z_page == 0 && nd->has_last_results) {
        /* Page 0: Last Game Results */
        font_draw_centered("LAST GAME RESULTS", FONT_Y(8), 500);
        font_draw("#  NAME             PTS  DTH", FONT_X(2), FONT_Y(9), 500);
        for (i = 0; i < nd->game_roster_count && i < 12; i++) {
            if (nd->game_roster[i].active) {
                int pid = nd->game_roster[i].id;
                const char* name = lobbyGetPlayerName(pid);
                if (name[0] == '\0') name = "???";
                if (pid < MAX_PLAYERS) {
                    font_printf(FONT_X(2), FONT_Y(10 + i), 500,
                                "%-2d %-16s %4d %4d",
                                i + 1, name,
                                g_Players[pid].numPoints % 10000,
                                g_Players[pid].numDeaths % 10000);
                }
            }
        }
    } else {
        /* Leaderboard page */
        font_draw_centered("ONLINE LEADERBOARD", FONT_Y(8), 500);
        if (nd->leaderboard_count > 0) {
            font_draw("#  NAME             W  SCR  GP", FONT_X(2), FONT_Y(9), 500);
            for (i = 0; i < nd->leaderboard_count && i < 12; i++) {
                font_printf(FONT_X(2), FONT_Y(10 + i), 500,
                            "%-2d %-16s %2d %4d %3d",
                            i + 1,
                            nd->leaderboard[i].name,
                            nd->leaderboard[i].wins,
                            nd->leaderboard[i].best_score % 10000,
                            nd->leaderboard[i].games_played % 1000);
            }
        } else {
            font_draw_centered("NO DATA YET", FONT_Y(14), 500);
        }
    }

    /* Page indicator */
    if (nd->has_last_results) {
        font_printf(FONT_X(2), FONT_Y(22), 500,
                    "Z: %s", g_z_page == 0 ? "RESULTS " : "LEADERS ");
    } else {
        font_draw("Z: LEADERS", FONT_X(2), FONT_Y(22), 500);
    }
}

void lobby_draw(void)
{
    const fnet_state_data_t* nd;
    int i;

    if (g_Game.gameState != GAMESTATE_LOBBY) return;

    nd = fnet_get_data();

    /* Title */
    font_draw_centered("LOBBY", FONT_Y(3), 500);

    /* Player count */
    font_printf(FONT_X(2), FONT_Y(6), 500,
                "PLAYERS: %d/%d", nd->lobby_count, FNET_MAX_PLAYERS);

    /* Z overlay check */
    if (g_z_held) {
        draw_z_overlay(nd);
        g_z_was_held = true;
        goto skip_player_list;
    } else if (g_z_was_held) {
        g_z_was_held = false;
        g_z_page_timer = 0;
        g_z_page = 0;
    }

    /* Player list with bird sprites */
    for (i = 0; i < nd->lobby_count && i < FNET_MAX_PLAYERS; i++) {
        int row = 8 + i;
        const char* name = nd->lobby_players[i].name;
        const char* ready_str = nd->lobby_players[i].ready ? "READY" : "---";
        int sid = nd->lobby_players[i].sprite_id % MAX_FLICKY_SPRITES;

        /* Draw bird sprite icon at the start of the row */
        jo_sprite_draw3D(g_FlickySprites[sid].up,
                         FONT_X(1) + 12, FONT_Y(row) + 4, 500);

        /* Text: name and ready state (offset right for sprite) */
        font_printf(FONT_X(5), FONT_Y(row), 500,
                    "%-14s %-5s", name, ready_str);
    }

skip_player_list:

    /* Waiting indicator */
    if (nd->lobby_count < 2) {
        font_draw("WAITING FOR PLAYERS...", FONT_X(5), FONT_Y(23), 500);
    }

    /* Log line */
    if (nd->log_count > 0) {
        font_draw(nd->log_lines[nd->log_count - 1],
                  FONT_X(3), FONT_Y(24), 500);
    }

    /* P2 co-op status */
    if (g_Game.hasSecondLocal) {
        font_printf(FONT_X(2), FONT_Y(25), 500, "P2: %-16s", g_Game.playerName2);
    }

    /* Controls hint */
    font_draw("L/R:BIRD A:RDY START:GO UP/DN:BOTS", FONT_X(1), FONT_Y(26), 500);
    font_draw("B:BACK  Y:QUIT  Z:STATS", FONT_X(1), FONT_Y(27), 500);
}
