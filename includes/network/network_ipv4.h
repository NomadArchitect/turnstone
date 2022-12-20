/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_IPV4_H
#define ___NETWORK_IPV4_H 0

#include <types.h>
#include <network.h>
#include <network/network_protocols.h>
#include <network/network_icmpv4.h>
#include <network/network_udpv4.h>

#define NETWORK_IPV4_VERSION 4
#define NETWORK_IPV4_TTL 128

#define NETWORK_IPV4_FLAG_DONT_FRAGMENT  2
#define NETWORK_IPV4_FLAG_MORE_FRAGMENTS 1

typedef enum {
    NETWORK_IPV4_PROTOCOL_ICMPV4=1,
    NETWORK_IPV4_PROTOCOL_IGMPV4=2,
    NETWORK_IPV4_PROTOCOL_TCPV4=6,
    NETWORK_IPV4_PROTOCOL_UDPV4=17,
}network_ipv4_protocol_t;

typedef struct {
    uint32_t header_length  : 4;
    uint32_t version        : 4;
    uint32_t ecn            : 2;
    uint32_t dscp           : 6;
    uint32_t total_length   : 16;
    uint32_t identification : 16;
    union {
        struct {
            uint16_t fragment_offset : 13;
            uint16_t flags           : 3;
        } __attribute__((packed)) fields;
        uint16_t bits;
    }                       flags_fragment_offset;
    uint32_t                ttl             : 8;
    network_ipv4_protocol_t protocol        : 8;
    uint32_t                header_checksum : 16;
    network_ipv4_address_t  source_ip;
    network_ipv4_address_t  destination_ip;
}__attribute__((packed)) network_ipv4_header_t;

extern network_ipv4_address_t NETWORK_IPV4_GLOBAL_BROADCAST_IP;
extern network_ipv4_address_t NETWORK_IPV4_ZERO_IP;

boolean_t              network_ipv4_is_address_eq(network_ipv4_address_t ipv4_addr1, network_ipv4_address_t ipv4_addr2);
uint8_t*               network_ipv4_process_packet(network_ipv4_header_t* recv_ipv4_packet, void* network_info, uint16_t* return_packet_len);
network_ipv4_header_t* network_ipv4_create_packet_from_icmp_packet(network_ipv4_address_t sip, network_ipv4_address_t dip, network_icmpv4_header_t* icmp_hdr, uint16_t icmp_packet_len);
network_ipv4_header_t* network_ipv4_create_packet_from_udp_packet(network_ipv4_address_t sip, network_ipv4_address_t dip, network_udpv4_header_t* udp_hdr);

#endif