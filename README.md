# bootpd

A small, self-contained **DHCP/BOOTP + TFTP server** for network booting,
built for developing and testing (legacy) PXE boot loaders.

Point it at a directory, list the machines you want to boot by MAC address,
and it answers their DHCP requests with an address and a boot file, then
serves that file over TFTP. No address pools, no leases database, no
system service to configure: one executable and one `.ini` file.

```
 PXE client                                   bootpd
 ──────────                                   ──────
 DHCPDISCOVER  ─────────────────────────────▶  known MAC? ──▶ DHCPOFFER  (address, server, boot file)
 DHCPREQUEST   ─────────────────────────────▶             ──▶ DHCPACK
 TFTP RRQ "pxelinux.0" (blksize, tsize) ────▶  new session ─▶ OACK, DATA 1..n
```

## Features

* **DHCP and plain BOOTP**: answers DISCOVER with OFFER, REQUEST with ACK and
  BOOTP requests with BOOTREPLY. It ignores unknown machines, so it can share
  a network with other devices.
* **Per-machine configuration**: address, subnet mask, router, boot file,
  host and domain name, lease times.
* **TFTP read server** (RFC 1350) with option negotiation: `blksize`,
  `timeout` and `tsize` (RFC 2347, 2348, 2349). Concurrent transfers,
  retransmission on timeout, duplicate-ACK tolerance.
* **Safe by default**: nothing outside the TFTP root is ever served. `..`,
  absolute paths and escaping symlinks are all refused. Uploads are refused.
* **Single threaded, built on C++20 coroutines**. See
  [docs/architecture.md](docs/architecture.md).
* **Runs on Windows and Linux**.

## Quick start

1. Give the network adapter facing the machines to boot a static address,
   for example `10.0.0.1/8`.
2. Put the boot file (the NBP, e.g. `pxelinux.0` or your own loader) in a
   directory.
3. Write a `config.ini`:

   ```ini
   v4_bind_address = 10.0.0.1
   tftp_base_dir   = ./tftp

   [00-1c-7e-35-ed-20]           ; MAC address of the machine to boot
   boot_file_name    = pxelinux.0
   v4_your_address   = 10.0.0.2  ; address given to that machine
   v4_server_address = 10.0.0.1  ; this computer
   v4_subnet_mask    = 255.0.0.0
   ```

4. Run it (ports 67 and 69 need administrator/root rights on Linux, see
   [Platform notes](#platform-notes)):

   ```sh
   bootpd -C config.ini
   ```

5. Power on the client and choose network boot. Press **Ctrl+C** to stop
   the server.

The full list of settings is in the
[configuration reference](docs/configuration.md). There is a commented
example in [`example/config.ini`](example/config.ini).

## Command line

```
bootpd [options]

  -C <file>         Configuration file (default: config.ini)
  -O "<key=value>"  Override a global configuration value, may be repeated
  -v, --verbose     Debug log output (-vv for trace output)
  -q, --quiet       Only log warnings and errors
  -h, --help        Show help and exit
  -V, --version     Show the version and exit
```

`-O` is handy for one-off changes without editing the file:

```sh
bootpd -C config.ini -O "tftp_base_dir = ./other-build" -O "dhcp_enabled = false"
```

A typical session log:

```
2026-09-30 19:27:05 INFO    DHCP server listening on '10.0.0.1:67', 1 client(s) configured.
2026-09-30 19:27:05 INFO    TFTP server listening on '10.0.0.1:69', serving files from 'C:\pxe\tftp'.
2026-09-30 19:27:09 INFO    Responding to DHCPDISCOVER from '00-1c-7e-35-ed-20' (xid 0x8f3c2a11) with DHCPOFFER, offering 10.0.0.2.
2026-09-30 19:27:10 INFO    Responding to DHCPREQUEST from '00-1c-7e-35-ed-20' (xid 0x8f3c2a11) with DHCPACK, offering 10.0.0.2.
2026-09-30 19:27:10 INFO    From '10.0.0.2:2070' received : RRQ(file="pxelinux.0", mode="octet", tsize="0")
2026-09-30 19:27:10 INFO    Sending 'pxelinux.0' to '10.0.0.2:2070' (42376 bytes, blksize = 512, timeout = 1s) ...
2026-09-30 19:27:10 INFO    Finished sending 'pxelinux.0' to '10.0.0.2:2070' in 0.09s.
```

## Building

You need a C++20 compiler (MSVC 2022, GCC 13+ or Clang 17+), CMake 3.20+ and
[Conan 2](https://conan.io) for the test dependencies (GoogleTest).

```sh
pip install conan
conan profile detect

# Build, run the tests and create the package in one go:
conan build . --build=missing -s compiler.cppstd=20
```

Or step by step, using the CMake presets Conan generates:

```sh
conan install . --build=missing -s compiler.cppstd=20
cmake --preset conan-release          # Windows / Visual Studio: conan-default
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
```

`build.sh` (Linux) and `build.bat` (Windows) do the same and install into
`./workspace`. To build without Conan and without the tests, plain CMake is
enough:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Conan options:

| Option               | Default | Meaning                                    |
|----------------------|---------|--------------------------------------------|
| `with_tests`         | `True`  | Build and run the GoogleTest suite         |
| `warnings_as_errors` | `False` | Fail the build on compiler warnings        |

For example `conan build . -s compiler.cppstd=20 -o "&:with_tests=False"`.

## Tests

The `tests/` directory holds the GoogleTest suite. It covers:

* unit tests for the configuration parser, argument parser, serializer and
  address handling;
* DHCP option/packet encoding, including malformed and hostile input;
* the DHCP reply logic (offers, acks, BOOTP, relays, unknown clients);
* TFTP packets, block reading, option negotiation and path sanitizing;
* the coroutine runtime (tasks, timers, cancellation);
* **end-to-end tests** that run the real DHCP and TFTP servers on loopback
  against small in-process clients: downloads, option negotiation,
  retransmission, duplicate ACKs, concurrent transfers and error cases.

Run them with `ctest` (see above) or directly with
`build/Release/tests/bootpd_tests`. Set `BOOTPD_TEST_LOG=1` to see the
server log while they run.

## Platform notes

**Windows**: when Windows asks, allow `bootpd.exe` through the firewall
(UDP 67 and 69). Binding `v4_bind_address` to the adapter's address is enough
to receive broadcasts on that adapter.

**Linux**:

* Ports below 1024 need root, or grant the capability once:
  `sudo setcap cap_net_bind_service=+ep ./bootpd`.
* Linux doesn't deliver broadcast packets (like DHCPDISCOVER) to a socket
  bound to a specific unicast address. Use `v4_bind_address = 0.0.0.0` on
  Linux, and preferably a machine where the boot network is the only
  network, or the one with the default route.
* Stop other DHCP servers on the same interface (dnsmasq, libvirt's
  default network, ...), or set `dhcp_enabled = false` and use theirs.

## Limitations

* IPv4 only. No DHCPv6 or UEFI HTTP boot.
* Static assignments only: every client needs its own section. There is no
  address pool.
* TFTP is read-only, and `netascii` transfers are sent verbatim, like most
  network boot TFTP servers do.
* No proxyDHCP (port 4011) mode.

## Project layout

```
bootpd/         the servers: DHCP/TFTP packets, DHCP server, TFTP server and sessions, main()
common/         support library: coroutine runtime, sockets, config parser, logger, ...
tests/          GoogleTest suite
docs/           configuration reference and architecture notes
example/        example configuration and Windows launcher
conanfile.py    Conan recipe (dependencies, build, test)
```

## License

MIT, see [LICENSE](LICENSE).
