# SourceTV Support

Project Build Status
------
Project | Build Status
------------ | -------------
SourceTV Support | [![Build Status](https://github.com/shqke/sourcetvsupport/workflows/Build%20&%20Deploy/badge.svg?branch=master)](https://github.com/shqke/sourcetvsupport/actions/)

Introduction
------
Set of plugins that help to fix broken broadcasting/demo recording features.

More information about SourceTV on wiki: https://developer.valvesoftware.com/wiki/SourceTV

Requirements
------
- [MM:Source](https://www.sourcemm.net/)
- [SourceMod (1.8+)](https://www.sourcemod.net/)

Supported Games
------
- [Left 4 Dead 1](https://store.steampowered.com/app/500/Left_4_Dead/)
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
- Property **CHLTVServer::stringTableCRC** was never set
- Insufficient buffer size to store string tables and/or data tables
- Hibernation was causing a consistent memory leak
- Cvar `tv_enable` and some others were not available from command line
- HLTV clients were forced to interact with lobby system
- (L4D2) **ISteamGameServer** interface was failing to initiate due to bound HLTV port being passed as query port
- Server would crash on HLTV client disconnect (auth ticket method)
- (L4D2) Event `player_full_connect` was sent for HLTV clients userids
- (L4D2) **ISteamGameServer** interface would log off whenever **CHLTVServer** was shutting down
- SourceTV bot was never moved into Spectator team
- (L4D) Server is now always respects value of cvar `sv_pausable`

Known Issues
------
- Missing arms models/blinking world model attachments during SourceTV footage playback
  - Needed modifications on game client
- Some models are getting stuck during playback (helicopter on c8m1 after intro)
- Relay feature is broken
