#pragma once

#include <cstdint>
#include <cstddef>

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

