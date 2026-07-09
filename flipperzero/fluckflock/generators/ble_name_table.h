/*
 * ble_name_table.h — device-name word lists for BLE advertisement name generation.
 *
 * Data only — no code. Included by identifiers.c.
 *
 * Strategy: pick a base name, optionally append a short numeric/letter suffix
 * for variation, so scanners see a population of plausible-but-distinct names.
 */

#pragma once

#include <stddef.h>

/* ---- Earbuds / headphones ---- */
static const char* const BLE_NAME_EARBUDS[] = {
    "AirPods",
    "AirPods Pro",
    "AirPods Max",
    "Galaxy Buds2",
    "Galaxy Buds Pro",
    "Galaxy Buds Live",
    "Pixel Buds Pro",
    "Pixel Buds A",
    "Jabra Elite 4",
    "Jabra Elite 7 Pro",
    "Jabra Elite 85t",
    "JBL Free II",
    "JBL Tune 230NC",
    "Bose QuietComfort Earbuds",
    "Bose Sport Earbuds",
    "Sony WF-1000XM5",
    "Sony WF-C500",
    "Beats Fit Pro",
    "Beats Studio Buds",
    "Soundcore Liberty 4",
    "Nothing Ear (2)",
    "TOZO T6",
    "Raycon E55",
};
#define BLE_NAME_EARBUDS_COUNT (sizeof(BLE_NAME_EARBUDS) / sizeof(BLE_NAME_EARBUDS[0]))

/* ---- Fitness trackers / smartwatches ---- */
static const char* const BLE_NAME_WEARABLES[] = {
    "Fitbit Charge 6",
    "Fitbit Inspire 3",
    "Fitbit Sense 2",
    "Mi Band 7",
    "Mi Band 8",
    "Mi Smart Band 8",
    "Amazfit GTR 4",
    "Amazfit Bip 5",
    "Galaxy Watch 6",
    "Galaxy Watch 5 Pro",
    "Apple Watch",
    "Garmin Venu 3",
    "Garmin Forerunner 265",
    "Polar H10",
    "Whoop 4.0",
};
#define BLE_NAME_WEARABLES_COUNT (sizeof(BLE_NAME_WEARABLES) / sizeof(BLE_NAME_WEARABLES[0]))

/* ---- Speakers ---- */
static const char* const BLE_NAME_SPEAKERS[] = {
    "JBL Flip 6",
    "JBL Charge 5",
    "JBL GO 3",
    "JBL Xtreme 3",
    "Bose SoundLink",
    "Bose SoundLink Flex",
    "Sony SRS-XB33",
    "Sony SRS-XE300",
    "UE BOOM 3",
    "UE WONDERBOOM 3",
    "Anker Soundcore 3",
    "Marshall Tufton",
    "Harman Kardon Onyx",
    "Tribit XSound Go",
};
#define BLE_NAME_SPEAKERS_COUNT (sizeof(BLE_NAME_SPEAKERS) / sizeof(BLE_NAME_SPEAKERS[0]))

/* ---- Asset trackers / tags ---- */
static const char* const BLE_NAME_TRACKERS[] = {
    "Tile",
    "Tile Slim",
    "Tile Sticker",
    "Tile Pro",
    "AirTag",
    "SmartTag2",
    "Chipolo ONE",
    "Chipolo CARD",
    "Pebblebee Tag",
    "Orbit Key",
};
#define BLE_NAME_TRACKERS_COUNT (sizeof(BLE_NAME_TRACKERS) / sizeof(BLE_NAME_TRACKERS[0]))

/* ---- Phones / generic ---- */
static const char* const BLE_NAME_PHONES[] = {
    "iPhone",
    "Samsung Galaxy",
    "Pixel 8",
    "Pixel 7a",
    "OnePlus 12",
    "Xiaomi 14",
    "OPPO Find X6",
    "Motorola Edge",
    "Nokia G60",
};
#define BLE_NAME_PHONES_COUNT (sizeof(BLE_NAME_PHONES) / sizeof(BLE_NAME_PHONES[0]))

/* ---- First names used in "<Name>'s <Device>" pattern ---- */
static const char* const BLE_NAME_FIRST_NAMES[] = {
    "James", "John", "Robert", "Michael", "David",
    "William", "Richard", "Joseph", "Thomas", "Daniel",
    "Mary", "Patricia", "Jennifer", "Linda", "Barbara",
    "Susan", "Jessica", "Sarah", "Karen", "Lisa",
    "Emma", "Olivia", "Ava", "Noah", "Liam",
    "Sophia", "Mia", "Charlotte", "Amelia", "Harper",
};
#define BLE_NAME_FIRST_NAMES_COUNT (sizeof(BLE_NAME_FIRST_NAMES) / sizeof(BLE_NAME_FIRST_NAMES[0]))

/* ---- Named-device suffixes (used with first-name pattern) ---- */
static const char* const BLE_NAME_POSSESSIVE_DEVICES[] = {
    "AirPods",
    "Apple Watch",
    "Galaxy Watch",
    "JBL Flip",
    "Tile",
    "Beats",
    "Fitbit",
    "Phone",
    "iPad",
};
#define BLE_NAME_POSSESSIVE_DEVICES_COUNT \
    (sizeof(BLE_NAME_POSSESSIVE_DEVICES) / sizeof(BLE_NAME_POSSESSIVE_DEVICES[0]))
