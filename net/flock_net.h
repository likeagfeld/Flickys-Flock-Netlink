/**
 * flock_net.h - Flicky's Flock Networking State Machine
 *
 * Manages the full network lifecycle: modem detection, connection,
 * authentication, lobby, and in-game input relay.
 *
 * Architecture follows the Disasteroids/Coup pattern:
 *   Saturn + NetLink --phone--> USB Modem --serial--> Bridge --TCP--> Server
 */

#ifndef FLOCK_NET_H
#define FLOCK_NET_H

#include <stdint.h>
#include <stdbool.h>
#include "net_transport.h"
#include "flock_protocol.h"

/*============================================================================
 * Constants
 *============================================================================*/

#define FNET_MAX_PLAYERS       12    /* Online mode: up to 12 players */
#define FNET_MAX_NAME          16
#define FNET_HEARTBEAT_INTERVAL 600  /* frames (~10 sec at 60fps) */
#define FNET_AUTH_TIMEOUT       300  /* frames (~5 sec) */
#define FNET_AUTH_MAX_RETRIES   5
#define FNET_MAX_PACKETS_FRAME  24   /* Max packets to process per frame */
#define FNET_INPUT_BUFFER_PER_PLAYER 8  /* Frames of input to buffer per player */
#define FNET_LEADERBOARD_MAX   10    /* Max leaderboard entries */

/*============================================================================
 * Network State Machine
 *============================================================================*/

typedef enum {
    FNET_STATE_OFFLINE = 0,     /* No network, local play only */
    FNET_STATE_CONNECTING,      /* Modem dialing */
    FNET_STATE_AUTHENTICATING,  /* Sent CONNECT, waiting for WELCOME */
    FNET_STATE_USERNAME,        /* Server requested username */
    FNET_STATE_LOBBY,           /* In lobby, waiting for players */
    FNET_STATE_PLAYING,         /* In-game, relaying inputs */
    FNET_STATE_DISCONNECTED,    /* Connection lost */
} fnet_state_t;

/*============================================================================
 * Lobby Player Info
 *============================================================================*/

typedef struct {
    uint8_t id;
    char    name[FNET_MAX_NAME + 1];
    bool    ready;
    bool    active;
    uint8_t sprite_id;    /* Selected bird color (0-11) */
} fnet_lobby_player_t;

/*============================================================================
 * Game Roster (survives lobby state transition for results screen)
 *============================================================================*/

typedef struct {
    uint8_t id;                       /* game_player_id */
    char    name[FNET_MAX_NAME + 1];
    bool    active;
    uint8_t sprite_id;                /* Bird color used in game */
} fnet_roster_entry_t;

/*============================================================================
 * Leaderboard Entry
 *============================================================================*/

typedef struct {
    char     name[FNET_MAX_NAME + 1];
    uint16_t wins;
    uint16_t best_score;
    uint16_t games_played;
} fnet_leaderboard_entry_t;

/*============================================================================
 * Remote Input Buffer
 *============================================================================*/

typedef struct {
    uint16_t frame_num;
    uint8_t  input_bits;
    uint8_t  player_id;
    bool     valid;
} fnet_input_entry_t;

/*============================================================================
 * Network State
 *============================================================================*/

typedef struct {
    /* Connection state */
    fnet_state_t state;
    bool modem_available;

    /* Transport */
    const net_transport_t* transport;

    /* RX state machine */
    fnet_rx_state_t rx;
    uint8_t rx_buf[FNET_RX_FRAME_SIZE];
    uint8_t tx_buf[FNET_TX_FRAME_SIZE];

    /* Auth */
    char my_uuid[SNCP_UUID_LEN + 4];
    bool has_uuid;
    uint8_t my_player_id;
    int auth_timer;
    int auth_retries;

    /* Username */
    char my_name[FNET_MAX_NAME + 1];

    /* Lobby */
    fnet_lobby_player_t lobby_players[FNET_MAX_PLAYERS];
    int lobby_count;
    bool my_ready;

    /* Game roster */
    fnet_roster_entry_t game_roster[FNET_MAX_PLAYERS];
    int game_roster_count;

    /* Game config (from server GAME_START) */
    uint32_t game_seed;
    uint8_t  num_lives;
    uint8_t  start_pos;     /* 0=fixed, 1=random */
    uint8_t  opponent_count;

    /* In-game input relay -- per-player ring buffers */
    fnet_input_entry_t remote_inputs[FNET_MAX_PLAYERS][FNET_INPUT_BUFFER_PER_PLAYER];
    int remote_input_head[FNET_MAX_PLAYERS];
    uint16_t local_frame;

    /* My selected bird sprite */
    uint8_t my_sprite;

    /* Username retry (for duplicate name handling) */
    int username_retry;

    /* Delta compression */
    uint8_t last_sent_input;   /* Last input bits sent to server */
    uint16_t send_cooldown;    /* Frames since last send (force at 15) */

    /* Player state sync */
    int player_state_cooldown;  /* Frames since last PLAYER_STATE sent */

    /* P2 local co-op delta compression */
    uint8_t last_sent_input_p2;
    uint16_t send_cooldown_p2;
    int player_state_cooldown_p2;

    /* Timers */
    int heartbeat_counter;
    int frame_count;

    /* Connection status messages */
    const char* status_msg;
    int connect_stage;

    /* Log messages for display */
    char log_lines[4][40];
    int  log_count;

    /* Last game winner */
    uint8_t last_winner_id;
    bool    has_last_results;

    /* Online leaderboard (from server) */
    fnet_leaderboard_entry_t leaderboard[FNET_LEADERBOARD_MAX];
    int leaderboard_count;

} fnet_state_data_t;

/*============================================================================
 * Public API
 *============================================================================*/

/** Initialize network state (call once at startup). */
void fnet_init(void);

/** Set modem availability (call after hardware detection). */
void fnet_set_modem_available(bool available);

/** Set the transport (call after successful modem connection). */
void fnet_set_transport(const net_transport_t* transport);

/** Set username for online play. */
void fnet_set_username(const char* name);

/** Get current network state. */
fnet_state_t fnet_get_state(void);

/** Get pointer to full state (for rendering). */
const fnet_state_data_t* fnet_get_data(void);

/**
 * Called when modem connection established.
 * Sends CONNECT message to server, transitions to AUTHENTICATING.
 */
void fnet_on_connected(void);

/**
 * Network tick -- call every frame.
 * Polls for incoming messages, processes them, sends heartbeat.
 */
void fnet_tick(void);

/**
 * Send local player input with delta compression.
 * Only transmits when input changes or every 15 frames as keepalive.
 */
void fnet_send_input_delta(uint16_t frame_num, uint8_t input_bits);

/** Toggle ready state in lobby. */
void fnet_send_ready(void);

/** Request game start (from lobby). */
void fnet_send_start_game(void);

/** Send disconnect and clean up. */
void fnet_send_disconnect(void);

/** Request pause toggle. */
void fnet_send_pause(void);

/** Add a log message (visible on connecting/lobby screens). */
void fnet_log(const char* msg);

/** Enter offline mode (no modem or connection failed). */
void fnet_enter_offline(void);

/** Send local player state to server (throttled to every 4 frames). */
void fnet_send_player_state(void);

/** Clear log lines (call when entering lobby to remove stale text). */
void fnet_clear_log(void);

/** Request leaderboard data from server. */
void fnet_request_leaderboard(void);

/** Send sprite selection to server. */
void fnet_send_sprite_select(uint8_t sprite_id);

/** Request server to add one bot. */
void fnet_send_bot_add(void);

/** Request server to remove one bot. */
void fnet_send_bot_remove(void);

/** Get most recent remote input for a player. Returns -1 if none. */
int fnet_get_remote_input(uint16_t frame_num, uint8_t player_id);

/** Register P2 local co-op player with server. */
void fnet_send_add_local_player(const char* name);

/** Deregister P2 local co-op player from server. */
void fnet_send_remove_local_player(void);

/** Send P2 co-op input with delta compression. */
void fnet_send_input_delta_p2(uint16_t frame_num, uint8_t input_bits);

/** Send P2 co-op player state to server (throttled). */
void fnet_send_player_state_p2(void);

/** Report local player death to server (client-authoritative collision). */
void fnet_send_player_death(void);

/** Report P2 co-op player death to server. */
void fnet_send_player_death_p2(void);

/** Report local player collected a powerup to server. */
void fnet_send_powerup_collect(int slot);

#endif /* FLOCK_NET_H */
