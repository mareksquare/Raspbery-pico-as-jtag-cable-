#include "tusb.h"
#include "pico/stdlib.h"
#include <string.h>
#include "pico/multicore.h"


void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize)
{
    uint8_t response[64];

    cmsis_dap_handle_command(buffer, response, bufsize);

    while (!tud_hid_ready()) tud_task();
    tud_hid_report(0, response, 64);
}

void core1_entry() {
    stdio_init_all();
    tusb_init();
    while (1) {
        tud_task();   // obsługa USB
        // kod dla rdzenia 1
    }
}

int main() {

    multicore_launch_core1(core1_entry);


    while (1) {
        //tud_task();   // obsługa USB
        // kod dla rdzenia 0
    }
}

