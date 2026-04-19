/**
 * connecting.c - Connection Screen for Online Play
 *
 * Manages the modem connection flow: probing, initializing, dialing.
 * Uses a frame-by-frame state machine so status messages render
 * between blocking modem calls.
 *
 * Adapted from Disasteroids connecting.c for Flicky's Flock.
 */

#include <jo/jo.h>
#include "main.h"
#include "connecting.h"
#include "net/flock_net.h"
#include "net/saturn_uart16550.h"
#include "net/modem.h"

/* slSynch() forces a frame render before blocking modem calls */
extern void slSynch(void);

/*============================================================================
 * Configuration
 *============================================================================*/

#define CONNECT_DIAL_NUMBER   "199404"
#define CONNECT_DIAL_TIMEOUT  180000000  /* ~60 seconds at 28.6MHz */

/*============================================================================
 * Saturn UART + Transport (defined in main.c)
 *============================================================================*/

extern saturn_uart16550_t g_uart;
extern bool g_modem_detected;
extern net_transport_t g_saturn_transport;

/*============================================================================
 * Connection State Machine
 *============================================================================*/

typedef enum {
    CONNECT_STAGE_INIT = 0,
    CONNECT_STAGE_SHOW_PROBE,
    CONNECT_STAGE_PROBING,
    CONNECT_STAGE_SHOW_INIT,
    CONNECT_STAGE_MODEM_INIT,
    CONNECT_STAGE_SHOW_DIAL,
    CONNECT_STAGE_DIALING,
    CONNECT_STAGE_CONNECTED,
    CONNECT_STAGE_FAILED,
} connect_stage_t;

static connect_stage_t g_connect_stage;
static const char* g_connect_msg = "";
static int g_connect_timer = 0;

/*============================================================================
 * Callbacks
 *============================================================================*/

void connecting_init(void)
{
    g_connect_stage = CONNECT_STAGE_INIT;
    g_connect_msg = "PREPARING...";
    g_connect_timer = 0;

    /* Stop title music during connection */
    jo_audio_stop_cd();

    /* Clear VDP2 text plane */
    jo_clear_screen();

    fnet_init();
    fnet_set_modem_available(g_modem_detected);
    fnet_set_username(g_Game.playerName[0] ? g_Game.playerName : "PLAYER");
}

void connecting_input(void)
{
    if (g_Game.gameState != GAMESTATE_CONNECTING) return;

    /* B button to cancel and return to title */
    if (jo_is_pad1_key_pressed(JO_KEY_B)) {
        if (g_Game.input.pressedLT == false) {
            fnet_send_disconnect();
            jo_clear_screen();
            g_Game.input.pressedABC = true; /* block title screen from re-processing B */
            g_Game.titleScreenChoice = 2;   /* reset cursor away from ONLINE */
            g_Game.gameState = GAMESTATE_TITLE_SCREEN;
        }
        g_Game.input.pressedLT = true;
    } else {
        g_Game.input.pressedLT = false;
    }
}

void connecting_update(void)
{
    modem_result_t result;

    if (g_Game.gameState != GAMESTATE_CONNECTING) return;

    switch (g_connect_stage) {

    case CONNECT_STAGE_INIT:
        if (!g_modem_detected) {
            g_connect_msg = "NO NETLINK MODEM";
            fnet_log("No NetLink modem detected");
            g_connect_stage = CONNECT_STAGE_FAILED;
            return;
        }
        g_connect_stage = CONNECT_STAGE_SHOW_PROBE;
        break;

    case CONNECT_STAGE_SHOW_PROBE:
        g_connect_msg = "PROBING MODEM...";
        fnet_log("Probing modem...");
        g_connect_stage = CONNECT_STAGE_PROBING;
        break;

    case CONNECT_STAGE_PROBING:
        slSynch();

        if (modem_probe(&g_uart) != MODEM_OK) {
            g_connect_msg = "NO MODEM RESPONSE";
            fnet_log("No modem response");
            g_connect_stage = CONNECT_STAGE_FAILED;
            return;
        }
        fnet_log("Modem detected");
        g_connect_stage = CONNECT_STAGE_SHOW_INIT;
        break;

    case CONNECT_STAGE_SHOW_INIT:
        g_connect_msg = "INITIALIZING MODEM...";
        fnet_log("Initializing modem...");
        g_connect_stage = CONNECT_STAGE_MODEM_INIT;
        break;

    case CONNECT_STAGE_MODEM_INIT:
        slSynch();

        if (modem_init(&g_uart) != MODEM_OK) {
            g_connect_msg = "MODEM INIT FAILED";
            fnet_log("Modem init failed");
            g_connect_stage = CONNECT_STAGE_FAILED;
            return;
        }
        fnet_log("Modem ready");
        g_connect_stage = CONNECT_STAGE_SHOW_DIAL;
        break;

    case CONNECT_STAGE_SHOW_DIAL:
        g_connect_msg = "DIALING SERVER...";
        fnet_log("Dialing " CONNECT_DIAL_NUMBER "...");
        g_connect_stage = CONNECT_STAGE_DIALING;
        break;

    case CONNECT_STAGE_DIALING:
        slSynch();

        result = modem_dial(&g_uart, CONNECT_DIAL_NUMBER, CONNECT_DIAL_TIMEOUT);
        switch (result) {
        case MODEM_CONNECT:
            g_connect_msg = "CONNECTED!";
            fnet_log("Connection established!");
            modem_flush_input(&g_uart);
            g_connect_stage = CONNECT_STAGE_CONNECTED;
            break;
        case MODEM_NO_CARRIER:
            g_connect_msg = "NO CARRIER";
            fnet_log("NO CARRIER - Check cable");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        case MODEM_BUSY:
            g_connect_msg = "LINE BUSY";
            fnet_log("LINE BUSY - Try again");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        case MODEM_NO_DIALTONE:
            g_connect_msg = "NO DIALTONE";
            fnet_log("NO DIALTONE - Check line");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        case MODEM_NO_ANSWER:
            g_connect_msg = "NO ANSWER";
            fnet_log("NO ANSWER - Server down?");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        case MODEM_TIMEOUT_ERR:
            g_connect_msg = "TIMEOUT";
            fnet_log("TIMEOUT - Server offline?");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        default:
            g_connect_msg = "UNKNOWN ERROR";
            fnet_log("Dial failed");
            g_connect_stage = CONNECT_STAGE_FAILED;
            break;
        }
        break;

    case CONNECT_STAGE_CONNECTED:
        /* Reset RX FIFO after connect */
        saturn_uart_reg_write(&g_uart, SATURN_UART_FCR,
            SATURN_UART_FCR_ENABLE | SATURN_UART_FCR_RXRESET);
        fnet_set_transport(&g_saturn_transport);
        fnet_on_connected();
        jo_clear_screen();
        g_Game.gameState = GAMESTATE_LOBBY;
        lobby_init();
        break;

    case CONNECT_STAGE_FAILED:
        g_connect_timer++;
        if (g_connect_timer > 180) { /* 3 seconds */
            jo_clear_screen();
            g_Game.gameState = GAMESTATE_TITLE_SCREEN;
        }
        break;
    }
}

void connecting_draw(void)
{
    const fnet_state_data_t* nd;
    int i;

    if (g_Game.gameState != GAMESTATE_CONNECTING) return;

    /* Title */
    font_draw_centered("CONNECTING", FONT_Y(8), 500);

    /* Status message */
    font_draw_centered(g_connect_msg, FONT_Y(14), 500);

    /* Log lines */
    nd = fnet_get_data();
    for (i = 0; i < 4; i++) {
        if (i < nd->log_count) {
            font_draw(nd->log_lines[i], FONT_X(3), FONT_Y(17 + i), 500);
        }
    }

    /* Cancel hint */
    font_draw_centered("PRESS B TO CANCEL", FONT_Y(26), 500);

    /* VDP2 fallback: also show status via jo_printf in case font isn't loaded */
    jo_printf(14, 8, "CONNECTING");
    jo_printf(12, 14, "%s", g_connect_msg);
    jo_printf(10, 26, "PRESS B TO CANCEL");
}
