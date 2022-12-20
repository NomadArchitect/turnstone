/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_PROTOCOLS_H
#define ___NETWORK_PROTOCOLS_H 0

#include <types.h>
#include <network.h>

#define NETWORK_PROTOCOL_ARP 0x0806
#define NETWORK_PROTOCOL_IPV4  0x0800


typedef uint8_t network_ipv4_address_t[4];

typedef uint8_t network_mac_address_t[6];


#endif