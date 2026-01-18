/*
 * Marek's Complete Composite CMSIS-DAP
 * Obsługuje: JTAG (GP2, GP4, GP5, GP7) + UART Debugging
 */
#define JTAG_DELAY_US  5   // lub 10, 20, 50 – jak w Arduino

#include "cmsis_dap.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>


uint8_t tdo_stream_buf[TDO_BUF_SIZE];

volatile uint32_t tdo_wp = 0;   // write pointer
volatile uint32_t tdo_rp = 0;   // read pointer




static bool connected = false;

// --- FUNKCJE GPIO ---
void dap_gpio_init() {
    int i;
    static bool initialized = false;
    if (initialized) return;
    for ( i = 20;i >0 ; i-- ){
	gpio_init(i);
	gpio_set_dir(i, GPIO_OUT);
	gpio_put(i, 0);
	
    }
    sleep_us(1);
    for ( i = 20;i >0 ; i-- ){
	gpio_init(i);
	gpio_set_dir(i, GPIO_OUT);
	gpio_put(i, 1);
	
    }
    sleep_us(1);

    gpio_init(PIN_TCK); gpio_set_dir(PIN_TCK, GPIO_OUT); gpio_put(PIN_TCK, 1);
    gpio_init(PIN_TMS); gpio_set_dir(PIN_TMS, GPIO_OUT); gpio_put(PIN_TMS, 1);
    gpio_init(PIN_TDI); gpio_set_dir(PIN_TDI, GPIO_OUT); gpio_put(PIN_TDI, 1);
    
    // TDO Input + PullUp
    gpio_init(PIN_TDO); gpio_set_dir(PIN_TDO, GPIO_IN);//  gpio_pull_up(PIN_TDO);
    
    gpio_init(PIN_LED); gpio_set_dir(PIN_LED, GPIO_OUT);
    initialized = true;
}

// --- OBSŁUGA KOMEND ---

static void handle_dap_info(uint8_t const *req, uint8_t *resp) {
    uint8_t info_id = req[1];
    resp[0] = ID_DAP_Info;
    const uint16_t MAX_REQ  = 64;   // maksymalna długość request (bezpieczna)
    const uint16_t MAX_RESP = 64;   // maksymalna długość response (bezpieczna)

// sanity check dla seq_count
    //uint8_t seq_count = req[1];
    //if (seq_count == 0 || seq_count > 8) { resp[0] = ID_DAP_JTAG_Sequence; resp[1] = DAP_ERROR; return; }

    //uint32_t req_idx  = 2;
    //uint32_t resp_idx = 2;

    
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
    sleep_ms(1);
    if (select & (1<<0)) gpio_put(PIN_TCK, (output & (1<<0)) ? 1 : 0);
    if (select & (1<<1)) gpio_put(PIN_TMS, (output & (1<<1)) ? 1 : 0);
    if (select & (1<<2)) gpio_put(PIN_TDI, (output & (1<<2)) ? 1 : 0);
    sleep_ms(1);
    
    uint8_t inputs = 0;
    if (gpio_get(PIN_TCK)) inputs |= (1<<0);
    if (gpio_get(PIN_TMS)) inputs |= (1<<1);
    if (gpio_get(PIN_TDI)) inputs |= (1<<2);
    if (gpio_get(PIN_TDO)) inputs |= (1<<3);
    
    resp[0] = ID_DAP_SWJ_Pins;
    resp[1] = inputs;
}


static void handle_dap_jtag_sequence(uint8_t const *req, uint8_t *resp)
{
    resp[0] = ID_DAP_JTAG_Sequence;
    resp[1] = DAP_OK;

    uint8_t seq_count = req[1];
    uint32_t req_idx  = 2;
    uint32_t resp_idx = 2;

    for (int s = 0; s < seq_count; s++)
    {
        uint8_t info    = req[req_idx++];
        uint8_t bit_len = info & 0x3F;
        if (bit_len == 0) bit_len = 64;

        bool tms_val     = (info & 0x40) != 0;
        bool capture_tdo = (info & 0x80) != 0;

        int byte_count = (bit_len + 7) / 8;

        uint8_t tdo_byte = 0;
        uint8_t bit_pos  = 0;
        uint8_t bit_tdo  = 0;

        for (int b = 0; b < byte_count; b++)
        {
            uint8_t tdi_byte = req[req_idx++];

            for (int bit = 0; bit < 8; bit++)
            {
                int total_bit = b * 8 + bit;
                if (total_bit >= bit_len) break;

                uint8_t tdi_val = (tdi_byte >> bit) & 1;

                // --- TCK LOW: ustaw TMS/TDI ---
                sleep_us(JTAG_DELAY_US);
                gpio_put(PIN_TCK, 0);
                sleep_us(JTAG_DELAY_US);
                gpio_put(PIN_TMS, tms_val);
                sleep_us(JTAG_DELAY_US);
                gpio_put(PIN_TDI, tdi_val);
                sleep_us(JTAG_DELAY_US);

                // --- TCK HIGH: próbkuj TDO ---
                gpio_put(PIN_TCK, 1);
                sleep_us(JTAG_DELAY_US);

		bit_tdo = gpio_get(PIN_TDO);//pobieramy tdo
		// --- RING BUFFER PRODUCER: zapis TDO bez utraty bitów ---
		uint32_t next_wp = (tdo_wp + 4) % TDO_BUF_SIZE;

		if (next_wp != tdo_rp) {
		    // jest miejsce w buforze
		    tdo_stream_buf[tdo_wp] = bit_tdo + '0';
		    if (tms_val){
			tdo_stream_buf[tdo_wp+1] = 'S';
		    }else{
			tdo_stream_buf[tdo_wp+1] = 's';
		    }

		    if (tdi_val){
			tdo_stream_buf[tdo_wp+2] = 'I';
		    }else{
			tdo_stream_buf[tdo_wp+2] = 'i';
		    }

		    if (capture_tdo){
			tdo_stream_buf[tdo_wp+3] = 'C';
		    }else{
			tdo_stream_buf[tdo_wp+3] = 'c';
		    }


		    tdo_wp = next_wp;
		} else {
		        tdo_stream_buf[tdo_wp] = 'A';
		    // BUFOR PEŁNY – NIE NADPISUJEMY!
		    // Możesz tu ustawić flagę overflow, jeśli chcesz
		}
                sleep_us(JTAG_DELAY_US);

                if (capture_tdo)
                {
                    if (bit_tdo)
                        tdo_byte |= (1u << bit_pos);

                    bit_pos++;

                    if (bit_pos == 8)
                    {
                        resp[resp_idx++] = tdo_byte;
                        tdo_byte = 0;
                        bit_pos  = 0;
                    }
                }

                // --- TCK LOW: koniec cyklu ---
                gpio_put(PIN_TCK, 0);
                sleep_us(JTAG_DELAY_US);
            }
        }

        // jeśli zostały niepełne bity
        if (capture_tdo && bit_pos > 0)
            resp[resp_idx++] = tdo_byte;
    }
}

/*
static void handle_dap_jtag_sequence(uint8_t const *req, uint8_t *resp)
{
    resp[0] = ID_DAP_JTAG_Sequence;
    resp[1] = DAP_OK;

    uint8_t seq_count = req[1];
    uint32_t req_idx = 2;
    uint32_t resp_idx = 2;

    for (int s = 0; s < seq_count; s++)
    {
        uint8_t info = req[req_idx++];
        uint8_t bit_len = info & 0x3F;
        if (bit_len == 0) bit_len = 64;
        bool tms_val     = (info & 0x40) != 0;  // bit 6 = TMS
        bool capture_tdo = (info & 0x80) != 0;  // bit 7 = CAPTURE
        int byte_count = (bit_len + 7) / 8;
        uint8_t tdo_byte = 0;
        uint8_t bit_pos  = 0;
        for (int b = 0; b < byte_count; b++)
        {
            uint8_t tdi_byte = req[req_idx++];
            for (int bit = 0; bit < 8; bit++)
            {
                int total_bit = b * 8 + bit;
                if (total_bit >= bit_len) break;
                uint8_t tdi_val = (tdi_byte >> bit) & 1;
                // --- TCK LOW: ustaw TMS/TDI ---
                gpio_put(PIN_TCK, 0);
                gpio_put(PIN_TDI, tdi_val);
                gpio_put(PIN_TMS, tms_val);
                sleep_us(JTAG_DELAY_US);   // setup time
                // --- TCK HIGH: próbkuj TDO ---
                gpio_put(PIN_TCK, 1);
                sleep_us(JTAG_DELAY_US);   // hold time
                if (capture_tdo)
                {
if (gpio_get(PIN_TDO))
    tdo_byte |= (1u << bit_pos);

if (bit_pos == 7) {
    // bajt kompletny
    resp[resp_idx++] = tdo_byte;

    for (int i = 0; i < 8; i++)
        tdo_stream_buf[tdo_wp++] = (tdo_byte & (1 << i)) ? '1' : '0';

    tdo_stream_buf[tdo_wp++] = '|';

    tdo_byte = 0;
    bit_pos = 0;
} else {
    bit_pos++;
}


                }

                // --- TCK LOW: koniec cyklu ---
                gpio_put(PIN_TCK, 0);
                sleep_us(JTAG_DELAY_US);
            }
        }

        if (capture_tdo && bit_pos > 0)
            resp[resp_idx++] = tdo_byte;
	// --- STAN SPOCZYNKU PO SEKWENCJI ---
	gpio_put(PIN_TDI, 0);
	sleep_us(JTAG_DELAY_US);
    }
}
*/


/*
static void handle_dap_jtag_sequence(uint8_t const *req, uint8_t *resp)
{
    resp[0] = ID_DAP_JTAG_Sequence;
    resp[1] = DAP_OK;

    uint8_t seq_count = req[1];
    uint32_t req_idx = 2;
    uint32_t resp_idx = 2;

    for (int s = 0; s < seq_count; s++)
    {
        uint8_t info = req[req_idx++];
        uint8_t bit_len = info & 0x3F;
        if (bit_len == 0) bit_len = 64;

        bool capture_tdo = (info & 0x40) != 0;
        bool tms_val     = (info & 0x80) != 0;   // *** KLUCZOWE ***

        int byte_count = (bit_len + 7) / 8;

        uint8_t tdo_byte = 0;
        uint8_t bit_pos = 0;

        for (int b = 0; b < byte_count; b++)
        {
            uint8_t tdi_byte = req[req_idx++];

            for (int bit = 0; bit < 8; bit++)
            {
                int total_bit = b * 8 + bit;
                if (total_bit >= bit_len) break;

                uint8_t tdi_val = (tdi_byte >> bit) & 1;

                // --- Ustaw TMS i TDI PRZED TCK rising ---
		sleep_us(1);
                gpio_put(PIN_TMS, tms_val);
		sleep_us(1);
                gpio_put(PIN_TDI, tdi_val);

                // --- TCK LOW ---
                gpio_put(PIN_TCK, 0);
                asm volatile("nop");
		sleep_us(1);
                // --- TCK HIGH (sample TDO) ---
                gpio_put(PIN_TCK, 1);
                asm volatile("nop");

                if (capture_tdo)
                {
                    if (gpio_get(PIN_TDO))
                        tdo_byte |= (1u << bit_pos);

                    bit_pos++;

                    if (bit_pos == 8)
                    {
                        resp[resp_idx++] = tdo_byte;
                        tdo_byte = 0;
                        bit_pos = 0;
                    }
                }

                // --- TCK LOW (koniec cyklu) ---
		sleep_us(1);
                gpio_put(PIN_TCK, 0);
            }
        }

        if (capture_tdo && bit_pos > 0)
            resp[resp_idx++] = tdo_byte;
    }
}
*/
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