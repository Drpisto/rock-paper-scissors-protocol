# RPSP - Rock Paper Scissors Protocol

a custom network protocol where you have to win rock paper scissors before sending data. built on top of raw IP (proto 253), no TCP no UDP.

i made this for fun, don't use it for anything serious lol

## how it works

before any data transfer, both sides play RPS. winner sends, loser waits. if you draw 5 times the connection just dies (DEADLOCK).

```
HELLO → ACCEPT → play RPS → winner sends DATA → FIN
```

## packet

```
[ 0x52 | type | move+round | session_id (2b) | checksum (2b) ]
         7 bytes total
```

move is packed in the top 2 bits of byte 2, round in the bottom 6.

## build

```bash
gcc server.c socket.c packet_crafter.c logic.c -o server
gcc client.c socket.c packet_crafter.c logic.c -o client

sudo ./server
sudo ./client <ip>
```

needs root for raw sockets.

## files

```
rps.h            types and structs
socket.c         send/recv over raw IP
packet_crafter.c build packets
logic.c          rps logic
server.c         server
client.c         client
```

shows up as `Unknown (253)` in wireshark. there's a lua dissector if you want it to look nicer.
# Still under development
