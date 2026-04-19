/**
 * flock_protocol.h - Flicky's Flock Network Protocol
 *
 * Binary protocol using SNCP framing for Flicky's Flock multiplayer.
 * Uses the same [LEN_HI][LEN_LO][PAYLOAD...] framing as Disasteroids/Coup.
 * Reuses SNCP auth handshake (CONNECT/WELCOME) for player authentication.
 *
 * Networking model: SERVER-AUTHORITATIVE
 *   Each Saturn sends its local player inputs and player state.
 *   The server owns pipe/powerup spawning, collision detection, scoring.
 *   Saturns run local physics for smooth rendering; server corrects periodically.
 *
 * Header-only: all functions are static inline.
 */

#ifndef FLOCK_PROTOCOL_H
#define FLOCK_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include "net_transport.h"

/*============================================================================
 * SNCP Auth Messages (shared with Coup/Disasteroids)
 *============================================================================*/

#define SNCP_MSG_CONNECT           0x01
#define SNCP_MSG_SET_USERNAME      0x02
#define SNCP_MSG_HEARTBEAT         0x04
#define SNCP_MSG_DISCONNECT        0x05

#define SNCP_MSG_USERNAME_REQUIRED 0x81
#define SNCP_MSG_WELCOME           0x82
#define SNCP_MSG_WELCOME_BACK      0x83
#define SNCP_MSG_USERNAME_TAKEN    0x84

#define SNCP_UUID_LEN              36

/*============================================================================
 * Flicky's Flock Client -> Server Messages (0x10 - 0x1F)
 *============================================================================*/

#define FNET_MSG_READY             0x10  /* Toggle ready state (no payload) */
#define FNET_MSG_INPUT_STATE       0x11  /* Per-frame input [frame:2 BE][input:1] */
#define FNET_MSG_START_GAME_REQ    0x12  /* Request game start (no payload) */
#define FNET_MSG_PAUSE_REQ         0x13  /* Request pause toggle (no payload) */
#define FNET_MSG_PLAYER_STATE      0x14  /* [y:2s][y_speed:2s][state:1][sprite:1] */
#define FNET_MSG_SPRITE_SELECT     0x15  /* [sprite_id:1] - request bird color change */
#define FNET_MSG_BOT_ADD           0x16  /* Request add one bot (no payload) */
#define FNET_MSG_BOT_REMOVE        0x17  /* Request remove one bot (no payload) */
#define FNET_MSG_ADD_LOCAL_PLAYER  0x18  /* [name_len:1][name:N] - register P2 co-op */
#define FNET_MSG_REMOVE_LOCAL_PLAYER 0x19 /* (no payload) - remove P2 co-op */
#define FNET_MSG_INPUT_STATE_P2    0x1A  /* [player_id:1][frame:2 BE][input:1] - P2 input */
#define FNET_MSG_LEADERBOARD_REQ   0x1B  /* Client requests leaderboard (no payload) */
#define FNET_MSG_CLIENT_DEATH      0x1C  /* Client reports own death (no payload) */
#define FNET_MSG_CLIENT_DEATH_P2   0x1D  /* Client reports P2 death [player_id:1] */

/*============================================================================
 * Flicky's Flock Server -> Client Messages (0xA0 - 0xBF)
 *============================================================================*/

#define FNET_MSG_LOBBY_STATE       0xA0  /* [count:1][{id:1,name:LP,ready:1}...] */
#define FNET_MSG_GAME_START        0xA1  /* [seed:4 BE][my_player_id:1][opponent_count:1][num_lives:1][start_pos:1] */
#define FNET_MSG_INPUT_RELAY       0xA2  /* [player_id:1][frame:2 BE][input:1] */
#define FNET_MSG_PLAYER_JOIN       0xA3  /* [id:1][name:LP] */
#define FNET_MSG_PLAYER_LEAVE      0xA4  /* [id:1] */
#define FNET_MSG_GAME_OVER         0xA5  /* [winner_id:1] */
#define FNET_MSG_LOG               0xA6  /* [len:1][text:N] */
#define FNET_MSG_PAUSE_ACK         0xA7  /* [paused:1] */
#define FNET_MSG_PLAYER_SYNC       0xA9  /* [player_id:1][y:2s][y_speed:2s][state:1][points:2][deaths:2][sprite:1] */
#define FNET_MSG_PIPE_SPAWN        0xAA  /* [slot:1][x:2][y:2s][gap:1][sections:1][top_y:2s] */
#define FNET_MSG_POWERUP_SPAWN     0xAC  /* [slot:1][type:1][x:2][y:2s] */
#define FNET_MSG_PLAYER_KILL       0xAE  /* [player_id:1] */
#define FNET_MSG_PLAYER_SPAWN      0xAF  /* [player_id:1] */
#define FNET_MSG_SCORE_UPDATE      0xB0  /* [player_id:1][points:2][deaths:2] */
#define FNET_MSG_POWERUP_EFFECT    0xB1  /* [type:1][picker_id:1] */
#define FNET_MSG_LEADERBOARD_DATA  0xB2  /* [count:1]{name_len:1,name:N,wins:2BE,best:2BE,gp:2BE}... */
#define FNET_MSG_LOCAL_PLAYER_ACK  0xB3  /* [player_id:1] - P2 assigned ID */

/*============================================================================
 * Input State Bitmask (only 4 bits needed for Flicky's Flock)
 *============================================================================*/

#define FNET_INPUT_FLAP    (1 << 0)   /* A/B/C pressed */
#define FNET_INPUT_LTRIG   (1 << 1)   /* L trigger (change character left) */
#define FNET_INPUT_RTRIG   (1 << 2)   /* R trigger (change character right) */
#define FNET_INPUT_START   (1 << 3)   /* START (pause) */

/*============================================================================
 * Buffer Sizes
 *============================================================================*/

#define FNET_RX_FRAME_SIZE  512
#define FNET_TX_FRAME_SIZE  64

/*============================================================================
 * Frame Send/Receive (SNCP framing)
 *============================================================================*/

/**
 * Send a binary frame: [LEN_HI][LEN_LO][payload...]
 */
static inline void fnet_send_frame(const net_transport_t* transport,
                                    const uint8_t* payload, int payload_len)
{
    uint8_t hdr[2];
    hdr[0] = (uint8_t)((payload_len >> 8) & 0xFF);
    hdr[1] = (uint8_t)(payload_len & 0xFF);
    net_transport_send(transport, hdr, 2);
    net_transport_send(transport, payload, payload_len);
}

/**
 * Receive state machine (identical to SNCP/Coup/Disasteroids).
 */
typedef struct {
    uint8_t* buf;
    int      buf_size;
    int      rx_pos;
    int      frame_len;
} fnet_rx_state_t;

static inline void fnet_rx_init(fnet_rx_state_t* st, uint8_t* buf, int buf_size)
{
    st->buf = buf;
    st->buf_size = buf_size;
    st->rx_pos = 0;
    st->frame_len = -1;
}

/* Max UART bytes to process per poll call. */
#define FNET_RX_MAX_PER_POLL  48

/**
 * Poll for a complete frame. Returns:
 *   >0 = frame length (payload in st->buf[0..len-1])
 *    0 = incomplete (call again next frame)
 *   -1 = error (frame too large or zero-length)
 */
static inline int fnet_rx_poll(fnet_rx_state_t* st,
                                const net_transport_t* transport)
{
    int bytes_read = 0;
    while (bytes_read < FNET_RX_MAX_PER_POLL && net_transport_rx_ready(transport)) {
        uint8_t b = net_transport_rx_byte(transport);
        bytes_read++;

        if (st->frame_len < 0) {
            st->buf[st->rx_pos++] = b;
            if (st->rx_pos == 2) {
                st->frame_len = ((int)st->buf[0] << 8) | (int)st->buf[1];
                st->rx_pos = 0;
                if (st->frame_len > st->buf_size || st->frame_len == 0) {
                    st->frame_len = -1;
                    st->rx_pos = 0;
                    return -1;
                }
            }
        } else {
            st->buf[st->rx_pos++] = b;
            if (st->rx_pos >= st->frame_len) {
                int len = st->frame_len;
                st->frame_len = -1;
                st->rx_pos = 0;
                return len;
            }
        }
    }
    return 0;
}

/*============================================================================
 * Decode Helpers
 *============================================================================*/

static inline int fnet_read_string(const uint8_t* p, int remaining,
                                    char* dst, int max)
{
    int slen, copy, i;
    if (remaining < 1) { dst[0] = '\0'; return -1; }
    slen = (int)p[0];
    if (remaining < 1 + slen) { dst[0] = '\0'; return -1; }
    copy = (slen < max - 1) ? slen : (max - 1);
    for (i = 0; i < copy; i++) dst[i] = (char)p[1 + i];
    dst[copy] = '\0';
    return 1 + slen;
}

/*============================================================================
 * Client -> Server Encode Functions
 *============================================================================*/

/** Encode CONNECT (new user, no UUID). */
static inline int fnet_encode_connect(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = SNCP_MSG_CONNECT;
    return 3;
}

/** Encode CONNECT with UUID for reconnection. */
static inline int fnet_encode_connect_uuid(uint8_t* buf, const char* uuid)
{
    int i;
    buf[0] = 0x00;
    buf[1] = 37;
    buf[2] = SNCP_MSG_CONNECT;
    for (i = 0; i < SNCP_UUID_LEN; i++)
        buf[3 + i] = (uint8_t)uuid[i];
    return 3 + SNCP_UUID_LEN;
}

/** Encode SET_USERNAME. */
static inline int fnet_encode_set_username(uint8_t* buf, const char* name)
{
    int nlen = 0;
    int payload_len;
    int i;
    while (name[nlen]) nlen++;
    if (nlen > 16) nlen = 16;
    payload_len = 1 + 1 + nlen;
    buf[0] = (uint8_t)((payload_len >> 8) & 0xFF);
    buf[1] = (uint8_t)(payload_len & 0xFF);
    buf[2] = SNCP_MSG_SET_USERNAME;
    buf[3] = (uint8_t)nlen;
    for (i = 0; i < nlen; i++)
        buf[4 + i] = (uint8_t)name[i];
    return 2 + payload_len;
}

/** Encode DISCONNECT. */
static inline int fnet_encode_disconnect(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = SNCP_MSG_DISCONNECT;
    return 3;
}

/** Encode HEARTBEAT. */
static inline int fnet_encode_heartbeat(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = SNCP_MSG_HEARTBEAT;
    return 3;
}

/** Encode READY toggle. */
static inline int fnet_encode_ready(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_READY;
    return 3;
}

/** Encode START_GAME request. */
static inline int fnet_encode_start_game(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_START_GAME_REQ;
    return 3;
}

/**
 * Encode INPUT_STATE: per-frame input for the local player.
 * [frame_hi:1][frame_lo:1][input:1]  (only 1 byte input for Flicky's Flock)
 */
static inline int fnet_encode_input_state(uint8_t* buf,
                                           uint16_t frame_num,
                                           uint8_t input_bits)
{
    buf[0] = 0x00;
    buf[1] = 0x04;  /* payload = type(1) + frame(2) + input(1) */
    buf[2] = FNET_MSG_INPUT_STATE;
    buf[3] = (uint8_t)((frame_num >> 8) & 0xFF);
    buf[4] = (uint8_t)(frame_num & 0xFF);
    buf[5] = input_bits;
    return 6;
}

/** Encode PAUSE request. */
static inline int fnet_encode_pause(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_PAUSE_REQ;
    return 3;
}

/**
 * Encode PLAYER_STATE: local player position for server sync.
 * [y:2s][y_speed:2s][state:1][sprite:1] = 7 bytes payload
 */
static inline int fnet_encode_player_state(uint8_t* buf,
                                            int16_t y, int16_t y_speed,
                                            uint8_t state, uint8_t sprite)
{
    buf[0] = 0x00;
    buf[1] = 0x07;  /* payload = type(1) + y(2) + y_speed(2) + state(1) + sprite(1) */
    buf[2] = FNET_MSG_PLAYER_STATE;
    buf[3] = (uint8_t)((y >> 8) & 0xFF);
    buf[4] = (uint8_t)(y & 0xFF);
    buf[5] = (uint8_t)((y_speed >> 8) & 0xFF);
    buf[6] = (uint8_t)(y_speed & 0xFF);
    buf[7] = state;
    buf[8] = sprite;
    return 9;  /* 2 header + 7 payload */
}

/** Encode SPRITE_SELECT: request bird color change. */
static inline int fnet_encode_sprite_select(uint8_t* buf, uint8_t sprite_id)
{
    buf[0] = 0x00;
    buf[1] = 0x02;  /* payload = type(1) + sprite_id(1) */
    buf[2] = FNET_MSG_SPRITE_SELECT;
    buf[3] = sprite_id;
    return 4;
}

/** Encode BOT_ADD: request server to add one bot. */
static inline int fnet_encode_bot_add(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_BOT_ADD;
    return 3;
}

/** Encode BOT_REMOVE: request server to remove one bot. */
static inline int fnet_encode_bot_remove(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_BOT_REMOVE;
    return 3;
}

/** Encode LEADERBOARD_REQ: request server send leaderboard data. */
static inline int fnet_encode_leaderboard_req(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_LEADERBOARD_REQ;
    return 3;
}

/** Encode ADD_LOCAL_PLAYER: register P2 co-op player. */
static inline int fnet_encode_add_local_player(uint8_t* buf, const char* name)
{
    int nlen = 0;
    int payload_len;
    int i;
    while (name[nlen]) nlen++;
    if (nlen > 16) nlen = 16;
    payload_len = 1 + 1 + nlen;  /* type + name_len + name */
    buf[0] = (uint8_t)((payload_len >> 8) & 0xFF);
    buf[1] = (uint8_t)(payload_len & 0xFF);
    buf[2] = FNET_MSG_ADD_LOCAL_PLAYER;
    buf[3] = (uint8_t)nlen;
    for (i = 0; i < nlen; i++)
        buf[4 + i] = (uint8_t)name[i];
    return 2 + payload_len;
}

/** Encode REMOVE_LOCAL_PLAYER: deregister P2 co-op player. */
static inline int fnet_encode_remove_local_player(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_REMOVE_LOCAL_PLAYER;
    return 3;
}

/** Encode INPUT_STATE_P2: per-frame input for P2 co-op player. */
static inline int fnet_encode_input_state_p2(uint8_t* buf,
                                              uint8_t player_id,
                                              uint16_t frame_num,
                                              uint8_t input_bits)
{
    buf[0] = 0x00;
    buf[1] = 0x05;  /* payload = type(1) + pid(1) + frame(2) + input(1) */
    buf[2] = FNET_MSG_INPUT_STATE_P2;
    buf[3] = player_id;
    buf[4] = (uint8_t)((frame_num >> 8) & 0xFF);
    buf[5] = (uint8_t)(frame_num & 0xFF);
    buf[6] = input_bits;
    return 7;
}

/** Encode CLIENT_DEATH: report local player's own death to server. */
static inline int fnet_encode_client_death(uint8_t* buf)
{
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = FNET_MSG_CLIENT_DEATH;
    return 3;
}

/** Encode CLIENT_DEATH_P2: report P2 co-op player's death to server. */
static inline int fnet_encode_client_death_p2(uint8_t* buf, uint8_t player_id)
{
    buf[0] = 0x00;
    buf[1] = 0x02;  /* payload = type(1) + player_id(1) */
    buf[2] = FNET_MSG_CLIENT_DEATH_P2;
    buf[3] = player_id;
    return 4;
}

#endif /* FLOCK_PROTOCOL_H */
