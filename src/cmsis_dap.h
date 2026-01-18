#ifndef CMSIS_DAP_H
#define CMSIS_DAP_H

#include <stdint.h>
// --- KONFIGURACJA PINÓW ---
#define PIN_TCK 2
#define PIN_TMS 4
#define PIN_TDI 3
#define PIN_TDO 5
#define DAP_JTAG_nRESET_PIN 6
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

#define DAP_RESET_TARGET 1

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
#define TDO_BUF_SIZE 16384   // 16 KB – bezpieczne dla długich sekwencji


extern uint8_t tdo_stream_buf[];
extern volatile uint32_t tdo_wp;
extern volatile uint32_t tdo_rp;


void cmsis_dap_handle_command(uint8_t const *req, uint8_t *resp, uint16_t bufsize);

#endif
