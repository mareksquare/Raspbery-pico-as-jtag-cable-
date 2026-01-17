#include "tusb.h"
#include "pico/unique_id.h"

// --- ID URZĄDZENIA (VID/PID) ---
// Przykładowe ID dla Raspberry Pi (możesz zmienić)
#define USB_VID   0x2E8A
#define USB_PID   0x000C 

// --- DEVICE DESCRIPTOR ---
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Callback wymagany przez TinyUSB - zwraca wskaźnik do deskryptora urządzenia
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

// --- HID REPORT DESCRIPTOR ---
// Prosty deskryptor HID (generyczny) wymagany dla CMSIS-DAP
// --- HID REPORT DESCRIPTOR (CMSIS-DAP v1) ---
uint8_t const desc_hid_report[] = {
    0x06, 0x00, 0xFF,      // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,            // Usage (Vendor 1)
    0xA1, 0x01,            // Collection (Application)

    0x15, 0x00,            // Logical Minimum (0)
    0x26, 0xFF, 0x00,      // Logical Maximum (255)

    0x75, 0x08,            // Report Size (8 bits)
    0x95, 0x40,            // Report Count (64 bytes)
    0x09, 0x01,            // Usage
    0x81, 0x02,            // Input (Data,Var,Abs)

    0x95, 0x40,            // Report Count (64 bytes)
    0x09, 0x01,            // Usage
    0x91, 0x02,            // Output (Data,Var,Abs)

    0xC0                   // End Collection
};

// Callback wymagany przez TinyUSB - zwraca deskryptor raportu HID
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
  (void) itf;
  return desc_hid_report;
}

// Callback wymagany przez TinyUSB - obsługa GET_REPORT (zwykle nieużywane w prostym CMSIS-DAP)
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // Nie zaimplementowano, zwracamy 0
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

// --- CONFIGURATION DESCRIPTOR ---
enum {
  ITF_NUM_HID,
  ITF_NUM_CDC,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};

#define EPNUM_HID       0x01
#define EPNUM_CDC_NOTIF 0x82
#define EPNUM_CDC_OUT   0x03
#define EPNUM_CDC_IN    0x83

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // HID CMSIS-DAP
  TUD_HID_INOUT_DESCRIPTOR(
      ITF_NUM_HID,
      4,                          // string index
      HID_ITF_PROTOCOL_NONE,
      sizeof(desc_hid_report),
      0x80 | EPNUM_HID,           // EP IN
      EPNUM_HID,                  // EP OUT
      64,
      1
  ),

  // CDC (UART bridge)
  TUD_CDC_DESCRIPTOR(
      ITF_NUM_CDC,
      5,                          // string index
      EPNUM_CDC_NOTIF,
      8,
      EPNUM_CDC_OUT,
      EPNUM_CDC_IN,
      64
  )
};
// Callback wymagany przez TinyUSB - zwraca konfigurację
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

// --- STRING DESCRIPTORS ---
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "MarekFactory",                // 1: Manufacturer
  "Pico CMSIS-DAP",              // 2: Product
  "123456",                      // 3: Serials (zastępowane niżej przez Unique ID)
};

static uint16_t _desc_str[32];

// Callback wymagany przez TinyUSB - zwraca ciągi znaków
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;
  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }
  else
  {
    // Obsługa Unique ID dla numeru seryjnego (index 3)
    if (index == 3) {
        char serial[17];
        pico_get_unique_board_id_string(serial, sizeof(serial));
        
        // Konwersja ASCII na UTF-16 LE
        for(uint8_t i=0; i<16; i++) {
          _desc_str[1+i] = serial[i];
        }
        chr_count = 16;
    } else {
        if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++)
        {
          _desc_str[1+i] = str[i];
        }
    }
  }

  // Pierwszy bajt to długość, drugi to typ (STRING)
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}


