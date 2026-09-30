# Architecture

bootpd runs on **one thread**. Both servers and every TFTP transfer are C++20
coroutines scheduled by a small event loop. There are no locks, queues or
worker threads, and shutting down means asking the loop to cancel everything.

```
                          main()
                            │ spawn                     spawn
             ┌──────────────┴───────────────┐
             ▼                              ▼
   dhcp_server_v4::run()           tftp_server_v4::run()
   loop:                           loop:
     co_await recv                   co_await recv
     handle() ─▶ send reply          RRQ ─▶ spawn(tftp_session_v4::serve_read(...))
                                                   │
                                                   ▼
                                         one coroutine per transfer, own socket:
                                           OACK ─▶ co_await ACK 0
                                           DATA n ─▶ co_await ACK n (retransmit on timeout)

   ──────────────────────── async::io_context::run() ────────────────────────
     ready queue ─▶ resume coroutines
     waiters     ─▶ poll()/WSAPoll() on their sockets, until the nearest deadline
```

## The coroutine runtime (`common/async.hpp`)

### `async::task<T>`

A lazily started coroutine that produces a `T` or throws.

* Nothing runs until the task is awaited.
* `co_await some_task()` starts it. When it finishes, the awaiting coroutine
  is resumed by *symmetric transfer*, so long await chains (even recursive
  ones) don't grow the stack.
* An exception thrown inside the task is rethrown from the `co_await`.
* The `task` object owns the coroutine frame. Destroying it destroys the
  frame.

### `async::io_context`

The event loop.

* `spawn(task<void>)` makes a task independent. The context owns it and runs
  it concurrently with the others. An exception that escapes a spawned task
  is logged and ends only that task.
* `wait_readable(socket, deadline)` and `sleep_for/sleep_until` are the only
  suspension points. The awaiting coroutine is parked in a waiter list.
  `run()` resumes whatever is ready, then blocks in `poll()` (or `WSAPoll()`
  on Windows) until a socket becomes readable or the nearest deadline
  passes.
* `run(should_stop)` returns once every spawned task has finished. The
  predicate is checked at least every 100 ms. `main()` passes
  `control_c::stop_requested`, which a signal handler sets.
* `stop()` resumes every waiting coroutine with an `async::operation_cancelled`
  exception, and later waits throw it straight away. The tasks unwind through
  their normal destructors: sockets close and files are released.
  `operation_cancelled` is deliberately **not** a `std::exception`, so the
  servers' `catch (std::exception const&)` error handlers can't swallow it.

### Sockets (`common/socket_udp.hpp`)

`socket_udp` is a non-blocking UDP socket.
`co_await socket.async_recv(context, timeout)` returns the next datagram,
or `std::nullopt` if the timeout passes first. Sending is synchronous: on a
UDP socket, `sendto` doesn't block for a meaningful amount of time, and a
full buffer is treated like a lost packet. All platform differences between
Winsock and BSD sockets live in `common/socket_api.cpp`.

## The servers

### DHCP (`bootpd/dhcp_server_v4.*`)

`run()` receives a datagram, parses it into a `dhcp_packet_v4`, calls
`handle()` and sends the reply, if there is one, to `reply_destination()`.
`handle()` is a pure function of the configuration and the request. That
makes the protocol logic easy to unit test without the network.

### TFTP (`bootpd/tftp_server_v4.*`, `bootpd/tftp_session_v4.*`)

The server coroutine only dispatches:

* A read request spawns `tftp_session_v4::serve_read()` with its own socket,
  and therefore its own transfer ID (TID). Many transfers can run at once.
* A write request is refused. Error packets are ignored. Anything else gets
  an "illegal operation" error.

A session checks the request. It sanitizes the path (`resolve_path`) and
negotiates the options (`negotiate`); both are pure functions with their own
tests. Then it runs the lock-step exchange: send a block, `co_await` its ACK,
retransmit on timeout. Packets from other addresses are answered with
"unknown transfer ID" and otherwise ignored. Duplicate ACKs are ignored
without retransmitting, which avoids the "Sorcerer's Apprentice" problem.

## Serialization (`common/serdes.hpp`)

Packets describe their layout once, in a `serdes(...)` member function. A
`serdes<serdes_reader>` or a `serdes<serdes_writer>` drives it, so the same
code parses and builds a packet. Multi-byte integers are converted to and
from network byte order. Reads go through `memcpy`, so unaligned fields are
safe, and running past the end of the buffer throws.

## Why coroutines instead of threads?

The previous design used two threads per server plus one per TFTP transfer,
passing packets through condition-variable queues. It had shutdown races (a
lost wake-up could hang Ctrl+C), per-transfer threads, and state shared
between threads. With coroutines:

* control flow reads top to bottom, like the blocking code it replaces;
* timeouts are deadlines on an await, not socket options plus exceptions;
* cancellation is one call, and cleanup is plain RAII;
* no state is shared between threads, so there are no data races.
