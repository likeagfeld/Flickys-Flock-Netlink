/**
 * flock_net.c - Flicky's Flock Networking State Machine
 *
 * Handles the full network lifecycle following the same patterns
 * as the Disasteroids/Coup networking implementation.
 */

#include <string.h>
#include "flock_net.h"
#include "../main.h"

/*============================================================================
 * Static State
 *============================================================================*/

static fnet_state_data_t g_net;

/*============================================================================
 * Forward declarations for game functions (defined in main.c)
 *============================================================================*/

extern FLICKY g_Players[];
extern PIPE g_Pipes[];
extern POWERUP g_PowerUps[];

extern void killPlayer(int playerID);
extern void spawnPlayer(int playerID, bool deductLife);
extern void applyPowerUp(PFLICKY player, PPOWERUP powerup);
extern int getNextFlickySprite(int spriteID, int offset);

/*============================================================================
 * Init / Config
 *============================================================================*/

void fnet_init(void)
{
    memset(&g_net, 0, sizeof(g_net));
    g_net.state = FNET_STATE_OFFLINE;
    g_net.modem_available = false;
    g_net.transport = (void*)0;
    g_net.status_msg = "Offline";
    fnet_rx_init(&g_net.rx, g_net.rx_buf, sizeof(g_net.rx_buf));
}

void fnet_set_modem_available(bool available)
{
    g_net.modem_available = available;
}

void fnet_set_transport(const net_transport_t* transport)
{
    g_net.transport = transport;
}

void fnet_set_username(const char* name)
{
    int i;
    for (i = 0; i < FNET_MAX_NAME && name[i]; i++)
        g_net.my_name[i] = name[i];
    g_net.my_name[i] = '\0';
}

fnet_state_t fnet_get_state(void)
{
    return g_net.state;
}

const fnet_state_data_t* fnet_get_data(void)
{
    return &g_net;
}

/*============================================================================
 * Logging
 *============================================================================*/

void fnet_log(const char* msg)
{
    int i;
    int dst;

    if (g_net.log_count < 4) {
        dst = g_net.log_count;
    } else {
        /* Shift log lines up */
        for (i = 0; i < 3; i++) {
            memcpy(g_net.log_lines[i], g_net.log_lines[i + 1], 40);
        }
        dst = 3;
    }

    for (i = 0; i < 39 && msg[i]; i++)
        g_net.log_lines[dst][i] = msg[i];
    g_net.log_lines[dst][i] = '\0';

    if (g_net.log_count < 4)
        g_net.log_count++;
}

void fnet_clear_log(void)
{
    memset(g_net.log_lines, 0, sizeof(g_net.log_lines));
    g_net.log_count = 0;
}

/*============================================================================
 * State Transitions
 *============================================================================*/

void fnet_enter_offline(void)
{
    g_net.state = FNET_STATE_OFFLINE;
    g_net.status_msg = "Offline";
}

void fnet_on_connected(void)
{
    int len;

    /* Reset RX state for fresh connection */
    fnet_rx_init(&g_net.rx, g_net.rx_buf, sizeof(g_net.rx_buf));

    g_net.state = FNET_STATE_AUTHENTICATING;
    g_net.status_msg = "Authenticating...";
    g_net.auth_timer = 0;
    g_net.auth_retries = 0;
    g_net.heartbeat_counter = 0;

    /* Send CONNECT (with UUID if we have one for reconnection) */
    if (g_net.has_uuid) {
        len = fnet_encode_connect_uuid(g_net.tx_buf, g_net.my_uuid);
    } else {
        len = fnet_encode_connect(g_net.tx_buf);
    }
    net_transport_send(g_net.transport, g_net.tx_buf, len);

    fnet_log("Sent CONNECT");
}

/*============================================================================
 * Byte-read helpers (big-endian)
 *============================================================================*/

static inline int16_t read_i16(const uint8_t* p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint16_t read_u16(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/*============================================================================
 * Message Processing
 *============================================================================*/

static void process_welcome(const uint8_t* payload, int len)
{
    int off;
    int i;

    if (len < 2) return;

    /* [uid:1][uuid:36][name_len:1][name:N] */
    g_net.my_player_id = payload[1];
    off = 2;

    /* Read UUID */
    if (off + SNCP_UUID_LEN <= len) {
        for (i = 0; i < SNCP_UUID_LEN; i++)
            g_net.my_uuid[i] = (char)payload[off + i];
        g_net.my_uuid[SNCP_UUID_LEN] = '\0';
        g_net.has_uuid = true;
        off += SNCP_UUID_LEN;
    }

    /* Read back username if provided */
    if (off < len) {
        fnet_read_string(&payload[off], len - off,
                         g_net.my_name, FNET_MAX_NAME + 1);
    }

    g_net.state = FNET_STATE_LOBBY;
    g_net.status_msg = "In Lobby";
    fnet_log("Welcome! You are Player");

    /* If we have a second local player, re-register them */
    if (g_Game.hasSecondLocal && g_Game.playerName2[0] != '\0') {
        int slen = fnet_encode_add_local_player(g_net.tx_buf,
                                                 g_Game.playerName2);
        net_transport_send(g_net.transport, g_net.tx_buf, slen);
        fnet_log("Registering Player 2...");
    }
}

static void process_username_required(void)
{
    int len;

    g_net.state = FNET_STATE_USERNAME;
    g_net.status_msg = "Enter username";

    /* If we already have a name set, send it immediately */
    if (g_net.my_name[0] != '\0') {
        len = fnet_encode_set_username(g_net.tx_buf, g_net.my_name);
        net_transport_send(g_net.transport, g_net.tx_buf, len);
        g_net.state = FNET_STATE_AUTHENTICATING;
        g_net.status_msg = "Authenticating...";
    }
}

static void process_lobby_state(const uint8_t* payload, int len)
{
    int off, i, consumed;

    if (len < 2) return;

    g_net.lobby_count = payload[1];
    if (g_net.lobby_count > FNET_MAX_PLAYERS)
        g_net.lobby_count = FNET_MAX_PLAYERS;

    off = 2;
    for (i = 0; i < g_net.lobby_count && off < len; i++) {
        if (off >= len) break;
        g_net.lobby_players[i].id = payload[off++];

        consumed = fnet_read_string(&payload[off], len - off,
                                     g_net.lobby_players[i].name,
                                     FNET_MAX_NAME + 1);
        if (consumed < 0) break;
        off += consumed;

        if (off < len)
            g_net.lobby_players[i].ready = (payload[off++] != 0);

        /* Read sprite_id (extended field) */
        if (off < len)
            g_net.lobby_players[i].sprite_id = payload[off++];
        else
            g_net.lobby_players[i].sprite_id = (uint8_t)i;

        g_net.lobby_players[i].active = true;
    }

    /* Clear remaining slots */
    for (; i < FNET_MAX_PLAYERS; i++) {
        g_net.lobby_players[i].active = false;
    }
}

static void process_game_start(const uint8_t* payload, int len)
{
    if (len < 8) return;

    /* [seed:4 BE][my_player_id:1][opponent_count:1][num_lives:1][start_pos:1] */
    g_net.game_seed = ((uint32_t)payload[1] << 24)
                    | ((uint32_t)payload[2] << 16)
                    | ((uint32_t)payload[3] << 8)
                    | ((uint32_t)payload[4]);
    g_net.my_player_id = payload[5];
    g_net.opponent_count = payload[6];
    g_net.num_lives = 3; /* default */
    if (len > 7) g_net.num_lives = payload[7];
    g_net.start_pos = 0; /* default fixed */
    if (len > 8) g_net.start_pos = payload[8];

    /* Validate player ID */
    if (g_net.my_player_id >= MAX_PLAYERS) {
        fnet_log("Bad player ID from server!");
        g_net.state = FNET_STATE_DISCONNECTED;
        g_net.status_msg = "Server error";
        return;
    }

    g_net.has_last_results = false;

    g_net.state = FNET_STATE_PLAYING;
    g_net.status_msg = "Playing";
    g_net.local_frame = 0;
    g_net.last_sent_input = 0;
    g_net.send_cooldown = 15; /* Force immediate send on first frame */
    g_net.player_state_cooldown = 4; /* Force immediate state send */

    /* P2 co-op delta compression init */
    g_net.last_sent_input_p2 = 0;
    g_net.send_cooldown_p2 = 15;
    g_net.player_state_cooldown_p2 = 4;

    /* Clear per-player input buffers */
    memset(g_net.remote_inputs, 0, sizeof(g_net.remote_inputs));
    memset(g_net.remote_input_head, 0, sizeof(g_net.remote_input_head));

    /* Clear lobby roster -- server will resend via PLAYER_JOIN */
    memset(g_net.lobby_players, 0, sizeof(g_net.lobby_players));
    g_net.lobby_count = 0;

    /* Clear game roster */
    memset(g_net.game_roster, 0, sizeof(g_net.game_roster));
    g_net.game_roster_count = 0;

    fnet_log("Game starting!");
}

static void process_input_relay(const uint8_t* payload, int len)
{
    uint8_t player_id;
    uint16_t frame_num;
    uint8_t input_bits;
    int idx;

    if (len < 5) return;

    /* [player_id:1][frame:2 BE][input:1] */
    player_id = payload[1];
    frame_num = ((uint16_t)payload[2] << 8) | (uint16_t)payload[3];
    input_bits = payload[4];

    /* Don't store our own input (server echoes to all) */
    if (player_id == g_net.my_player_id) return;
    /* Don't store P2 echo either */
    if (g_Game.hasSecondLocal && player_id == g_Game.myPlayerID2) return;
    if (player_id >= FNET_MAX_PLAYERS) return;

    /* Store in per-player ring buffer */
    idx = g_net.remote_input_head[player_id] % FNET_INPUT_BUFFER_PER_PLAYER;
    g_net.remote_inputs[player_id][idx].frame_num = frame_num;
    g_net.remote_inputs[player_id][idx].input_bits = input_bits;
    g_net.remote_inputs[player_id][idx].player_id = player_id;
    g_net.remote_inputs[player_id][idx].valid = true;
    g_net.remote_input_head[player_id]++;
}

static void process_game_over(const uint8_t* payload, int len)
{
    fnet_log("Game Over!");

    if (len >= 2) {
        g_net.last_winner_id = payload[1];
    } else {
        g_net.last_winner_id = 0xFF;
    }
    g_net.has_last_results = true;

    /* Return network state to lobby */
    g_net.state = FNET_STATE_LOBBY;
    g_net.status_msg = "In Lobby";
}

static void process_log(const uint8_t* payload, int len)
{
    char msg[40];
    if (len < 2) return;
    fnet_read_string(&payload[1], len - 1, msg, sizeof(msg));
    fnet_log(msg);
}

static void process_player_sync(const uint8_t* payload, int len)
{
    uint8_t pid;
    int16_t y, y_speed;
    uint8_t state, sprite;
    uint16_t points, deaths;

    /* [type:1][player_id:1][y:2s][y_speed:2s][state:1][points:2][deaths:2][sprite:1] = 12 */
    if (len < 12) return;

    pid = payload[1];
    if (pid >= MAX_PLAYERS) return;
    if (pid == g_net.my_player_id) return; /* ignore own echo */
    if (g_Game.hasSecondLocal && pid == g_Game.myPlayerID2) return; /* ignore P2 echo */

    y = read_i16(&payload[2]);
    y_speed = read_i16(&payload[4]);
    state = payload[6];
    points = read_u16(&payload[7]);
    deaths = read_u16(&payload[9]);
    sprite = payload[11];

    /* Update remote player state */
    g_Players[pid].y_pos = (int)y;
    g_Players[pid].y_speed = (int)y_speed;
    g_Players[pid].numPoints = (int)points;
    g_Players[pid].numDeaths = (int)deaths;
    g_Players[pid].totalScore = (int)points - (int)deaths;
    if (g_Players[pid].totalScore < 0) g_Players[pid].totalScore = 0;
    g_Players[pid].spriteID = (int)sprite % MAX_FLICKY_SPRITES;
}

static void process_pipe_spawn(const uint8_t* payload, int len)
{
    uint8_t slot;
    int16_t x, y, top_y;
    uint8_t gap, sections;

    /* [type:1][slot:1][x:2][y:2s][gap:1][sections:1][top_y:2s] = 10 */
    if (len < 10) return;

    slot = payload[1];
    if (slot >= MAX_PIPES) return;

    x = read_i16(&payload[2]);
    y = read_i16(&payload[4]);
    gap = payload[6];
    sections = payload[7];
    top_y = read_i16(&payload[8]);

    g_Pipes[slot].state = PIPESTATE_INITIALIZED;
    g_Pipes[slot].x_pos = (int)x;
    g_Pipes[slot].y_pos = (int)y;
    g_Pipes[slot].gap = (int)gap;
    g_Pipes[slot].numSections = (int)sections;
    g_Pipes[slot].top_y_pos = (int)top_y;
    g_Pipes[slot].z_pos = 500;
    g_Pipes[slot].scoredBy = 0;
}

static void process_powerup_spawn(const uint8_t* payload, int len)
{
    uint8_t slot, type;
    int16_t x, y;

    /* [type:1][slot:1][pu_type:1][x:2][y:2s] = 7 */
    if (len < 7) return;

    slot = payload[1];
    if (slot >= MAX_POWER_UPS) return;

    type = payload[2];
    x = read_i16(&payload[3]);
    y = read_i16(&payload[5]);

    g_PowerUps[slot].state = POWERUP_INITIALIZED;
    g_PowerUps[slot].type = (int)type;
    g_PowerUps[slot].x_pos = (int)x;
    g_PowerUps[slot].y_pos = (int)y;
    g_PowerUps[slot].z_pos = 500;
}

static void process_player_kill(const uint8_t* payload, int len)
{
    uint8_t pid;

    /* [type:1][player_id:1] = 2 */
    if (len < 2) return;

    pid = payload[1];
    if (pid >= MAX_PLAYERS) return;

    /* Client-authoritative: ignore server kills for local player(s).
     * Local player deaths are detected by client-side collision and
     * reported to server via CLIENT_DEATH. Server relays to others. */
    if (pid == g_net.my_player_id) return;
    if (g_Game.hasSecondLocal && pid == g_Game.myPlayerID2) return;

    killPlayer((int)pid);
}

static void process_player_spawn(const uint8_t* payload, int len)
{
    uint8_t pid;

    /* [type:1][player_id:1] = 2 */
    if (len < 2) return;

    pid = payload[1];
    if (pid >= MAX_PLAYERS) return;

    spawnPlayer((int)pid, false);
}

static void process_score_update(const uint8_t* payload, int len)
{
    uint8_t pid;
    uint16_t points, deaths;

    /* [type:1][player_id:1][points:2][deaths:2] = 6 */
    if (len < 6) return;

    pid = payload[1];
    if (pid >= MAX_PLAYERS) return;

    points = read_u16(&payload[2]);
    deaths = read_u16(&payload[4]);

    /* Track point changes to increase progressive speed */
    {
        int old_points = g_Players[pid].numPoints;
        int new_points = (int)points;
        if (new_points > old_points) {
            /* +64 per gate in fixed-point 8.8 (~25% of base per gate) */
            g_Game.pipeSpeed += 64 * (new_points - old_points);
        }
    }

    g_Players[pid].numPoints = (int)points;
    g_Players[pid].numDeaths = (int)deaths;
    g_Players[pid].totalScore = (int)points - (int)deaths;
    if (g_Players[pid].totalScore < 0) g_Players[pid].totalScore = 0;
}

static void process_powerup_effect(const uint8_t* payload, int len)
{
    uint8_t type, picker_id;
    int i;

    /* [type:1][pu_type:1][picker_id:1] = 3 */
    if (len < 3) return;

    type = payload[1];
    picker_id = payload[2];

    /* Apply powerup effects based on type */
    switch (type) {
    case POWERUP_ONE_UP:
        if (picker_id < MAX_PLAYERS) {
            /* Give extra life to picker */
            if (g_Players[picker_id].numDeaths > 0) {
                g_Players[picker_id].numDeaths--;
            }
        }
        break;

    case POWERUP_REVERSE_GRAVITY:
        /* Apply to all flying players */
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (g_Players[i].state == FLICKYSTATE_FLYING) {
                g_Players[i].reverseGravityTimer = REVERSE_GRAVITY_TIMER;
            }
        }
        break;

    case POWERUP_LIGHTNING:
        /* Apply to all flying players */
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (g_Players[i].state == FLICKYSTATE_FLYING) {
                g_Players[i].lightningTimer = LIGHTNING_TIMER;
            }
        }
        break;

    case POWERUP_ROBOTNIK:
        /* Kill the picker */
        if (picker_id < MAX_PLAYERS) {
            killPlayer((int)picker_id);
        }
        break;

    case POWERUP_STONE_SNEAKERS:
        /* Apply to all flying players */
        for (i = 0; i < MAX_PLAYERS; i++) {
            if (g_Players[i].state == FLICKYSTATE_FLYING) {
                g_Players[i].stoneSneakersTimer = STONE_SNEAKERS_TIMER;
            }
        }
        break;
    }
}

static void process_leaderboard(const uint8_t* payload, int len)
{
    int off, i, nlen, copy;

    /* [type:1][count:1]{name_len:1, name:N, wins:2BE, best:2BE, gp:2BE}... */
    if (len < 2) return;

    g_net.leaderboard_count = payload[1];
    if (g_net.leaderboard_count > FNET_LEADERBOARD_MAX)
        g_net.leaderboard_count = FNET_LEADERBOARD_MAX;

    off = 2;
    for (i = 0; i < g_net.leaderboard_count && off < len; i++) {
        if (off >= len) break;
        nlen = payload[off++];
        copy = (nlen < FNET_MAX_NAME) ? nlen : FNET_MAX_NAME;
        if (off + nlen + 6 > len) { g_net.leaderboard_count = i; break; }
        memcpy(g_net.leaderboard[i].name, &payload[off], copy);
        g_net.leaderboard[i].name[copy] = '\0';
        off += nlen;
        g_net.leaderboard[i].wins = read_u16(&payload[off]); off += 2;
        g_net.leaderboard[i].best_score = read_u16(&payload[off]); off += 2;
        g_net.leaderboard[i].games_played = read_u16(&payload[off]); off += 2;
    }
}

static void process_message(const uint8_t* payload, int len)
{
    uint8_t msg_type;

    if (len < 1) return;
    msg_type = payload[0];

    switch (msg_type) {
    case SNCP_MSG_WELCOME:
    case SNCP_MSG_WELCOME_BACK:
        process_welcome(payload, len);
        break;

    case SNCP_MSG_USERNAME_REQUIRED:
        process_username_required();
        break;

    case SNCP_MSG_USERNAME_TAKEN:
    {
        if (g_net.username_retry < 9) {
            int nlen = 0;
            int slen;
            g_net.username_retry++;
            while (g_net.my_name[nlen] && nlen < FNET_MAX_NAME) nlen++;
            if (nlen > 0 && g_net.my_name[nlen - 1] >= '1' &&
                g_net.my_name[nlen - 1] <= '9') {
                nlen--;
            }
            if (nlen < FNET_MAX_NAME) {
                g_net.my_name[nlen] = '0' + g_net.username_retry;
                g_net.my_name[nlen + 1] = '\0';
            }
            slen = fnet_encode_set_username(g_net.tx_buf, g_net.my_name);
            net_transport_send(g_net.transport, g_net.tx_buf, slen);
            g_net.state = FNET_STATE_AUTHENTICATING;
            fnet_log("Name taken, trying...");
        } else {
            fnet_log("All names taken!");
            g_net.state = FNET_STATE_DISCONNECTED;
            g_net.status_msg = "Name unavailable";
        }
        break;
    }

    case FNET_MSG_LOBBY_STATE:
        process_lobby_state(payload, len);
        break;

    case FNET_MSG_GAME_START:
        process_game_start(payload, len);
        break;

    case FNET_MSG_INPUT_RELAY:
        process_input_relay(payload, len);
        break;

    case FNET_MSG_GAME_OVER:
        process_game_over(payload, len);
        break;

    case FNET_MSG_PAUSE_ACK:
        /* Game code handles actual pause state change */
        break;

    case FNET_MSG_LOG:
        process_log(payload, len);
        break;

    case FNET_MSG_PLAYER_JOIN:
    {
        if (len >= 2) {
            uint8_t pid = payload[1];
            int slot;
            int target = -1;
            for (slot = 0; slot < FNET_MAX_PLAYERS; slot++) {
                if (g_net.lobby_players[slot].active &&
                    g_net.lobby_players[slot].id == pid) {
                    target = slot;
                    break;
                }
            }
            if (target < 0) {
                for (slot = 0; slot < FNET_MAX_PLAYERS; slot++) {
                    if (!g_net.lobby_players[slot].active) {
                        target = slot;
                        break;
                    }
                }
            }
            if (target >= 0) {
                int name_consumed = 0;
                uint8_t join_sprite = pid; /* default */
                g_net.lobby_players[target].id = pid;
                g_net.lobby_players[target].active = true;
                if (len >= 3) {
                    name_consumed = fnet_read_string(&payload[2], len - 2,
                                     g_net.lobby_players[target].name,
                                     FNET_MAX_NAME + 1);
                    if (name_consumed < 0) name_consumed = 0;
                }
                /* Read sprite_id after name (extended PLAYER_JOIN) */
                if (2 + name_consumed < len) {
                    join_sprite = payload[2 + name_consumed];
                }
                g_net.lobby_players[target].sprite_id = join_sprite;
                if (target >= g_net.lobby_count)
                    g_net.lobby_count = target + 1;

                /* Set sprite on the actual player */
                if (pid < MAX_PLAYERS) {
                    g_Players[pid].spriteID = (int)(join_sprite % MAX_FLICKY_SPRITES);
                }
            }

            /* Also store in game_roster */
            {
                int rt = -1;
                uint8_t roster_sprite = pid;
                for (slot = 0; slot < FNET_MAX_PLAYERS; slot++) {
                    if (g_net.game_roster[slot].active &&
                        g_net.game_roster[slot].id == pid) {
                        rt = slot; break;
                    }
                }
                if (rt < 0) {
                    for (slot = 0; slot < FNET_MAX_PLAYERS; slot++) {
                        if (!g_net.game_roster[slot].active) {
                            rt = slot; break;
                        }
                    }
                }
                if (rt >= 0) {
                    int rc = 0;
                    g_net.game_roster[rt].id = pid;
                    g_net.game_roster[rt].active = true;
                    if (len >= 3) {
                        rc = fnet_read_string(&payload[2], len - 2,
                                         g_net.game_roster[rt].name,
                                         FNET_MAX_NAME + 1);
                        if (rc < 0) rc = 0;
                    }
                    if (2 + rc < len) {
                        roster_sprite = payload[2 + rc];
                    }
                    g_net.game_roster[rt].sprite_id = roster_sprite;
                    if (rt >= g_net.game_roster_count)
                        g_net.game_roster_count = rt + 1;
                }
            }
        }
        if (g_net.state == FNET_STATE_LOBBY)
            fnet_log("Player joined!");
        break;
    }

    case FNET_MSG_PLAYER_LEAVE:
        fnet_log("Player left!");
        break;

    case FNET_MSG_PLAYER_SYNC:
        process_player_sync(payload, len);
        break;

    case FNET_MSG_PIPE_SPAWN:
        process_pipe_spawn(payload, len);
        break;

    case FNET_MSG_POWERUP_SPAWN:
        process_powerup_spawn(payload, len);
        break;

    case FNET_MSG_PLAYER_KILL:
        process_player_kill(payload, len);
        break;

    case FNET_MSG_PLAYER_SPAWN:
        process_player_spawn(payload, len);
        break;

    case FNET_MSG_SCORE_UPDATE:
        process_score_update(payload, len);
        break;

    case FNET_MSG_POWERUP_EFFECT:
        process_powerup_effect(payload, len);
        break;

    case FNET_MSG_LEADERBOARD_DATA:
        process_leaderboard(payload, len);
        break;

    case FNET_MSG_LOCAL_PLAYER_ACK:
        /* [type:1][player_id:1] -- server assigned P2 a game slot */
        if (len >= 2 && payload[1] != 0xFF) {
            g_Game.myPlayerID2 = payload[1];
            fnet_log("Player 2 joined!");
        }
        break;

    default:
        break;
    }
}

/*============================================================================
 * Network Tick (call every frame)
 *============================================================================*/

void fnet_tick(void)
{
    int processed;
    int len;

    g_net.frame_count++;

    if (g_net.state == FNET_STATE_OFFLINE ||
        g_net.state == FNET_STATE_DISCONNECTED) {
        return;
    }

    if (!g_net.transport) return;

    /* Process incoming messages (bounded per frame) */
    processed = 0;
    while (processed < FNET_MAX_PACKETS_FRAME) {
        len = fnet_rx_poll(&g_net.rx, g_net.transport);
        if (len <= 0) break;
        process_message(g_net.rx_buf, len);
        processed++;
    }

    /* Auth retry logic */
    if (g_net.state == FNET_STATE_AUTHENTICATING) {
        g_net.auth_timer++;
        if (g_net.auth_timer >= FNET_AUTH_TIMEOUT) {
            g_net.auth_timer = 0;
            g_net.auth_retries++;
            if (g_net.auth_retries >= FNET_AUTH_MAX_RETRIES) {
                fnet_log("Auth failed - timeout");
                g_net.state = FNET_STATE_DISCONNECTED;
                g_net.status_msg = "Auth timeout";
                return;
            }
            /* Retry CONNECT */
            if (g_net.has_uuid) {
                len = fnet_encode_connect_uuid(g_net.tx_buf, g_net.my_uuid);
            } else {
                len = fnet_encode_connect(g_net.tx_buf);
            }
            net_transport_send(g_net.transport, g_net.tx_buf, len);
            fnet_log("Retrying auth...");
        }
    }

    /* Heartbeat */
    g_net.heartbeat_counter++;
    if (g_net.heartbeat_counter >= FNET_HEARTBEAT_INTERVAL) {
        g_net.heartbeat_counter = 0;
        len = fnet_encode_heartbeat(g_net.tx_buf);
        net_transport_send(g_net.transport, g_net.tx_buf, len);
    }
}

/*============================================================================
 * Send Functions
 *============================================================================*/

void fnet_send_input_delta(uint16_t frame_num, uint8_t input_bits)
{
    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;

    if (input_bits != g_net.last_sent_input || g_net.send_cooldown >= 15) {
        int len = fnet_encode_input_state(g_net.tx_buf, frame_num, input_bits);
        net_transport_send(g_net.transport, g_net.tx_buf, len);
        g_net.last_sent_input = input_bits;
        g_net.send_cooldown = 0;
    } else {
        g_net.send_cooldown++;
    }

    g_net.local_frame = frame_num;
}

void fnet_send_ready(void)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    g_net.my_ready = !g_net.my_ready;
    len = fnet_encode_ready(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_start_game(void)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    len = fnet_encode_start_game(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_disconnect(void)
{
    int len;
    if (!g_net.transport) return;
    if (g_net.state == FNET_STATE_OFFLINE) return;
    len = fnet_encode_disconnect(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
    g_net.state = FNET_STATE_DISCONNECTED;
    g_net.status_msg = "Disconnected";
}

void fnet_send_pause(void)
{
    int len;
    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;
    len = fnet_encode_pause(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_player_state(void)
{
    int len;

    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;

    /* Throttle to every 4 frames */
    g_net.player_state_cooldown++;
    if (g_net.player_state_cooldown < 4) return;
    g_net.player_state_cooldown = 0;

    if (g_net.my_player_id >= MAX_PLAYERS) return;
    if (g_Players[g_net.my_player_id].state == FLICKYSTATE_DEAD) return;

    len = fnet_encode_player_state(g_net.tx_buf,
        (int16_t)g_Players[g_net.my_player_id].y_pos,
        (int16_t)g_Players[g_net.my_player_id].y_speed,
        (uint8_t)g_Players[g_net.my_player_id].state,
        (uint8_t)g_Players[g_net.my_player_id].spriteID);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_sprite_select(uint8_t sprite_id)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    g_net.my_sprite = sprite_id;
    len = fnet_encode_sprite_select(g_net.tx_buf, sprite_id);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_bot_add(void)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    len = fnet_encode_bot_add(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_bot_remove(void)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    len = fnet_encode_bot_remove(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_request_leaderboard(void)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    len = fnet_encode_leaderboard_req(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

/*============================================================================
 * P2 Co-op Send Functions
 *============================================================================*/

void fnet_send_add_local_player(const char* name)
{
    int len;
    if (g_net.state != FNET_STATE_LOBBY || !g_net.transport) return;
    len = fnet_encode_add_local_player(g_net.tx_buf, name);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_remove_local_player(void)
{
    int len;
    if (!g_net.transport) return;
    if (g_net.state != FNET_STATE_LOBBY &&
        g_net.state != FNET_STATE_PLAYING) return;
    len = fnet_encode_remove_local_player(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_input_delta_p2(uint16_t frame_num, uint8_t input_bits)
{
    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;
    if (g_Game.myPlayerID2 == 0xFF) return;

    if (input_bits != g_net.last_sent_input_p2 ||
        g_net.send_cooldown_p2 >= 15) {
        int len = fnet_encode_input_state_p2(g_net.tx_buf,
                                              g_Game.myPlayerID2,
                                              frame_num, input_bits);
        net_transport_send(g_net.transport, g_net.tx_buf, len);
        g_net.last_sent_input_p2 = input_bits;
        g_net.send_cooldown_p2 = 0;
    } else {
        g_net.send_cooldown_p2++;
    }
}

void fnet_send_player_state_p2(void)
{
    int len;

    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;
    if (g_Game.myPlayerID2 == 0xFF) return;

    /* Throttle to every 4 frames */
    g_net.player_state_cooldown_p2++;
    if (g_net.player_state_cooldown_p2 < 4) return;
    g_net.player_state_cooldown_p2 = 0;

    if (g_Game.myPlayerID2 >= MAX_PLAYERS) return;
    if (g_Players[g_Game.myPlayerID2].state == FLICKYSTATE_DEAD) return;

    len = fnet_encode_player_state(g_net.tx_buf,
        (int16_t)g_Players[g_Game.myPlayerID2].y_pos,
        (int16_t)g_Players[g_Game.myPlayerID2].y_speed,
        (uint8_t)g_Players[g_Game.myPlayerID2].state,
        (uint8_t)g_Players[g_Game.myPlayerID2].spriteID);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

/*============================================================================
 * Death Reporting (client-authoritative collision)
 *============================================================================*/

void fnet_send_player_death(void)
{
    int len;
    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;
    len = fnet_encode_client_death(g_net.tx_buf);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

void fnet_send_player_death_p2(void)
{
    int len;
    if (g_net.state != FNET_STATE_PLAYING || !g_net.transport) return;
    if (g_Game.myPlayerID2 == 0xFF) return;
    len = fnet_encode_client_death_p2(g_net.tx_buf, g_Game.myPlayerID2);
    net_transport_send(g_net.transport, g_net.tx_buf, len);
}

/*============================================================================
 * Remote Input Query
 *============================================================================*/

int fnet_get_remote_input(uint16_t frame_num, uint8_t player_id)
{
    int i;
    int best = -1;
    uint16_t best_frame = 0;

    if (player_id >= FNET_MAX_PLAYERS) return -1;

    for (i = 0; i < FNET_INPUT_BUFFER_PER_PLAYER; i++) {
        if (!g_net.remote_inputs[player_id][i].valid)
            continue;

        if (g_net.remote_inputs[player_id][i].frame_num == frame_num) {
            return (int)g_net.remote_inputs[player_id][i].input_bits;
        }

        if (best < 0 || g_net.remote_inputs[player_id][i].frame_num > best_frame) {
            best_frame = g_net.remote_inputs[player_id][i].frame_num;
            best = (int)g_net.remote_inputs[player_id][i].input_bits;
        }
    }

    return best;
}
