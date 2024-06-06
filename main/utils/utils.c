#include "utils.h"

/*
  * Function: mac_to_hex_string
  * ----------------------------
  *   Convert a MAC address to a string
  *
*/
char * mac_to_hex_string(const uint8_t *mac) {
    char *hex_str = (char *) malloc(sizeof(char) * 18);
    sprintf(hex_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return hex_str;
}