#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"
#include "beef_config_store.h"
#include "beef_protocol.h"

#define USB_VID_DEFAULT 0xFEED
#define USB_PID_RP2354B 0x0003
#define USB_VID_KONAMI  0x1CCF
#define USB_PID_ENTRY   0x1018
#define USB_PID_PREMIUM 0x8048

#define CFG_CONTROLLER_TYPE 31u

enum {
  CONTROLLER_DEFAULT = 0,
  CONTROLLER_IIDX_ENTRY = 1,
  CONTROLLER_IIDX_PREMIUM = 2
};

static tusb_desc_device_t desc_device = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = USB_VID_DEFAULT,
  .idProduct = USB_PID_RP2354B,
  .bcdDevice = 0x0100,
  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,
  .bNumConfigurations = 0x01
};

static uint8_t controller_type(void) {
  uint8_t type = beef_config_store_data()[CFG_CONTROLLER_TYPE];
  return type <= CONTROLLER_IIDX_PREMIUM ? type : CONTROLLER_DEFAULT;
}

uint8_t const *tud_descriptor_device_cb(void) {
  switch (controller_type()) {
    case CONTROLLER_IIDX_ENTRY:
      desc_device.idVendor = USB_VID_KONAMI;
      desc_device.idProduct = USB_PID_ENTRY;
      break;
    case CONTROLLER_IIDX_PREMIUM:
      desc_device.idVendor = USB_VID_KONAMI;
      desc_device.idProduct = USB_PID_PREMIUM;
      break;
    default:
      desc_device.idVendor = USB_VID_DEFAULT;
      desc_device.idProduct = USB_PID_RP2354B;
      break;
  }
  return (uint8_t const *)&desc_device;
}

/*
 * IIDX joystick report, matching the AT90 firmware layout:
 *   X, Y, 14 buttons (2 padding bits)
 * Output report:
 *   11 button-light bits + 5 padding bits + TT RGB + center-bar RGB = 8 bytes.
 */
static uint8_t const desc_gamepad_report[] = {
  0x05, 0x01,       /* Usage Page (Generic Desktop) */
  0x09, 0x04,       /* Usage (Joystick) */
  0xA1, 0x01,       /* Collection (Application) */

    0x09, 0x01,     /* Usage (Pointer) */
    0xA1, 0x00,     /* Collection (Physical) */
      0x09, 0x30,   /* X */
      0x09, 0x31,   /* Y */
      0x15, 0x00,
      0x26, 0xFF, 0x00,
      0x35, 0xFF,
      0x45, 0x01,
      0x75, 0x08,
      0x95, 0x02,
      0x81, 0x02,
    0xC0,

    0x05, 0x09,     /* Button page */
    0x19, 0x01,
    0x29, 0x0E,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x0E,
    0x81, 0x02,
    0x75, 0x02,
    0x95, 0x01,
    0x81, 0x03,

    /* Host-controlled button lights: 11 bits + 5 padding. */
    0x05, 0x0A,
    0x19, 0x01,
    0x29, 0x0B,
    0x75, 0x01,
    0x95, 0x0B,
    0x91, 0x02,
    0x75, 0x05,
    0x95, 0x01,
    0x91, 0x03,

    /*
     * TT RGB + center RGB: R,G,B,R,G,B.
     *
     * A Variable Output item must have local Usage items for the fields it
     * describes. The previous descriptor emitted six variable bytes without
     * any usages after the button-light Output item. Windows HIDCLASS rejects
     * that interface with CM_PROB_FAILED_START (Code 10).
     *
     * Ordinal usages 12..17 match the six lighting channels and keep the
     * 8-byte OUT report layout unchanged.
     */
    0x05, 0x0A,     /* Usage Page (Ordinal) */
    0x19, 0x0C,     /* Usage Minimum (12) */
    0x29, 0x11,     /* Usage Maximum (17) */
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, 0x06,
    0x91, 0x02,

  0xC0
};

/* AT90-compatible 13-key array on a separate interface. */
static uint8_t const desc_keyboard_report[] = {
  0x05, 0x01,
  0x09, 0x06,
  0xA1, 0x01,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0xFF,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x95, 0x0D,
    0x75, 0x08,
    0x81, 0x00,
  0xC0
};

/* Beef Configurator vendor interface. */
static uint8_t const desc_config_report[] = {
  0x06, 0xEB, 0xFF,
  0x09, 0x01,
  0x15, 0x00,
  0x26, 0xFF, 0x00,
  0xA1, 0x01,
    0x85, 0x01, 0x09, 0x01, 0x75, 0x08, 0x96, 0x8B, 0x00, 0xB1, 0x02,
    0x85, 0x02, 0x09, 0x02, 0x75, 0x08, 0x95, 0x01,       0xB1, 0x02,
    0x85, 0x03, 0x09, 0x03, 0x75, 0x08, 0x95, 0x07,       0xB1, 0x02,
    0x85, 0x04, 0x09, 0x04, 0x75, 0x08, 0x95, 0x3C,       0xB1, 0x02,
    0x85, 0x05, 0x09, 0x05, 0x75, 0x08, 0x95, 0x3C,       0x81, 0x02,
    0x85, 0x06, 0x09, 0x06, 0x75, 0x08, 0x95, 0x0B,       0xB1, 0x02,
    0x85, 0x07, 0x09, 0x07, 0x75, 0x08, 0x95, 0x1C,       0x81, 0x02,
  0xC0
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  switch (instance) {
    case BEEF_HID_INSTANCE_GAMEPAD:
      return desc_gamepad_report;
    case BEEF_HID_INSTANCE_KEYBOARD:
      return desc_keyboard_report;
    case BEEF_HID_INSTANCE_CONFIG:
      return desc_config_report;
    default:
      return NULL;
  }
}

enum {
  ITF_NUM_GAMEPAD,
  ITF_NUM_KEYBOARD,
  ITF_NUM_CONFIG,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + (2 * TUD_HID_DESC_LEN))
#define EPNUM_GAMEPAD_OUT 0x01
#define EPNUM_GAMEPAD_IN  0x81
#define EPNUM_KEYBOARD_IN 0x82
#define EPNUM_CONFIG_IN   0x83

static uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_GAMEPAD, 0, HID_ITF_PROTOCOL_NONE,
                           sizeof(desc_gamepad_report),
                           EPNUM_GAMEPAD_OUT, EPNUM_GAMEPAD_IN, 64, 1),

  TUD_HID_DESCRIPTOR(ITF_NUM_KEYBOARD, 0, HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_keyboard_report), EPNUM_KEYBOARD_IN, 64, 1),

  TUD_HID_DESCRIPTOR(ITF_NUM_CONFIG, 0, HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_config_report), EPNUM_CONFIG_IN, 64, 1)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_configuration;
}

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL
};

static char const *manufacturer_string(void) {
  return controller_type() == CONTROLLER_DEFAULT ? "Beef Board" : "Konami Amusement";
}

static char const *product_string(void) {
  switch (controller_type()) {
    case CONTROLLER_IIDX_ENTRY:
      return "beatmania IIDX controller entry model";
    case CONTROLLER_IIDX_PREMIUM:
      return "beatmania IIDX controller premium model";
    default:
      return "BEEF BOARD RP2354B";
  }
}

static uint16_t desc_str[64];
static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  size_t count;

  if (index == STRID_LANGID) {
    desc_str[1] = 0x0409;
    count = 1;
  } else if (index == STRID_SERIAL) {
    pico_get_unique_board_id_string(serial, sizeof(serial));
    count = strlen(serial);
    if (count > 63) count = 63;
    for (size_t i = 0; i < count; ++i) desc_str[1 + i] = (uint8_t)serial[i];
  } else {
    char const *str = NULL;
    if (index == STRID_MANUFACTURER) str = manufacturer_string();
    if (index == STRID_PRODUCT) str = product_string();
    if (!str) return NULL;

    count = strlen(str);
    if (count > 63) count = 63;
    for (size_t i = 0; i < count; ++i) {
      desc_str[1 + i] = (uint8_t)str[i];
    }
  }

  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
  return desc_str;
}
