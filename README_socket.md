# socketToSpread

`socketToSpread` is a C-based gateway daemon that bridges TCP network socket clients (such as netcat `nc` or custom applications) to a [Spread Toolkit](https://www.spread.org/) group communication system, backed by an SQLite cache.

## Features

- **TCP Server Socket:** Listens on a configurable TCP port (default: `9090`) for incoming client connections.
- **Spread Integration:** Connects to a Spread daemon to multicast messages and join groups.
- **SQLite Caching:** Tracks node states (`CONNECTED`, `DISCONNECTED`, `LEAVE`) and membership changes in an SQLite database.
- **Interactive Control Commands:** Supports runtime configuration via command tokens (`^set`, `^get`, `^join`, `^leave`, `^dump`, `^start`, `^exit`).

---

## Prerequisites

- **Spread Toolkit** (headers and library installed, typically in `/usr/local`)
- **SQLite3** library
- **Pthreads** library
- A standard C compiler (`gcc`) and `make`

---

## Compilation

The project comes with a comprehensive `Makefile`. To build all binaries (including `socketToSpread` and `socketTestClient`), run:

```bash
make clean
make all
```

---

## Configuration

A startup configuration file (`$HOME/.start.rc`) must exist for `socketToSpread` upon launch. It uses `^set` syntax to define persistent variables.

Example `.start.rc`:
```text
^set DATABASE /var/tmp/user.db
^set GROUP global
^set SERVER 4803
^set ALTSERVER localhost:4803
^set STALE_AFTER 600
```

---

## Usage

### 1. Starting the Daemon

Run `socketToSpread` with optional arguments:

```bash
./socketToSpread -p 9090 -d /var/tmp/mycache.db
```

**Command-Line Options:**
- `-c <cfg>` : Specify custom config file.
- `-d <database>` : Set SQLite cache database path.
- `-p <port>` : TCP port to listen on (default: `9090`).
- `-l <logfile>` : Log file for debugging output.
- `-h` : Display help menu.
- `-P` : Print configuration and exit.

### 2. Connecting with Netcat (`nc`)

You can interactively send commands or multicast messages to the Spread network using `nc`:

```bash
nc localhost 9090
```

### 3. Using the Test Client

Alternatively, use the provided `socketTestClient` utility:

```bash
./socketTestClient -H 127.0.0.1 -p 9090
```

---

## Available Commands

Once connected via a TCP client, you can type commands prefixed with `^`:

- `^help` : List available commands.
- `^start` : Connect to the Spread daemon and start listening for inbound messages.
- `^join <group>` : Join a Spread group.
- `^leave <group>` : Leave a Spread group.
- `^set <key> <val>` : Set a runtime configuration variable.
- `^get <key>` : Retrieve a configuration variable value.
- `^dump` : Dump current symbol table and SQLite cache state.
- `^connect` / `^disconnect` : Manage Spread connection state.
- `^exit` : Terminate the session/daemon connection.

Any raw text entered without a `^` prefix will be automatically multicast to the active Spread group.