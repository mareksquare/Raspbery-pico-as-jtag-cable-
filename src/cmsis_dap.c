/*
 * Marek's Complete Composite CMSIS-DAP
 * Obsługuje: JTAG (GP2, GP4, GP5, GP7) + UART Debugging
 */

#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>

// --- KONFIGURACJA PINÓW ---
#define PIN_TCK 2
#define PIN_TMS 4
#define PIN_TDI 5
#define PIN_TDO 7
#define PIN_LED 25

// --- DEFINICJE CMSIS-DAP ---
#define ID_DAP_Info              0x00
#define ID_DAP_Connect           0x02
#define ID_DAP_Disconnect        0x03
#define ID_DAP_TransferConfigure 0x04
#define ID_DAP_Transfer          0x05
#define ID_DAP_TransferBlock     0x06
#define ID_DAP_TransferAbort     0x07
#define ID_DAP_WriteABORT        0x08
#define ID_DAP_Delay             0x09
#define ID_DAP_ResetTarget       0x0A
#define ID_DAP_SWJ_Pins          0x10
#define ID_DAP_SWJ_Clock         0x11
#define ID_DAP_SWJ_Sequence      0x12
#define ID_DAP_SWD_Configure     0x13
#define ID_DAP_JTAG_Sequence     0x14
#define ID_DAP_HostStatus        0x01

#define DAP_OK    0x00
#define DAP_ERROR 0xFF

// INFO ID
#define DAP_ID_VENDOR           0x01
#define DAP_ID_PRODUCT          0x02
#define DAP_ID_SER_NUM          0x03
#define DAP_ID_FW_VER           0x04
#define DAP_ID_CAPABILITIES     0xF0
#define DAP_ID_PACKET_COUNT     0xFE
#define DAP_ID_PACKET_SIZE      0xFF

static bool connected = false;

// --- FUNKCJE GPIO ---
void dap_gpio_init() {
    static bool initialized = false;
    if (initialized) return;

    gpio_init(PIN_TCK); gpio_set_dir(PIN_TCK, GPIO_OUT); gpio_put(PIN_TCK, 1);
    gpio_init(PIN_TMS); gpio_set_dir(PIN_TMS, GPIO_OUT); gpio_put(PIN_TMS, 1);
    gpio_init(PIN_TDI); gpio_set_dir(PIN_TDI, GPIO_OUT); gpio_put(PIN_TDI, 1);
    
    // TDO Input + PullUp
    gpio_init(PIN_TDO); gpio_set_dir(PIN_TDO, GPIO_IN);  gpio_pull_up(PIN_TDO);
    
    gpio_init(PIN_LED); gpio_set_dir(PIN_LED, GPIO_OUT);
    initialized = true;
}

// --- OBSŁUGA KOMEND ---

static void handle_dap_info(uint8_t const *req, uint8_t *resp) {
    uint8_t info_id = req[1];
    resp[0] = ID_DAP_Info;
    
    uint8_t len = 0;
    const char *str_val = NULL;

    switch(info_id) {
        case DAP_ID_VENDOR:  str_val = "Marek Factory"; break;
        case DAP_ID_PRODUCT: str_val = "Pico JTAG"; break;
        case DAP_ID_SER_NUM: str_val = "12345678"; break;
        case DAP_ID_FW_VER:  str_val = "1.0.0"; break;
        
        case DAP_ID_CAPABILITIES:
            len = 1; resp[1] = len; resp[2] = 0x02; // 0x02 = JTAG Only
            break;
        case DAP_ID_PACKET_COUNT:
            len = 1; resp[1] = len; resp[2] = 1;
            break;
        case DAP_ID_PACKET_SIZE:
            // KLUCZOWE: OpenOCD wymaga rozmiaru pakietu (64 bajty)
            len = 2; resp[1] = len; resp[2] = 64; resp[3] = 0;
            break;
        default:
            resp[1] = 0; break;
    }

    if (str_val) {
        len = (uint8_t)strlen(str_val);
        resp[1] = len;
        memcpy(&resp[2], str_val, len);
    }
}

static void handle_dap_connect(uint8_t const *req, uint8_t *resp) {
    dap_gpio_init();
    resp[0] = ID_DAP_Connect;
    if (req[1] == 1) { // 1 = SWD
        resp[1] = DAP_ERROR; 
    } else {
        resp[1] = 2; // 2 = JTAG
        connected = true;
        gpio_put(PIN_LED, 1);
    }
}

static void handle_dap_disconnect(uint8_t const *req, uint8_t *resp) {
    resp[0] = ID_DAP_Disconnect;
    connected = false;
    resp[1] = DAP_OK;
    gpio_put(PIN_LED, 0);
}

static void handle_dap_swj_pins(uint8_t const *req, uint8_t *resp) {
    uint8_t output = req[1];
    uint8_t select = req[2];
    
    if (select & (1<<0)) gpio_put(PIN_TCK, (output & (1<<0)) ? 1 : 0);
    if (select & (1<<1)) gpio_put(PIN_TMS, (output & (1<<1)) ? 1 : 0);
    if (select & (1<<2)) gpio_put(PIN_TDI, (output & (1<<2)) ? 1 : 0);
    
    uint8_t inputs = 0;
    if (gpio_get(PIN_TCK)) inputs |= (1<<0);
    if (gpio_get(PIN_TMS)) inputs |= (1<<1);
    if (gpio_get(PIN_TDI)) inputs |= (1<<2);
    if (gpio_get(PIN_TDO)) inputs |= (1<<3);
    
    resp[0] = ID_DAP_SWJ_Pins;
    resp[1] = inputs;
}

static void handle_dap_jtag_sequence(uint8_t const *req, uint8_t *resp) {
    resp[0] = ID_DAP_JTAG_Sequence;
    resp[1] = DAP_OK;
    uint8_t seq_count = req[1];
    uint32_t req_idx = 2;
    uint32_t resp_idx = 2;

    for (int i = 0; i < seq_count; i++) {
        uint8_t info = req[req_idx++];
        uint8_t len = info & 0x3F;
        if (len == 0) len = 64;
        bool tdo_capture = (info & 0x40);
        int byte_count = (len + 7) / 8;
        uint8_t tdo_byte = 0;
        int bit_in_byte = 0;
        
        for (int b = 0; b < byte_count; b++) {
             uint8_t tdi_byte = req[req_idx++];
             for (int bit = 0; bit < 8; bit++) {
                 int total_bit = b*8 + bit;
                 if (total_bit >= len) break;
                 uint8_t tdi_val = (tdi_byte >> bit) & 1;
                 
                 gpio_put(PIN_TDI, tdi_val);
                 gpio_put(PIN_TCK, 0);
                 asm volatile("nop");
                 gpio_put(PIN_TCK, 1); // Sample TDO
                 asm volatile("nop");

                 if (tdo_capture && gpio_get(PIN_TDO)) {
                     tdo_byte |= (1 << bit_in_byte);
                 }
                 gpio_put(PIN_TCK, 0);
                 
                 if (tdo_capture) {
                    bit_in_byte++;
                    if (bit_in_byte == 8) {
                        resp[resp_idx++] = tdo_byte;
                        tdo_byte = 0;
                        bit_in_byte = 0;
                    }
                 }
             }
        }
        if (tdo_capture && bit_in_byte > 0) resp[resp_idx++] = tdo_byte;
    }
}

// GŁÓWNY ROUTER
void cmsis_dap_handle_command(uint8_t const *req_original, uint8_t *resp, uint16_t bufsize) {
    uint8_t req[64];
    uint16_t copy_len = (bufsize > 64) ? 64 : bufsize;
    memcpy(req, req_original, copy_len);
    if (copy_len < 64) memset(req + copy_len, 0, 64 - copy_len);
    memset(resp, 0, 64);
    
    uint8_t cmd = req[0];
    switch(cmd) {
        case ID_DAP_Info:           handle_dap_info(req, resp); break;
        case ID_DAP_Connect:        handle_dap_connect(req, resp); break;
        case ID_DAP_Disconnect:     handle_dap_disconnect(req, resp); break;
        case ID_DAP_SWJ_Pins:       handle_dap_swj_pins(req, resp); break;
        case ID_DAP_JTAG_Sequence:  handle_dap_jtag_sequence(req, resp); break;
        case ID_DAP_SWJ_Clock:      
        case ID_DAP_HostStatus:     
        case ID_DAP_TransferConfigure: 
            resp[0] = cmd; resp[1] = DAP_OK; break;
        default:
            resp[0] = cmd; resp[1] = DAP_ERROR; break;
    }
}