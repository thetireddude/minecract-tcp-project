# Minecraft TCP Proxy in C++

*A hands-on systems and networking project guide for your Xubuntu VM, VS Code, and existing Minecraft server setup.*

> **Project goal:** Start from the Minecraft server setup you have already used, insert your own C++ TCP proxy in front of it, then optionally work toward replacing the playit.gg tunneling layer with your own relay/tunnel system.

## 1. Starting Point: Your Existing Minecraft Setup

You have already hosted a Minecraft Java server locally using a `start.sh` script that launches `server.jar`, and you used **playit.gg** to expose that local server so friends on the internet could connect without directly reaching your private machine.

```text
Friends online
     |
     v
playit.gg public endpoint
     |
     v
playit.gg tunnel
     |
     v
Your machine
     |
     v
start.sh -> server.jar
     |
     v
Minecraft server on localhost:25565
```

This project builds directly on that experience. Instead of treating networking as a black box, you will progressively implement the layer between the Minecraft client and server yourself.

## 2. What You Are Building First

Your first version is a **local TCP proxy**. Keep your existing `start.sh` and `server.jar` setup unchanged. Your C++ program will listen on a different local port and forward traffic to the real Minecraft server.

```text
Minecraft client
connects to localhost:25566
        |
        v
Your C++ TCP proxy
listens on :25566
        |
        v
Existing Minecraft server
running through start.sh / server.jar
on localhost:25565
```

From Minecraft's perspective, your proxy is the server. From `server.jar`'s perspective, your proxy is a client. Your program therefore has to implement both sides of a TCP connection.

## 3. What This Project Demonstrates

- C++ systems programming on Linux
- TCP/IP and socket programming
- Client/server architecture and connection lifecycles
- Bidirectional I/O and concurrency
- Threads and synchronization, with an optional progression to `select` / `poll` / `epoll`
- Binary protocol parsing for the Minecraft Java protocol
- Resource cleanup, graceful shutdown, logging, and testing
- Optional deeper networking work: NAT, public relays, persistent outbound tunnels, and multiplexing

## 4. Environment Setup

Recommended environment: your **Xubuntu VirtualBox VM**, VS Code, `g++`, Git, and your existing Minecraft Java server directory.

### Install development tools

```bash
sudo apt update
sudo apt install build-essential gdb git cmake
```

### Create the proxy project

```bash
mkdir minecraft-tcp-proxy
cd minecraft-tcp-proxy
code .
```

You do **not** need the Code::Blocks "Console Application" template from the tutorial. In VS Code, a folder containing `main.cpp` is enough.

### Keep your Minecraft server separate

```text
minecraft-server/
  start.sh
  server.jar
  server.properties
  ...

minecraft-tcp-proxy/
  src/
  README.md
  ...
```

This lets you start the real server exactly as you did before, then start your proxy as a second process.

### Initialize Git

```bash
git init
git add .
git commit -m "Initialize C++ Minecraft TCP proxy"
```

## 5. Suggested Project Structure

```text
minecraft-tcp-proxy/
├── README.md
├── CMakeLists.txt            # add after the basic prototype works
├── src/
│   ├── main.cpp
│   ├── TcpListener.cpp
│   ├── TcpListener.h
│   ├── TcpClient.cpp
│   ├── TcpClient.h
│   ├── ProxySession.cpp
│   ├── ProxySession.h
│   ├── MinecraftProtocol.cpp
│   └── MinecraftProtocol.h
└── tests/
    └── protocol_tests.cpp
```

## 6. Build Plan

### Milestone 0 - Learn the socket primitives

Follow the basic Linux C++ TCP server and TCP client tutorials. The goal is to understand the primitives your proxy will combine.

**Implementation checklist**
- Write a tiny TCP echo server.
- Write a tiny TCP client that connects to it.
- Practice `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `recv()`, `send()`, and `close()`.
- Compile and run both inside Xubuntu.

**Done when**
- You can explain what a socket is.
- You can explain the difference between a listening socket and a connected socket.
- You understand why a server binds to a port while a client connects to one.

### Milestone 1 - Transparent local Minecraft proxy

Start your normal Minecraft server with `start.sh` so `server.jar` listens on port `25565`. Then make your C++ program listen on `25566` and establish an upstream connection to `25565` for every accepted client.

**Implementation checklist**
- Run `./start.sh` in your Minecraft server directory.
- Confirm the real server is listening on `localhost:25565`.
- Run your proxy and listen on `localhost:25566`.
- Configure Minecraft to connect to `localhost:25566`.
- Accept the client socket, connect to `localhost:25565`, and relay raw bytes.

**Done when**
- Minecraft can join the existing world through `localhost:25566`.
- Your proxy logs accepted connection, upstream connection, and disconnect events.
- No Minecraft-specific parsing is required yet.

### Milestone 2 - Correct bidirectional forwarding

Minecraft traffic flows both ways at arbitrary times. Make the client-to-server and server-to-client directions progress independently.

**Implementation checklist**
- Implement `forwardLoop(sourceFd, destinationFd)`.
- Run one worker for client -> server and another for server -> client.
- Treat `recv() == 0` as a clean disconnect.
- Implement `sendAll()` because `send()` may write fewer bytes than requested.
- Close both sockets when the session ends.

**Done when**
- The client can join, move around, load chunks, chat, and disconnect without the proxy hanging.
- You can explain blocking I/O and why the two directions cannot depend on one another.

### Milestone 3 - Multiple player sessions

Allow multiple clients to use the proxy at the same time. Each accepted client gets its own upstream socket and its own proxy session.

**Implementation checklist**
- Create a `ProxySession` abstraction for one client/upstream pair.
- Give each session independent forwarding workers.
- Track active connection count.
- Test repeated joins/leaves and, if possible, two simultaneous Minecraft clients.

**Done when**
- New connections do not block existing ones.
- The process survives repeated connections.
- File descriptors and threads are cleaned up when sessions end.

### Milestone 4 - Minecraft-aware handshake parsing

Once transparent forwarding is stable, decode selected Minecraft packet framing instead of treating every byte as opaque. Start with the initial handshake only.

**Implementation checklist**
- Implement VarInt decoding and encoding.
- Read the packet length and packet ID.
- Parse protocol version, requested host, requested port, and next state.
- Log the decoded fields.
- Forward the original bytes unchanged after inspection.

**Done when**
- The proxy correctly recognizes a handshake without breaking the connection.
- Malformed or partial input fails safely.
- You can explain why TCP is a byte stream and why application-level framing is necessary.

### Milestone 5 - Status, ping, and connection metrics

Add observability without trying to implement the full Minecraft protocol.

**Implementation checklist**
- Recognize status-state traffic.
- Measure connection duration and bytes in/out.
- Record basic timing around status/ping traffic.
- Print useful per-session summaries when clients disconnect.

### Milestone 6 - Production-style polish

- Add `--listen-port`, `--upstream-host`, and `--upstream-port` arguments.
- Add structured logs.
- Handle `SIGINT` for graceful shutdown.
- Add CMake.
- Add unit tests for VarInt and configuration parsing.
- Write a README with architecture, screenshots/log samples, and run instructions.

## 7. Core Networking Logic to Understand

### Listening side: your proxy acts like a server

```cpp
int listenFd = socket(AF_INET, SOCK_STREAM, 0);
bind(listenFd, ...);
listen(listenFd, SOMAXCONN);
int clientFd = accept(listenFd, ...);
```

### Upstream side: your proxy acts like a client

```cpp
int upstreamFd = socket(AF_INET, SOCK_STREAM, 0);
connect(upstreamFd, ...);
```

### Forwarding loop

```cpp
while (true) {
    ssize_t n = recv(sourceFd, buffer, sizeof(buffer), 0);
    if (n <= 0) break;
    sendAll(destinationFd, buffer, n);
}
```

**Important:** TCP does not preserve application message boundaries. One `recv()` call may contain part of a Minecraft packet, exactly one packet, or multiple packets. Transparent forwarding is simpler because you can relay whatever bytes arrive; protocol parsing requires buffering and framing.

## 8. How to Run the Project with Your Existing Server

1. Open one terminal in your existing Minecraft server directory.
2. Run `./start.sh` exactly as you did before and wait for `server.jar` to finish starting.
3. Open a second terminal in `minecraft-tcp-proxy/`.
4. Build and run the proxy so it listens on `25566` and forwards to `localhost:25565`.
5. In Minecraft, create or edit a server entry whose address is `localhost:25566`.
6. Join the server and watch the proxy logs while you move, chat, and disconnect.

```text
Terminal 1:  ./start.sh
Terminal 2:  ./minecraft-proxy --listen-port 25566 --upstream-host 127.0.0.1 --upstream-port 25565
Minecraft:   localhost:25566
```

## 9. Testing Strategy

| Test | What to verify |
|---|---|
| Smoke test | Can Minecraft connect through `localhost:25566`? |
| Gameplay traffic | Move around, load chunks, chat, and interact. The proxy should remain stable. |
| Disconnect/reconnect | Join, leave, and reconnect several times. The proxy should stay alive. |
| Server-down | Run the proxy while `server.jar` is offline. Report the upstream failure cleanly. |
| Multiple sessions | Use more than one client if available, or repeat sessions aggressively. |
| Parser unit tests | Feed known VarInt and handshake byte sequences into parsing functions without Minecraft running. |
| Resource test | Use `lsof` or `/proc/<pid>/fd` to make sure descriptors do not accumulate after disconnects. |

## 10. Git Commit Checkpoints

1. Initialize project and basic build
2. Add TCP listener and accept one client
3. Add upstream connection to `localhost:25565`
4. Forward client traffic to the Minecraft server
5. Add bidirectional relay
6. Handle disconnects and cleanup
7. Add concurrent proxy sessions
8. Add Minecraft VarInt parser
9. Parse Minecraft handshake
10. Add connection metrics and logging
11. Add graceful shutdown and configuration
12. Add tests and README

## 11. How This Relates to playit.gg

Your local proxy is **not yet a replacement for playit.gg**. It solves a different first problem: learning how a TCP intermediary accepts one connection, creates another, and forwards traffic safely.

| Layer | What it does |
|---|---|
| Existing server | `server.jar` provides the Minecraft application/server logic on port `25565`. |
| Your C++ proxy | Sits locally in front of `server.jar` and teaches sockets, forwarding, concurrency, and protocol parsing. |
| playit.gg | Provides the public internet-facing relay/tunnel that lets friends reach a machine behind NAT/firewall constraints. |
| Stretch goal | Implement your own simple public relay + outbound local tunnel so you understand the abstraction playit.gg previously provided. |

### Why playit.gg was useful

A home machine usually has a private address and may sit behind NAT. Accepting arbitrary inbound internet connections can require router configuration, port forwarding, firewall changes, or a public address. A tunneling service can avoid that by having your local machine establish an **outbound connection** to a publicly reachable relay, then forwarding remote client traffic through that established path.

## 12. Stretch Goal: Build a Simple Minecraft Tunnel

Only attempt this after the local proxy is reliable. The goal is to reproduce the core shape of the setup you previously achieved with playit.gg, not to match a production tunneling service feature-for-feature.

```text
Friend on the internet
        |
        v
Public cloud VM / relay
        |
        | persistent tunnel
        v
Your local tunnel client
        |
        v
Your C++ Minecraft proxy
        |
        v
start.sh -> server.jar :25565
```

Possible steps:
- Run a relay process on a cloud VM with a public IP and a public Minecraft-facing port.
- Run a local tunnel client on your machine that makes an outbound persistent TCP connection to the relay.
- When a friend connects to the public relay, associate that connection with a tunnel stream back to your local machine.
- Forward the stream to your local proxy/server.
- Add reconnect behavior, heartbeats, simple authentication, and connection IDs.
- As an advanced extension, multiplex several Minecraft sessions over one persistent tunnel connection.

This stretch goal gives you a reason to study **NAT, public vs. private IP addressing, inbound vs. outbound connections, relays, persistent connections, reconnection logic, and multiplexing**.

## 13. What Not to Do

- Do not begin by implementing the entire Minecraft protocol. Get transparent byte forwarding working first.
- Do not try to replace playit.gg on day one. The public relay/tunnel is a stretch goal after the local proxy.
- Do not hide the networking behind a large framework. The point is to work directly with Linux sockets and I/O.
- Do not jump straight to `epoll` before you understand blocking sockets and a thread-based solution.
- Do not spend most of the project on UI. A CLI with good logs and documentation is enough.
- Do not copy a finished Minecraft proxy repository line-for-line. Use references for protocol details, then implement your own design.

## 14. Suggested Timeline

| Phase | Target | Output |
|---|---|---|
| Days 1-2 | Sockets refresher | Echo server + TCP client |
| Days 3-4 | Transparent proxy | Minecraft joins your existing server through `:25566` |
| Days 5-6 | Concurrency | Stable bidirectional forwarding + multiple sessions |
| Days 7-9 | Protocol parsing | VarInt + Minecraft handshake parsing |
| Days 10-12 | Polish | Metrics, graceful shutdown, CMake, tests, README |
| Optional | Tunnel stretch goal | Public relay + local outbound tunnel client |

## 15. Interview Story

> "I had previously hosted a Minecraft server locally using a shell script and `server.jar`, then used playit.gg to tunnel it so friends could connect over the internet. I wanted to understand what was happening below that abstraction, so I built a C++ TCP proxy in front of the Minecraft server using Linux sockets. I added bidirectional forwarding, concurrent sessions, connection metrics, and Minecraft handshake parsing. As a stretch goal, I explored the public-relay/outbound-tunnel architecture that services like playit.gg make easy for users."

## 16. Definition of a Strong V1

- Runs on Linux from the command line.
- Works with your existing `start.sh` / `server.jar` Minecraft server without modifying it.
- Listens on a configurable local port such as `25566`.
- Connects to a configurable upstream such as `localhost:25565`.
- Relays traffic in both directions without corrupting it.
- Supports multiple sessions.
- Logs connection lifecycle events and byte counts.
- Cleans up sockets and threads correctly.
- Includes a clear README with architecture and run instructions.

> **Build the transparent local proxy first. Minecraft protocol parsing is the next layer. Replacing the playit.gg-style tunnel is an optional third layer.**
