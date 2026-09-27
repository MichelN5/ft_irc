# ft_irc

## 1. Project Overview

`ft_irc` is a standards-compliant Internet Relay Chat (IRC) server written in C++98. The program produces an executable named `ircserv` that multiplexes network I/O across all client connections using a single non-blocking `poll()` event loop. It implements the essential client-server protocol requirements of [RFC 1459](https://tools.ietf.org/html/rfc1459) and [RFC 2812](https://tools.ietf.org/html/rfc2812). This project implements the **server only**; it does not implement an IRC client, nor does it implement server-to-server linking or multi-server mesh federation, which are strictly outside the 42 subject requirements.

The designated reference client used to validate this server is **Irssi**, chosen for its strict conformance to IRC protocol standards, lightweight terminal footprint, and deterministic handling of numeric replies.

---

## 2. Goal / Scope

### Implemented Functionality (Mandatory Scope)
- **Authentication & Registration**: `PASS` password validation, `NICK` nickname assignment and in-session alteration, `USER` user registration, registration welcome burst (`001`, `002`, `003`, `004`), `PING`/`PONG` heartbeat, and `CAP` capability negotiation acknowledgment.
- **Channel Operations & Messaging**: `JOIN` (channel creation, automatic first-member operator grant, comma-separated channel lists, keys), `PART` (channel departure with reason), and `PRIVMSG` (private user-to-user and channel-wide broadcast, strictly excluding the sender).
- **Channel Operator Privileges & Commands**:
  - `KICK`: Forceful member removal with optional comment.
  - `INVITE`: Scoped per-channel-per-user invitations lifting invite-only restrictions for target users.
  - `TOPIC`: Query current topic (`331`/`332`) and update topic (enforced by `+t`).
  - `MODE`: Complete channel mode manipulation with support for single, compound, and mixed signs, plus bare mode query (`324 RPL_CHANNELMODEIS`) and user mode query (`221 RPL_UMODEIS`).
- **Channel Modes (All 5 Flags)**:
  - `i`: Invite-only channel toggling (`+i` / `-i`).
  - `t`: Operator-restricted topic modification (`+t` / `-t`).
  - `k`: Channel key/password requirement (`+k <key>` / `-k`).
  - `o`: Operator privilege delegation and revocation (`+o <nick>` / `-o <nick>`).
  - `l`: Maximum channel user capacity limit (`+l <count>` / `-l`).
- **Privilege Separation**: Strict enforcement returning numeric `482 ERR_CHANOPRIVSNEEDED` whenever a non-operator attempts an operator-gated command or mode flag.
- **Robustness**: Complete tolerance against TCP packet fragmentation, chunked commands, ungraceful socket closure mid-command, backlog queuing for suspended clients, and clean signal handling (`SIGINT`, `SIGTERM`, and ignored `SIGPIPE`).

### Out of Scope
In accordance with the 42 project subject:
- **Bonus Features**: DCC file transfer and IRC bot are not part of the mandatory specification and are not included.
- **Server Linking**: Server-to-server (`SERVER`, `SQUIT`) routing and IRC network clustering are excluded.
- **Client Software**: This repository builds `ircserv` exclusively.

---

## 3. Build & Run Instructions

### Prerequisites
- **Operating System**: Linux (Ubuntu 20.04 / 22.04 LTS recommended; POSIX sockets and `poll()` API).
- **Compiler**: `c++` or `g++` supporting `-std=c++98`.
- **Build System**: GNU `make`.

### Compilation
From the root of the repository, execute:
```bash
make        # Compiles the ircserv executable with -Wall -Wextra -Werror -std=c++98
make clean  # Removes object files (.o)
make fclean # Removes object files and the ircserv binary
make re     # Performs a clean re-compilation from scratch
```

### Execution
```bash
./ircserv <port> <password>
```
- `<port>`: The TCP port number the server will bind and listen on (valid range: `1` to `65535`).
- `<password>`: A non-empty connection password required for client authentication.

**Concrete Example:**
```bash
./ircserv 6667 mysecretpassword
```

### Connecting Clients

#### Option A: Reference Client (Irssi)
Install Irssi (if not already installed):
```bash
sudo apt-get update && sudo apt-get install -y irssi
```
Launch Irssi and connect using the `/connect` command:
```text
irssi
/connect -password mysecretpassword 127.0.0.1 6667 alice
```

#### Option B: Raw Terminal Client (Netcat)
Netcat allows manual inspection of raw RFC messages and exact numeric responses:
```bash
nc 127.0.0.1 6667
```
Once connected, authenticate manually:
```text
PASS mysecretpassword
NICK alice
USER alice 0 * :Alice Wonderland
```

---

## 4. Architecture Summary

The server is built around a single-threaded, event-driven, non-blocking architecture designed to satisfy all 42 School non-blocking constraints:
- **Single Multiplexing Call Site**: Exactly one `poll()` call site exists in the entire codebase, located inside `Server::run()` in [Server.cpp](Server.cpp#L88-L146). All active sockets—the listening server socket (`serverFd`) and all client sockets—are registered within a single `std::vector<struct pollfd> pollFds` container and evaluated on each iteration.
- **Poll-First Precondition**: Syscalls (`accept`, `recv`, `send`) are never executed unconditionally. A socket is read from only when `poll()` sets `POLLIN`, and written to only when `poll()` sets `POLLOUT`.
- **Strict `fcntl` Configuration**: Sockets are set to non-blocking mode exclusively using the bare form `fcntl(fd, F_SETFL, O_NONBLOCK)` without `F_GETFL` or compound flags (see `Server::setNonBlocking()` in [Server.cpp](Server.cpp#L150-L156)).
- **Non-blocking Retry Handling**: After `poll()` reports readiness, `recv()` and `send()` still handle `EAGAIN`, `EWOULDBLOCK`, and `EINTR` as retryable conditions. EOF and non-retryable socket errors trigger connection cleanup (see `Server::readClient()` and `Server::writeClient()` in [Server.cpp](Server.cpp)).
- **No Subprocesses**: The server does not use `fork()` or multi-threading; all client operations execute in a unified non-blocking memory space.

---

## 5. Command Reference

| Command | Syntax | Description | Example |
| :--- | :--- | :--- | :--- |
| `PASS` | `PASS <password>` | Supplies the connection authentication password. | `PASS mysecretpassword` |
| `NICK` | `NICK <nickname>` | Sets or alters the client's nickname. | `NICK alice` |
| `USER` | `USER <username> <hostname> <server> :<realname>` | Sets username and real name; completes registration. | `USER alice 0 * :Alice Smith` |
| `JOIN` | `JOIN <channel>[,<chans>] [<key>[,<keys>]]` | Enters or creates channel(s); first member becomes operator. | `JOIN #chat pass123` |
| `PART` | `PART <channel> [:<reason>]` | Leaves the specified channel with an optional parting comment. | `PART #chat :Going to lunch` |
| `PRIVMSG` | `PRIVMSG <target> :<message>` | Sends a private message to a user or broadcasts to a channel. | `PRIVMSG #chat :Hello team!` |
| `QUIT` | `QUIT [:<reason>]` | Disconnects the client and broadcasts the reason to peers. | `QUIT :Client exiting` |
| `KICK` | `KICK <channel> <nick> [:<comment>]` | Operator forcibly expels a user from a channel. | `KICK #chat bob :Rule violation` |
| `INVITE` | `INVITE <nick> <channel>` | Invites a user to a channel (required when `+i` is active). | `INVITE charlie #chat` |
| `TOPIC` | `TOPIC <channel> [:<newtopic>]` | Queries current topic or modifies it (operator-gated under `+t`). | `TOPIC #chat :Project Discussion` |
| `MODE (query)` | `MODE <channel>` | Queries currently active channel modes and their parameters. | `MODE #chat` |
| `MODE +i / -i` | `MODE <channel> +i` / `-i` | Sets or removes invite-only restriction. | `MODE #chat +i` |
| `MODE +t / -t` | `MODE <channel> +t` / `-t` | Restricts topic alterations to channel operators (`+t`), or opens it (`-t`). | `MODE #chat +t` |
| `MODE +k / -k` | `MODE <channel> +k <key>` / `-k` | Sets or clears channel password/key required to join. | `MODE #chat +k secret123` |
| `MODE +o / -o` | `MODE <channel> +o <nick>` / `-o <nick>` | Grants or revokes channel operator status. | `MODE #chat +o bob` |
| `MODE +l / -l` | `MODE <channel> +l <limit>` / `-l` | Sets or removes maximum simultaneous user limit. | `MODE #chat +l 25` |

---

## 6. Full Testing Guide

This section is structured to mirror the sections of the 42 Evaluation Sheet in exact sequence. Every test can be conducted manually using standard terminal tools (`nc` and `irssi`).

---

### Part A: Basic Code-Inspection Checks

Before launching the server, verify the core architectural constraints directly in the code:

1. **Makefile Check**:
   - Run `make re`. Ensure it compiles cleanly with flags `-Wall -Wextra -Werror -std=c++98` with zero warnings or errors.
2. **Single Multiplexing Call Site (Gate A)**:
   - Run:
     ```bash
     grep -rn "poll(" .
     grep -rn "select(" .
     grep -rn "epoll_" .
     grep -rn "kqueue(" .
     ```
   - **Success**: Exactly one multiplexing call site exists in the codebase: `poll(&pollFds[0], pollFds.size(), -1);` in `Server::run()` at [Server.cpp:95](Server.cpp#L95). No `select`, `epoll`, or `kqueue` calls exist.
3. **Strict `fcntl` Form (Gate D)**:
   - Run:
     ```bash
     grep -rn "fcntl(" .
     ```
   - **Success**: The only occurrence is `fcntl(fd, F_SETFL, O_NONBLOCK);` in `Server::setNonBlocking()` at [Server.cpp:152](Server.cpp#L152). No `F_GETFL`, compound bitwise expressions, or alternate flags exist.
4. **Non-blocking Error Handling**:
   - Run:
     ```bash
      grep -rn "EAGAIN" .
      grep -rn "EWOULDBLOCK" .
      ```
   - **Success**: `recv()` and `send()` treat `EAGAIN` and `EWOULDBLOCK` as temporary conditions and keep the client connected. EOF and other socket errors cleanly disconnect only the affected client.
5. **No Forking (Gate E)**:
   - Run:
     ```bash
     grep -rn "fork(" .
     ```
   - **Success**: Zero matches. The program is strictly single-process.

---

### Part B: Networking Verification

#### 1. Argument Parsing & Port Binding
Start the server in Terminal 1:
```bash
./ircserv 6667 mypassword
```
- **Success**: The server starts, prints no runtime errors, and blocks cleanly in its event loop.
- Verify binding to all interfaces:
  ```bash
  ss -tulpn | grep 6667
  # or
  netstat -tulpn | grep 6667
  ```
  **Success**: Shows `0.0.0.0:6667` in `LISTEN` state.

#### 2. Simultaneous Connections (`nc` and `irssi`)
- **Terminal 2 (Irssi)**:
  ```bash
  irssi
  /connect -password mypassword 127.0.0.1 6667 alice
  ```
  **Success**: Alice connects and receives welcome numerics (`001`-`004`).
- **Terminal 3 (netcat)**:
  ```bash
  nc 127.0.0.1 6667
  ```
  Send:
  ```text
  PASS mypassword
  NICK bob
  USER bob 0 * :Bob Builder
  ```
  **Success**: Bob connects simultaneously without interrupting Alice.
- **Channel Join and Broadcast**:
  - In Terminal 2 (Alice):
    ```text
    /join #test
    ```
  - In Terminal 3 (Bob):
    ```text
    JOIN #test
    ```
    Bob receives `:bob!bob@localhost JOIN :#test` along with `353 RPL_NAMREPLY` showing `@alice bob`.
  - In Terminal 3 (Bob):
    ```text
    PRIVMSG #test :Hello Alice!
    ```
  - In Terminal 2 (Alice): Alice sees `<bob> Hello Alice!`.
  - **Failure Condition**: Server blocks, hangs, echoes message back to Bob, or drops either connection.

---

### Part C: Networking Specials (Robustness & Edge Cases)

#### 1. Partial Command / Fragmentation Test (Exact Subject Test)
Open a new netcat connection in Terminal 4:
```bash
nc 127.0.0.1 6667
```
Send the authentication command in deliberately fragmented chunks without a newline:
```text
PA
```
Wait a few seconds. Meanwhile, in Terminal 2 (Alice), verify Irssi is still completely responsive by typing a message:
```text
/privmsg #test Still alive?
```
Alice receives her normal interaction. Now complete the command in Terminal 4:
```text
SS mypassword
NICK fraguser
USER frag 0 * :Fragmented User
```
- **Success**: `fraguser` registers successfully and receives `001 RPL_WELCOME`.
- **Failure Condition**: The server hangs while waiting for the rest of `PASS`, or drops other clients.

#### 2. Client Mid-Session Abrupt Termination
With `fraguser` still connected in Terminal 4:
- Kill the netcat client abruptly with `Ctrl+C` or by terminating its terminal window.
- Look at Terminal 2 (Alice in `#test`) or Terminal 3 (Bob in `#test`).
- **Success**: Server stays up without crashing, cleans up `fraguser`'s socket, and remaining clients remain fully operational.

#### 3. Netcat Killed Mid-Partial-Command
- In a new terminal:
  ```bash
  nc 127.0.0.1 6667
  ```
- Send partial bytes:
  ```text
  PAS
  ```
- Immediately press `Ctrl+C` while the command is incomplete.
- **Success**: Server does not crash, does not leak the open file descriptor, and continues serving active clients normally.

#### 4. Suspended Client Flood Backlog Delivery (`Ctrl+Z` / `SIGSTOP`)
- In Terminal 3 (Bob): Suspend the netcat process using `Ctrl+Z` (sends `SIGSTOP`).
- In Terminal 2 (Alice in `#test`): Send 5 messages rapidly:
  ```text
  /privmsg #test Message 1
  /privmsg #test Message 2
  /privmsg #test Message 3
  /privmsg #test Message 4
  /privmsg #test Message 5
  ```
- In Terminal 3 (Bob): Resume the process by typing:
  ```bash
  fg
  ```
- **Success**: Bob immediately receives all 5 backlogged messages in exact sequential order without message corruption or truncation.

---

### Part D: Basic Client Commands

#### 1. Authentication Edge Cases
In a fresh netcat session:
```bash
nc 127.0.0.1 6667
```
- **Wrong Password**:
  ```text
  PASS wrongpass
  NICK testuser
  USER test 0 * :Test
  ```
  **Success**: Server replies `464 * :Password incorrect`.
- **Flexible Ordering (NICK/USER before PASS)**:
  Reconnect:
  ```bash
  nc 127.0.0.1 6667
  ```
  Send:
  ```text
  NICK ordereduser
  USER ord 0 * :Ordered
  ```
  *(Client is not yet registered)*. Now send `PASS`:
  ```text
  PASS mypassword
  ```
  **Success**: Registration completes immediately upon receiving `PASS`, sending `001`-`004`.
- **Duplicate Nickname Collision (433)**:
  In another terminal:
  ```bash
  nc 127.0.0.1 6667
  PASS mypassword
  NICK ordereduser
  ```
  **Success**: Server rejects with `433 * ordereduser :Nickname is already in use`.

#### 2. Channel Operations & PRIVMSG
- **Channel Join**:
  ```text
  JOIN #demo
  ```
  **Success**: Server responds with `JOIN :#demo`, topic reply (`331` or `332`), `353 RPL_NAMREPLY` listing `@ordereduser`, and `366 RPL_ENDOFNAMES`.
- **Direct User-to-User PRIVMSG**:
  Connect a second user `receiver` in another terminal, then from `ordereduser`:
  ```text
  PRIVMSG receiver :Secret direct note
  ```
  **Success**: `receiver` receives `:ordereduser!ord@localhost PRIVMSG receiver :Secret direct note`.
- **PRIVMSG Error Numerics**:
  - `PRIVMSG` -> returns `411 :No recipient given (PRIVMSG)`
  - `PRIVMSG receiver` -> returns `412 :No text to send`
  - `PRIVMSG nonexistentnick :hi` -> returns `401 nonexistentnick :No such nick/channel`
  - `PRIVMSG #nonexistentchan :hi` -> returns `403 #nonexistentchan :No such channel`
- **PART Channel**:
  ```text
  PART #demo :Leaving now
  ```
  **Success**: Server broadcasts `:ordereduser!ord@localhost PART #demo :Leaving now` and removes the user from `#demo`.
- **Cannot Send to Channel When Not Member (404)**:
  While not on `#demo`, send:
  ```text
  PRIVMSG #demo :Hello?
  ```
  **Success**: Server rejects with `404 #demo :Cannot send to channel`.

---

### Part E: Channel Operator Commands & Modes

Set up the operator test environment with two clients:
- **Client 1 (Operator)**: Netcat or Irssi connected as `op_user`. Joins `#ops` first (auto-granted `@` operator).
- **Client 2 (Regular Member)**: Netcat or Irssi connected as `reg_user`. Joins `#ops` second (regular member).

#### 1. Non-Operator Privilege Rejection (`482`)
From **Client 2 (`reg_user`)**, attempt operator actions:
```text
KICK #ops op_user :bye
TOPIC #ops :Hacked Topic
MODE #ops +i
MODE #ops +t
MODE #ops +k secretkey
MODE #ops +o reg_user
MODE #ops +l 10
```
- **Success**: Each command is individually rejected with numeric `:ircserv 482 reg_user #ops :You're not channel operator`.
- **Failure Condition**: Any of the actions succeed or return an incorrect error code.

#### 2. KICK Command
From **Client 1 (`op_user`)**:
```text
KICK #ops reg_user :Rule violation
```
- **Success**: Both clients receive `:op_user!op@localhost KICK #ops reg_user :Rule violation`. `reg_user` is removed from `#ops`.
- Re-join `reg_user`:
  ```text
  JOIN #ops
  ```

#### 3. Topic Restriction (`+t` / `-t`) & Topic Query
- Under default `+t`, `reg_user` cannot alter topic (verified above).
- From **Client 1 (`op_user`)**:
  ```text
  TOPIC #ops :Official Channel Topic
  ```
  **Success**: Broadcasts `:op_user!op@localhost TOPIC #ops :Official Channel Topic`.
- From **Client 2 (`reg_user`)**: Query topic:
  ```text
  TOPIC #ops
  ```
  **Success**: Returns `:ircserv 332 reg_user #ops :Official Channel Topic`.
- From **Client 1 (`op_user`)**: Remove `+t`:
  ```text
  MODE #ops -t
  ```
- From **Client 2 (`reg_user`)**: Now alter topic:
  ```text
  TOPIC #ops :Topic changed by regular user
  ```
  **Success**: Broadcasts `:reg_user!reg@localhost TOPIC #ops :Topic changed by regular user`.

#### 4. Channel Key / Password (`+k` / `-k`)
From **Client 1 (`op_user`)**:
```text
MODE #ops +k secretkey
```
In a third terminal, connect `user3`:
```bash
nc 127.0.0.1 6667
PASS mypassword
NICK user3
USER u3 0 * :User 3
```
- Try to join without key:
  ```text
  JOIN #ops
  ```
  **Success**: Rejected with `:ircserv 475 user3 #ops :Cannot join channel (+k)`.
- Join with correct key:
  ```text
  JOIN #ops secretkey
  ```
  **Success**: Joins `#ops` successfully (`:user3!u3@localhost JOIN :#ops`).
- `user3` parts: `PART #ops :bye`.
- From **Client 1 (`op_user`)**: Remove key: `MODE #ops -k secretkey`.

#### 5. Invite-Only Mode (`+i` / `-i`) & INVITE
From **Client 1 (`op_user`)**:
```text
MODE #ops +i
```
- Non-operator blocked from inviting under `+i`: From **Client 2 (`reg_user`)**:
  ```text
  INVITE user3 #ops
  ```
  **Success**: Rejected with `:ircserv 482 reg_user #ops :You're not channel operator`.
- From `user3`: Try to join without invite:
  ```text
  JOIN #ops
  ```
  **Success**: Rejected with `:ircserv 473 user3 #ops :Cannot join channel (+i)`.
- From **Client 1 (`op_user`)**: Invite `user3`:
  ```text
  INVITE user3 #ops
  ```
  **Success**: `op_user` receives `:ircserv 341 op_user user3 #ops`. `user3` receives `:op_user!op@localhost INVITE user3 :#ops`.
- Duplicate invite check: From `op_user`, invite `reg_user` (already in channel):
  ```text
  INVITE reg_user #ops
  ```
  **Success**: Rejected with `:ircserv 443 op_user reg_user #ops :is already on channel`.
- From `user3`: Join now:
  ```text
  JOIN #ops
  ```
  **Success**: `user3` joins successfully (one-time invitation consumed).
- Lift `+i`: From `op_user`, `MODE #ops -i`.

#### 6. User Limit (`+l` / `-l`)
Currently `#ops` has 3 members (`op_user`, `reg_user`, `user3`).
From `user3`: Leave channel:
```text
PART #ops :leaving
```
(Now 2 members remain in `#ops`).
From **Client 1 (`op_user`)**: Set capacity to 2:
```text
MODE #ops +l 2
```
- From `user3`: Try to join:
  ```text
  JOIN #ops
  ```
  **Success**: Rejected with `:ircserv 471 user3 #ops :Cannot join channel (+l)`.
- From `reg_user`: Leave channel:
  ```text
  PART #ops :freeing slot
  ```
- From `user3`: Try to join again:
  ```text
  JOIN #ops
  ```
  **Success**: Joins successfully now that a free slot is available.
- From `op_user`: Remove limit: `MODE #ops -l`.

#### 7. Operator Status Grant & Revoke (`+o` / `-o`)
- From **Client 1 (`op_user`)**: Grant operator to `user3`:
  ```text
  MODE #ops +o user3
  ```
  **Success**: Broadcasts `:op_user!op@localhost MODE #ops +o user3`. `user3` can now execute operator commands.
- From `user3`: Revoke operator from `user3` (self-deop):
  ```text
  MODE #ops -o user3
  ```
  **Success**: Broadcasts `:user3!u3@localhost MODE #ops -o user3`. Subsequent operator actions from `user3` are blocked with `:ircserv 482 user3 #ops :You're not channel operator`.

#### 8. Compound / Mixed MODE Flags & Queries
From **Client 1 (`op_user`)**:
- Set multiple modes at once:
  ```text
  MODE #ops +it
  ```
  **Success**: Broadcasts `:op_user!op@localhost MODE #ops +it`.
- Query channel mode:
  ```text
  MODE #ops
  ```
  **Success**: Server responds with numeric `:ircserv 324 op_user #ops +it`.
- Query user mode:
  ```text
  MODE op_user
  ```
  **Success**: Server responds with numeric `:ircserv 221 op_user +`.
- Combined `+o` and `-o` in a single command:
  ```text
  MODE #ops +o-o user3 op_user
  ```
  **Success**: Broadcasts `:op_user!op@localhost MODE #ops +o-o user3 op_user`. Operator privilege is atomically transferred in one command.

---

### Part F: Valgrind Leak Verification

To verify zero memory leaks across active sessions:
1. Start the server under Valgrind:
   ```bash
   valgrind --leak-check=full --show-leak-kinds=all ./ircserv 6667 mypassword
   ```
2. In a second terminal, connect a client with `nc`, authenticate, create a channel, send messages, and disconnect:
   ```bash
   (printf "PASS mypassword\r\nNICK valgrind_user\r\nUSER val 0 * :Valgrind User\r\nJOIN #leakcheck\r\nPRIVMSG #leakcheck :Testing leaks\r\nPART #leakcheck\r\nQUIT :done\r\n"; sleep 0.5) | nc 127.0.0.1 6667
   ```
3. Stop the server in Terminal 1 using `Ctrl+C` (`SIGINT`).
4. **Success**: Valgrind reports:
   ```text
   ERROR SUMMARY: 0 errors from 0 contexts
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   ```

---

## 7. Known Limitations / Notes

1. **Standalone Architecture**: As required by the 42 subject, `ircserv` operates strictly as an autonomous, single-server system. It does not link with other IRC daemons or form IRC networks.
2. **Server-Side Focus**: This project implements the server engine. Interactive end-user chatting requires an IRC client such as Irssi or raw terminal tools like Netcat.
3. **Bonus Boundaries**: Features categorized as optional bonuses in the subject (DCC file transfer and automated IRC bots) are intentionally omitted from this submission to focus 100% of engineering rigor on the stability, performance, and gate compliance of the mandatory IRC server specification.
