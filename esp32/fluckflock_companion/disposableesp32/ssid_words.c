#include "ssid_words.h"

// Primary flavor -- weighted to appear in most generated names (spec.md 5.2.1).
const char *const protest_words[] = {
    "Resist", "Dissent", "Uprising", "Solidarity", "Rebellion", "Freedom",
    "Riot", "Marchers", "Picket", "Strike", "Barricade", "Insurgent",
    "Agitator", "Dissenter", "Rally", "Protest", "Liberty", "Defiant",
    "Unrest", "Uproar", "Outcry", "Mutiny", "Vanguard", "Underground",
    "Comrade", "Ungovernable", "NoMasters", "PowerToThePeople",
    "WeAreLegion", "NeverCompliant",
};
const size_t protest_words_count = sizeof(protest_words) / sizeof(protest_words[0]);

// Mundane home-network flavor so results don't all read as overtly political.
const char *const everyday_words[] = {
    "Pizza", "Garage", "Attic", "Living Room", "Basement", "Rover",
    "Coffee", "Sunset", "TheNest", "Router",
};
const size_t everyday_words_count = sizeof(everyday_words) / sizeof(everyday_words[0]);

// Realistic router-culture suffixes/patterns people actually use.
const char *const suffixes[] = {
    "_5G", "2.4GHz", "Guest", "-EXT", "_Home", "'s Network", " House",
    "HQ", "_2", "!!",
};
const size_t suffixes_count = sizeof(suffixes) / sizeof(suffixes[0]);

// Joiners used when concatenating two words.
const char *const connectors[] = {
    "", "-", "_", " ", "Of", "And",
};
const size_t connectors_count = sizeof(connectors) / sizeof(connectors[0]);
