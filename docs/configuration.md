# Configuration reference

bootpd reads a single INI file (`config.ini` by default, or the file given
with `-C`).

## Syntax

```ini
; comment
key = value            ; trailing comment
key = "quoted ; value" ; quotes are removed, ';' inside quotes is kept

[section]
key = value
```

* Blank lines are ignored, and leading and trailing whitespace is trimmed.
* Keys before the first `[section]` are **global settings**. Each section
  describes **one client machine**.
* Integers may be written in decimal, or in hex, octal or binary with a
  `0x`, `0o` or `0b` prefix. Booleans accept `true/false`, `yes/no`, `on/off`
  and `1/0`.
* Addresses may be IPv4 literals or host names. Host names are resolved once,
  at startup.
* An invalid value (for example `tftp_listen_port = 70000`) stops the server
  at startup with an error that names the key. It is never silently replaced
  by the default.
* Files saved with a UTF-8 byte order mark or with Windows line endings are
  accepted.

Global values can be overridden from the command line with
`-O "key = value"`.

## Global settings

| Key                | Default   | Description |
|--------------------|-----------|-------------|
| `v4_bind_address`  | `0.0.0.0` | Address of the network adapter to serve on. `0.0.0.0` means all adapters (required on Linux for DHCP, see the README). |
| `dhcp_enabled`     | `true`    | Run the DHCP/BOOTP server. |
| `dhcp_listen_port` | `67`      | UDP port of the DHCP server. |
| `tftp_enabled`     | `true`    | Run the TFTP server. |
| `tftp_listen_port` | `69`      | UDP port of the TFTP server. |
| `tftp_base_dir`    | `./`      | Root directory of the TFTP server, relative to the working directory. Must exist. Files outside of it are never served. |

## Client sections

Each client is a section named after its MAC address. Dashes or colons and
any letter case are accepted, so `[00-1c-7e-35-ed-20]` and
`[00:1C:7E:35:ED:20]` are the same client. The server doesn't answer
machines that have no section. Two sections for the same MAC address are an
error.

| Key                      | Default                 | DHCP field / option | Description |
|--------------------------|-------------------------|---------------------|-------------|
| `v4_your_address`        | `0.0.0.0`               | `yiaddr`            | The address given to the client. |
| `v4_server_address`      | `0.0.0.0`               | `siaddr`            | The "next server": where the client downloads its boot file from, normally this computer. |
| `boot_file_name`         | *(empty)*               | `file`, option 67   | The file to boot (the NBP), relative to `tftp_base_dir`. Under 128 characters. |
| `server_host_name`       | *(empty)*               | `sname`, option 12  | Server host name. Under 64 characters. |
| `v4_subnet_mask`         | `0.0.0.0`               | option 1            | Subnet mask. |
| `v4_router_address`      | `v4_server_address`     | option 3            | Default gateway. |
| `v4_log_server_address`  | `v4_server_address`     | option 7            | Log server. |
| `v4_dhcp_server_address` | `v4_server_address`     | option 54           | DHCP server identifier. A DHCPREQUEST that names a different server is ignored. |
| `domain_name`            | `localhost`             | option 15           | Domain name. |
| `address_lease_time`     | `172800`                | option 51           | Lease time, in seconds. |
| `address_renewal_time`   | `86400`                 | option 58           | Renewal (T1) time, in seconds. |
| `address_rebinding_time` | `151200`                | option 59           | Rebinding (T2) time, in seconds. |

Every reply includes options 1, 7, 15, 51, 54, 58 and 59, plus any other
configured option the client asks for in its parameter request list
(option 55). Replies to plain BOOTP clients (requests without a DHCP message
type) leave out the DHCP-only options 51, 54, 58 and 59.

Older configuration files may contain `v4_client_address` and
`v4_gateway_address`. They are accepted but ignored: per RFC 2131, `ciaddr`
and `giaddr` in a reply are taken from the client's request.

## Where replies are sent

As RFC 2131 (section 4.1) specifies:

1. If the request came through a DHCP relay (`giaddr` set), the reply goes
   to the relay on port 67.
2. If the client already has an address (`ciaddr` set), the reply is sent
   there by unicast.
3. Otherwise the reply is broadcast to `255.255.255.255`.

## TFTP details

* Only read requests are served. Write requests get an "access violation"
  error.
* Transfer modes `octet` and `netascii` are accepted. Both are sent as-is.
* File names may use `/` or `\` as separators. A leading `/` is relative to
  `tftp_base_dir`. Names containing `..` that leave the root, absolute Windows
  paths (`C:\...`) and symlinks pointing outside the root are refused.
* Supported options:

  | Option    | Range       | Meaning |
  |-----------|-------------|---------|
  | `blksize` | 8 to 65464  | Block size in bytes (RFC 2348). Default 512. |
  | `timeout` | 1 to 255    | Retransmission timeout in seconds (RFC 2349). Default 1. |
  | `tsize`   | -           | The server replies with the file size (RFC 2349). |

  Unknown or out-of-range options are ignored, as RFC 2347 allows. If no
  option is accepted, the server sends the first DATA block straight away
  instead of an OACK.
* A block that isn't acknowledged is retransmitted up to 4 times before the
  transfer is aborted.
* Block numbers wrap around to 0 after 65535, so files larger than
  `65535 × blksize` work with clients that support rollover.

## Example

See [`example/config.ini`](../example/config.ini).
