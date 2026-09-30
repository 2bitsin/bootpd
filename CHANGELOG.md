# Changelog

## 1.1.0

### Architecture

* Replaced the threads (two per server, one per TFTP transfer) and
  condition-variable queues with a single-threaded C++20 coroutine runtime
  (`async::task`, `async::io_context`). See
  [docs/architecture.md](docs/architecture.md).
* Portable socket layer (Winsock and BSD sockets), non-blocking sockets and
  `poll`/`WSAPoll`. **bootpd now builds and runs on Linux as well as Windows.**
* The code is split into a `bootpd_core` library and the `bootpd` executable,
  so the tests can link the library.
* Conan 2 recipe, CMake presets and a GoogleTest suite (98 tests, including
  end-to-end tests over loopback). CI on Linux (GCC, Clang) and Windows (MSVC).

### Security fixes

* **TFTP path traversal**: file names such as `../../secret` or absolute
  paths let a client read any file on the machine. Paths are now confined to
  `tftp_base_dir`, symlinks included.
* **DHCP out-of-bounds access**: a parameter request list containing option
  0 or 255 indexed outside the option table (crash).
* **DHCP out-of-bounds read**: a hardware address length (`hlen`) above 16
  read past the `chaddr` field.
* **TFTP option abuse**: `blksize=0` made the server send empty blocks
  forever. A huge `blksize` allocated that much memory. `timeout=0` could hang
  a session forever, and non-numeric values killed the session without an
  error packet. Options are now range-checked (RFC 2348/2349) and ignored
  when invalid.

### Bug fixes

DHCP
* String options (host name, domain name, boot file name) were encoded with
  the size of the C++ string object, not the text. That meant 32 or 16
  bytes of zero padding, or an exception for longer values.
* Client sections written in upper case (`[00-1C-7E-...]`) were never
  matched, and their settings were read from the wrong section.
* The global settings section was treated as a client.
* `dhcp_options_v4`: `set()` didn't return a value (undefined behaviour),
  move assignment recursed infinitely, and the move constructor left the
  cookie uninitialized.
* Plain BOOTP requests (no DHCP message type) got no reply, although the
  server is a BOOTP server.
* A DHCPREQUEST that selected a different DHCP server was still acknowledged.
* Replies ignored the relay agent (`giaddr`) and the client's `ciaddr`, and
  always forced the broadcast flag. They now follow RFC 2131, 4.1.
* Replies are padded to the 300 byte BOOTP minimum.
* The default rebinding time (7200s) was shorter than the renewal time,
  which is invalid.
* Options are parsed leniently: a missing END option or a truncated option
  no longer drops the whole packet. Repeated options are concatenated
  (RFC 3396).

TFTP
* A duplicate ACK aborted the transfer. Now it's ignored, without
  retransmitting (the Sorcerer's Apprentice problem).
* A packet from a stray address was answered with an error, *and then
  processed as if it came from the client*.
* An empty OACK was sent when none of the requested options were accepted.
* Option names and the transfer mode were case sensitive (the RFCs say they
  aren't).
* `netascii` files were opened in text mode on Windows, which broke sizes
  and offsets.
* Error packets are no longer answered with error packets.
* Unknown opcodes are reported as errors.

Common
* Config parser: `key =` with an empty value was undefined behaviour, line
  numbers in error messages were wrong, invalid values were silently
  replaced by defaults, and a `;` inside a quoted value started a comment.
  UTF-8 BOMs and CRLF files are handled.
* `lexical_cast`: `0B` binary prefix typo; booleans were parsed as numbers
  (the bool branch was unreachable); no range checks (`70000` as a port
  became `4464`).
* Logger: every line contained stray NUL characters, format strings were
  only checked at runtime, and `localtime` isn't thread safe.
* Serializer: unaligned and type-punned reads (undefined behaviour); byte
  order conversion was wrong on big-endian hosts.
* Functions marked `noexcept` that could throw (which calls
  `std::terminate`): `tftp_packet::opcode`, `tftp_packet::serdes_size_hint`
  and `address_v4::to_string`.
* The old worker queue could lose the stop notification and hang on Ctrl+C.
* Windows: an ICMP "port unreachable" made the next `recvfrom` fail with
  `WSAECONNRESET`.
* `std::tolower` was called with negative `char` values (undefined behaviour).
* `main()` no longer runs `system("pause")` on error, which printed an error
  on Linux. The example `bootpd.bat` pauses instead.
* Removed unused and non-compiling code (`utility_file.hpp`,
  `mac_address.hpp`, `generic_error.hpp`, CRC32 and hash helpers).

### New

* Command line options `-h/--help`, `-V/--version`, `-v/--verbose`, `-vv`
  and `-q/--quiet`.
* `dhcp_enabled` and `tftp_enabled` settings, to run just one of the servers.
* MAC addresses in section names may use `:` or `-`, in any case.
