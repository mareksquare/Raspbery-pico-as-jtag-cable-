#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// --- KONFIGURACJA OGÓLNA ---
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

// --- KONFIGURACJA KLAS (Composite: HID + CDC) ---
#define CFG_TUD_CDC                 1
#define CFG_TUD_HID                 1
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// --- CDC ---
#define CFG_TUD_CDC_RX_BUFSIZE      64
#define CFG_TUD_CDC_TX_BUFSIZE      64
#define CFG_TUD_CDC_EP_BUFSIZE      64

// --- HID (CMSIS-DAP v1) ---
#define CFG_TUD_HID_EP_BUFSIZE      64
#define CFG_TUD_HID_POLL_INTERVAL   1
#define CFG_TUD_HID_BOOT_PROTOCOL   0

#ifdef __cplusplus
}
#endif

#endif
