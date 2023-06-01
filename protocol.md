# Description of communications protocol between the Chica Client and the Chica Server.

Last edit: 30th May 2023  
Chica Server version 0.0.4a

This is a summary of the communications protocol between the [MakeYourPet](https://github.com/MakeYourPet) client and server. It is intended to help those wishing to retain the use of the Chica server, but to write their own client. Both the [Chica Server](https://play.google.com/store/apps/details?id=com.makeyourpet.chicaserver&hl=en_US&gl=US) and [Chica Client](https://play.google.com/store/search?q=Chica%20client&c=apps&hl=en_US&gl=US) are available from the Google Android Play Store.

## DISCLAIMER: This document has been written only from knowledge gained by reverse engineering the link, and may not be exhaustive, or indeed correct. Use at your peril.

---

## Protocol

Let’s look first at getting some communication going.

The client talks to the server using fairly standard TCP protocol on port 18711. It appears that the server will accept multiple connections, but will only talk to the first. So if client A connects, then client B connects, and then client A disconnects, the server will not talk to B. If client A reconnects, it will not talk to A. It seems that the Chica server will only talk the first and only connection. This is not problem, because generally, you'll only have the one server and the one client - it's just interesting how it works.

When the client connects to the server, the server will output its’ standard message. This looks something like

```
72 65 61 64 79 3A 42 50 53 3D 20 20 31 7C 56 3D 	|ready:BPS=  1|V=|
2D 2D 2D 7C 49 3D 2D 2D 2D 7C 49 50 3D 31 39 32 	|---|I=---|IP=192|
2E 31 36 38 2E 31 2E 31 30 36 7C 4C 45 47 53 3D 	|.168.1.106|LEGS=|
2D 2D 2D 2D 2D 2D 7C 46 4C 41 47 53 3D 30 30 30 	|------|FLAGS=000|
30 30 30 31 30 30 0A                            	|000100 |
```
All communication appears to be in plain 7 bit ASCII with each packet terminated with a line feed character (0x0A). There is no terminating NUL character, if you are working in ‘C’ or something that likes NUL terminated strings, you are going to have to add one. All strings received from, and sent to the server are terminated with a newline character (0x0A).

At this point all communication will stop, because the server is waiting a reply from your client. It will wait a long time. I have not tested how long it will wait. It may wait forever. Each packet the server sends needs to be acknowledged. This can be a string such as the 4 bytes “ack\n”. So if every time you get a packet from the server you reply with “ack\n” you will get a stream of data from the server.

Now that we have some communication going, let’s look at that data.

`
ready:BPS=  1|V=---|I=---|IP=192.168.1.106|LEGS=------|FLAGS=000000100
`

It is not fixed format. It has to be parsed because stuff moves about, another packet may be e.g.

`
busy:BPS= 123|V= 7.936|I= 2.014|IP=192.168.1.106|LEGS=x-x-x-|FLAGS=110000100
`

If the server responds with a “busy” you MUST ack and read again. There is no ambiguity about that. When the server responds with “ready” you can send the different commands. Not entirely sure what “busy” means in this context. i.e. if you instruct the hexapod to stand, ready appears before all 6 legs are in contact with the ground. So if the hexapod says busy, just ack it and read again.

Most of this stuff in the packet from the server is self explanatory.

BPS is I believe beats per seconds with the Servo2040 board and shows how well it is communicating.

Legs from left to right are L1, L2, L3, R1, R2, R3 so “LEGS=x-x-x-” shows the hexapod standing on 3 legs (2 left 1 right) typical for a tripod gait.

Flags. This is a mix of binary and decimal values, and can be broken down as below.


```
FLAGS=abcdefghi

	a 	=0 if powered off
		=1 if powered up
	b 	=0 if sitting
		=1 if standing
	c	unknown
	d	=0 if not crab mode
		=1 if crab
	e	describes the current mode
		=0 if standard mode
		=1 if race mode
		=2 if off-road mode
		=3 if custom mode
		=4 if quad mode
	f	unknown
	g 	=0 autosit disabled
		=1 if autosit enabled
	h	=0 not in block mode
		=1 if block mode
```
---
# Commands

Remember all these need to be terminated with the linefeed character. They are case sensitive, i.e. all lowercase. If you send a command it doesn’t know or like, then handshaking is likely to stop. You will have to restart your client.

|Command|Description|
|-------|-----------|
|ack|Standard acknowledgement|
|autosit|Sets the autosit flag|
||When set, if the server does not receive a command (other than ack), after 5 secs or so, it will automatically sit and power off the servos. The server seems to be intelligent enough to know that it is auto sitting. sending a command like walk3… will automagically caused the hexapod to power up and stand before walking off.|
|beep|Makes the server beep|
|block|Select block mode. (Tucks the legs in)|
|bounce|Shimmy|
|bye|Hmmm, not sure about this.|
||Only seen it once, and I cannot remember in what context.|
|custom|Select custom mode (see config file)|
|home|Resets leg positions. Clears the leg touch sensors – or attempts to.|
|offroad|Select off road mode (see config file)|
|quad:a,b|Selects the quadruped mode. a & b are the legs to disable. See walking below.|
|race|Select race mode (see config file)|
|setxy|See walking a quadruped below.|
|sit|Stand if sitting, sit if standing. (Check flags)
|standard|Select standard mode. (check config file)
|torque|Power on/off (Check flags)

You cannot send two consecutive commands. i.e. you cannot send “torque” then “sit”. You have to have at least one “ack” in between even if the server responds ready, you need at least 1 ack between every command.

---
# Walking

Walking commands will walk until you stop them.

|Command|Description|
|-------|-----------|
|clear|stops walking
|gait:x,y,0
||gait describes the gait. It can be one of the following
||walk3 = Tripod
||walk2 = Triple
||walk1 = Ripple
||walk25 = Triple 2.5
||walk15 = Ripple 1.5
||walkwave =Wave Gait.
||x and y are greater than -1 but less then 1 i.e. -1 < x & y > -1
||x seems to be to 9 decimal places and affects a turn, y seems to be to 8 decimal places and affects forward/backward motion
||last parameter always seems to be zero
||e.g.
||walk3:0.3,0,0 slowly spins the hexabot to the left
||walk3:-0.3,0,0 slowly spins the hexabot to the right
||walk3:0,0.7,0 moderate speed straight ahead
||walk3:0,-0.99,0 fast straight backwards
||walk3:0.2,0.5,0 curved walk forward leftish.
||
|quad:a,b|where a and b specify the legs to disable specified as single digits0=L1, 1=L2, 2=L3, 3=R1, 4=R2, 5=R3
||So…
||quad:0,3 disables the front two legs
||Then you specify movement with setxy e.g.
|setxy:x,y|it’s very much like the walking commands above, but there is no third parameter e.g.
||setxy:-5,0 slow rotate left
||setxy:0,0.9 fast walk forward


If you need to change the quad mode, you have to switch to “standard” first. This is so the server can re-arrange it’s legs.

---

Anyway, if you are writing your own client, you will probably only want to use a subset of these commands. I suggest torque, sit, walk3 and clear.

Also in this repository there is a simple client program called ctrl.c. It uses a thread to communicate with the server. It reads commands from standard input so you can either type them in on a keyboard or stream a test file like …

```
$ cat test1.txt | ctrl -v 192.168.1.106
```
Build it with the Makefile or simply using the commands …

```c
gcc -Wall -g -O3 -c ctrl.c
gcc -Wall -o ctrl ctrl*.o  -pthread -lm -lc 
```
Good luck.
