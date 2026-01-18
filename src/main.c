#include "tusb.h"
#include "pico/stdlib.h"
#include <string.h>
#include "pico/multicore.h"
#include "cmsis_dap.h"

//extern uint8_t tdo_stream_buf[];
//extern volatile uint32_t tdo_wp;
//extern volatile uint32_t tdo_rp;


void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize)
{
    uint8_t response[64];

    cmsis_dap_handle_command(buffer, response, bufsize);

    while (!tud_hid_ready()) tud_task();
    tud_hid_report(0, response, 64);
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if (dtr) {
        tud_cdc_write_str("CDC ready\n");
        tud_cdc_write_flush();
    }
}

void core1_entry() {         // kod dla rdzenia 1

    while (1) {
    }

}

void process_cdc_command(const char *cmd)
{
    if (strcmp(cmd, "ping") == 0) {
        tud_cdc_write_str("pong\n");
    }
    else if (strcmp(cmd, "jtag on") == 0) {
        tud_cdc_write_str("JTAG enabled\n");
    }
    else {
        tud_cdc_write_str("Unknown command\n");
    }
    tud_cdc_write_flush();
}

static char linebuf[128];
static int idx = 0;

void tud_cdc_rx_cb(uint8_t itf)
{
    while (tud_cdc_available()) {
        char c = tud_cdc_read_char();

        if (c == '\n' || c == '\r') {
            linebuf[idx] = 0;
            process_cdc_command(linebuf);
            idx = 0;
        } else if (idx < sizeof(linebuf)-1) {
            linebuf[idx++] = c;
        }
    }
}

int main() {        // kod dla rdzenia 0

    multicore_launch_core1(core1_entry);
    stdio_init_all();
    tusb_init();

    while (1) {
        tud_task();   // obsługa USB

	// --- RING BUFFER CONSUMER: wysyłanie TDO przez CDC ---
	while (tdo_rp != tdo_wp && tud_cdc_write_available()) {
	    tud_cdc_write_char(tdo_stream_buf[tdo_rp]);
	    tdo_rp = (tdo_rp + 1) % TDO_BUF_SIZE;
	}

	tud_cdc_write_flush();

    }
}


