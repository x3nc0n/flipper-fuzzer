/*
 * ssid_table.h — word lists and template descriptors for SSID generation.
 *
 * Data only — no code. Included by identifiers.c.
 *
 * Design: identifiers.c picks one of the SSID_TEMPLATES[] by index, then fills
 * the format-tagged slots from the sub-tables below.
 */

#pragma once

#include <stddef.h>

/* ---- First-name pool (used in "<Name>'s <Device>" patterns) ---- */
static const char* const SSID_FIRST_NAMES[] = {
    "James",  "John",   "Robert", "Michael", "David",   "William", "Richard",
    "Joseph", "Thomas", "Daniel", "Matthew", "Anthony", "Mark",    "Donald",
    "Paul",   "Steven", "Andrew", "Kenneth", "Joshua",  "Kevin",
    "Mary",   "Patricia", "Jennifer", "Linda",   "Barbara", "Susan",  "Jessica",
    "Sarah",  "Karen",  "Lisa",   "Nancy",   "Betty",   "Margaret","Sandra",
    "Ashley", "Emily",  "Donna",  "Michelle","Carol",   "Amanda",
};
#define SSID_FIRST_NAME_COUNT (sizeof(SSID_FIRST_NAMES) / sizeof(SSID_FIRST_NAMES[0]))

/* ---- ISP/router brand prefixes ---- */
static const char* const SSID_ISP_PREFIXES[] = {
    "NETGEAR",
    "TP-Link_",
    "ATT",
    "Xfinity",
    "DIRECT-",
    "Spectrum",
    "CenturyLink",
    "Verizon-",
    "COX",
    "ARRIS-",
    "MySpectrumWiFi",
    "HOME-",
    "WLAN-",
    "WiFi-",
};
#define SSID_ISP_PREFIX_COUNT (sizeof(SSID_ISP_PREFIXES) / sizeof(SSID_ISP_PREFIXES[0]))

/* ---- Common static/well-known public SSIDs ---- */
static const char* const SSID_STATIC_NAMES[] = {
    "xfinitywifi",
    "CableWiFi",
    "optimumwifi",
    "attwifi",
    "Boingo Hotspot",
    "eduroam",
    "PASSPOINT",
    "Google Starbucks",
    "Starbucks WiFi",
    "Panera Bread",
    "McDonalds Free WiFi",
    "_The UPS Store",
    "hhonors",
    "Marriott_GUEST",
    "HiltonHHonors",
    "Airport WiFi",
    "Southwest_WiFi",
    "Delta-WiFi",
    "_PnP_",
    "SETUP",
    "linksys",
    "dlink",
    "belkin.setup",
    "default",
    "SINGTEL-XXXX",
    "AndroidShare",
    "iPhone",
};
#define SSID_STATIC_NAME_COUNT (sizeof(SSID_STATIC_NAMES) / sizeof(SSID_STATIC_NAMES[0]))

/* ---- HP printer model suffixes ---- */
static const char* const SSID_HP_MODELS[] = {
    "LaserJet", "OfficeJet", "DeskJet", "Envy", "PhotoSmart",
};
#define SSID_HP_MODEL_COUNT (sizeof(SSID_HP_MODELS) / sizeof(SSID_HP_MODELS[0]))

/* ---- Mobile hotspot device suffix ---- */
static const char* const SSID_HOTSPOT_DEVICES[] = {
    "iPhone",
    "iPhone 13",
    "iPhone 14",
    "iPhone 15",
    "Galaxy S23",
    "Galaxy S24",
    "Pixel 7",
    "Pixel 8",
    "iPad",
    "MacBook",
    "Phone",
};
#define SSID_HOTSPOT_DEVICE_COUNT (sizeof(SSID_HOTSPOT_DEVICES) / sizeof(SSID_HOTSPOT_DEVICES[0]))

/*
 * Template IDs — used internally by chaff_generate_ssid().
 * Each template describes how to compose an SSID string.
 * Keep in sync with the switch in identifiers.c.
 *
 * 0  "<ISP_PREFIX><4 HEX digits>"              e.g. "NETGEAR4F2A"
 * 1  "<ISP_PREFIX><2 decimal digits>"          e.g. "TP-Link_27"
 * 2  "<Name>'s <HotspotDevice>"                e.g. "Sarah's iPhone 14"
 * 3  "<StaticName>"                            e.g. "xfinitywifi"
 * 4  "HP-Print-<2 HEX>-<HPModel>"             e.g. "HP-Print-3F-LaserJet"
 * 5  "DIRECT-<2 HEX>-<HotspotDevice>"         e.g. "DIRECT-A1-Pixel 7"
 * 6  "AndroidAP_<4 decimal digits>"           e.g. "AndroidAP_5821"
 * 7  "Redmi Note <digit> Pro"                  e.g. "Redmi Note 9 Pro"
 * 8  "Setup<4 HEX>"                           e.g. "Setup3B9C"
 * 9  "ASUS_<4 HEX>"                           e.g. "ASUS_9F1C"
 */
#define SSID_TEMPLATE_COUNT 10
