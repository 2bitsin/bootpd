#pragma once

#include <cstddef>
#include <cstdint>

static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_DISCOVER = 1u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_OFFER = 2u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_REQUEST = 3u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_DECLINE = 4u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_ACK = 5u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_NAK = 6u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_RELEASE = 7u;
static inline constexpr const std::uint8_t DHCP_MESSAGE_TYPE_INFORM = 8u;

static inline constexpr const std::uint32_t DHCP_MAGIC_COOKIE = 0x63825363;

static inline constexpr const std::uint8_t DHCP_HARDWARE_TYPE_ETHERNET = 1u;

static inline constexpr const std::uint8_t DHCP_OPCODE_REQUEST = 1u;
static inline constexpr const std::uint8_t DHCP_OPCODE_RESPONSE = 2u;

static inline constexpr const std::uint16_t DHCP_FLAGS_BROADCAST = 0x8000u;

static inline constexpr const std::uint16_t DHCP_SERVER_PORT = 67u;
static inline constexpr const std::uint16_t DHCP_CLIENT_PORT = 68u;

// BOOTP messages are at least 300 bytes long (RFC 951), some clients drop shorter replies.
static inline constexpr const std::size_t DHCP_MINIMUM_PACKET_SIZE = 300u;

// Option codes (RFC 2132)
static inline constexpr const std::uint8_t DHCP_OPTION_PAD = 0u;
static inline constexpr const std::uint8_t DHCP_OPTION_SUBNET_MASK = 1u;
static inline constexpr const std::uint8_t DHCP_OPTION_ROUTER = 3u;
static inline constexpr const std::uint8_t DHCP_OPTION_LOG_SERVER = 7u;
static inline constexpr const std::uint8_t DHCP_OPTION_HOST_NAME = 12u;
static inline constexpr const std::uint8_t DHCP_OPTION_DOMAIN_NAME = 15u;
static inline constexpr const std::uint8_t DHCP_OPTION_LEASE_TIME = 51u;
static inline constexpr const std::uint8_t DHCP_OPTION_MESSAGE_TYPE = 53u;
static inline constexpr const std::uint8_t DHCP_OPTION_SERVER_IDENTIFIER = 54u;
static inline constexpr const std::uint8_t DHCP_OPTION_PARAMETER_REQUEST_LIST = 55u;
static inline constexpr const std::uint8_t DHCP_OPTION_RENEWAL_TIME = 58u;
static inline constexpr const std::uint8_t DHCP_OPTION_REBINDING_TIME = 59u;
static inline constexpr const std::uint8_t DHCP_OPTION_TFTP_SERVER_NAME = 66u;
static inline constexpr const std::uint8_t DHCP_OPTION_BOOT_FILE_NAME = 67u;
static inline constexpr const std::uint8_t DHCP_OPTION_END = 255u;
