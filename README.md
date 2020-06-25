# SourceTV Support

Project Build Status
------
Project | Build Status
------------ | -------------
SourceTV Support | [![Build Status](https://github.com/shqke/sourcetvsupport/workflows/Build%20&%20Deploy/badge.svg?branch=master)](https://github.com/shqke/sourcetvsupport/actions/)

Introduction
------
Set of plugins that help to fix broken broadcasting/demo recording features.

More information on wiki: https://developer.valvesoftware.com/wiki/SourceTV

Requirements
------
- [MM:Source](https://www.sourcemm.net/)
- [SourceMod (1.8+)](https://www.sourcemod.net/)

Supported Games
------
- ~~[Left 4 Dead 1](https://store.steampowered.com/app/500/Left_4_Dead/)~~
- [Left 4 Dead 2](https://store.steampowered.com/app/550/Left_4_Dead_2/)

Installation
------
1. Get [latest sourcetvsupport release](https://github.com/shqke/sourcetvsupport/actions) for your OS (linux or windows)
2. Extract the zip file into your server's mod folder
3. Add `-hltv` and/or `+tv_enable 1` to srcds start parameters
4. Set SourceTV related cvars into server config

Command Line Parameters
------
- `+tv_enable 1` - Make SourceTV available on first map start (don't require to `changelevel`/`map`)
- `-hltv` - Enable broadcasting feature (clients can connect to a port specified in `tv_port` to spectate the game)
- `+tv_port <port>` - Set the SourceTV host port (default 27020). Must be set as srcds start parameter
- `-tvmasteronly` - SourceTV can only serve one client and can't be used as relay proxy

Fixed Issues
------
- `CHLTVServer::stringTableCRC` was never set to value of `CGameServer::stringTableCRC`
- Insufficient buffer size to store string tables data
- Cvar `sv_hibernate_when_empty` would cause a consistent memory leak
- Cvar `tv_enable` and some others were not available from command line
- HLTV clients are no longer forced to join lobbies
- Server fails to initiate due to bound HLTV port being passed as query port, freezes
- Server crash on HLTV client disconnect (auth ticket method)
- Event `player_full_connect` will never be sent for HLTV clients
- `ISteamGameServer::LogOff` no longer called whenever SourceTV shuts down
- SourceTV bot now is being forced to change his team to spectators

Known Issues
------
- Missing arms models/blinking world model attachments during SourceTV footage playback
  - Needed modifications on game client
- Some models are getting stuck during playback (helicopter on c8m1 after intro)
