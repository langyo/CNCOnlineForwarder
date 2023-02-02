# Project Archived
Check out https://ra3battle.net and this project: https://github.com/RA3BattleNet/Relay

---

# C&C:Online Forwarder
[![Build Status](https://github.com/lanyizi/CNCOnlineForwarder/workflows/build-cnconline-forwarder-x/badge.svg)](https://github.com/lanyizi/CNCOnlineForwarder/actions)

An unofficial proxy server for [C&amp;C:Online](https://cnc-online.net).

May help RA3 Players with connection issues, especially those caused by a [symmetric NAT](https://en.wikipedia.org/wiki/Network_address_translation) router.

**Note:** This proxy server cannot be used replace C&C:Online. You must already have your game started using [C&C:Online Client](https://cnc-online.net/download/) in order to use it.

Current features:
- [x] NatNeg Server Proxy: Help players to connect to each other by establishing relays between players.

Planned features:
- [ ] A client program which injects DLL into Red Alert 3 to enable features of CNCOnlineForwarder
- [ ] Peerchat Proxy: avoid TCP 6667 port's issues
- [ ] Local HTTP server: avoid the problem of _"Failed to connect to servers. Please check to make sure you have an active connection to the Internet"_ during log in of C&C:Online caused by high latency between http.server.cnc-online.net and player's computer.

## How to run this server
[Prebuilt binaries](https://nightly.link/lanyizi/CNCOnlineForwarder/workflows/action.yaml/master) can be downloaded from [Github actions](https://github.com/lanyizi/CNCOnlineForwarder/actions). To run the server, make sure to allow this program in your Firewall Settings, since it will need to receive inbound UDP packets before sending them out. 

On Windows, normally we can just use the prebuilt binaries.
But you can also build it by yourself. You need to have Boost installed (at least 1.74), then just use CMake to build the project.

Before I could write the client part of this project, you'll have to edit your `hosts` file in order to let the game use your proxy server.
It should look like this:

`[Your proxy server's IP address] natneg.server.cnc-online.net`
