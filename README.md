## CMIIDI Kernel Driver & Linux applemidi

CMIDID (Configurable MIDI Driver) is an experimental Linux kernel driver which 
can be used to build custom lowcost MIDI keyboards with breadboards, jumpwires
and buttons.

This repositoy includes another kernel driver which implements the RTP MIDI
(aka AppleMIDI) realtime protocol.

It's possible to use several Linux machines with CMIDID drivers to send
MIDI events over the network and combine the events via AppleMIDI into a
single MIDI output.

###DISCLAIMER

We are not responsible for any damage to your system. Neither software nor hardware.

Use at your own responsibility 

## Preparing your Raspberry 

In order to build kernel modules on your raspberry pi, running the current version of raspbian, it is necessary to install the kernel sources as they are not shipped in a convenient debian package.

To facilitate the process of determining the according commit hash of your build a little script exist.

Just follow the tutorial at https://github.com/notro/rpi-source/wiki

## AppleMIDI

The applemidi driver is able to create a bridge between an alsa sequencer port and any applemidi compatible network client.

It is ported from a project called midikit (https://code.google.com/p/midikit/) from Jonas Pommerening under BSD-3 license.

The logical location within the kernel can be described as follows:

Alsa sequencer -> Applemidi -> Network Stack -> any compatible client

The Protocol is based on RTPMIDI specified in ~~RFC 4695~~ RFC 6295 (http://tools.ietf.org/html/rfc6295) with a proprietary extension from Apple.

The extension includes the otherwise separately performed session management functions directly into the protocol and is simple to implement. This allows a compatibility of a huge group of implementations:
*	CoreMIDI (Apple)
*	nmj (Java Android)
*	Arduino AppleMidi
*	rtpMIDI (Windows)
*	...

### Installation
The installation of the kernel module is straight forward.

After cloning the repository

	git clone https://github.com/jtee/cmidid-kerneldriver.git .

to your raspberry pi, you have to build the module.

	cd applemidi
	make
	
The build process should run without any warnings.

If this was successful you have the binary module `applertp.ko`

#### 64 bit divmod

This module is intended to be used on a raspberry pi. As there exists no 64 bit division or multiplication on the ARM, this has to be emulated in a software trap.
This is included by the `_uldivmod.S` assembly code.

If you want to use this module on an architecture where the `__aeabi_uldivmod` symbol is available you have to remove the `_uldivmod.o` target from `applertp-objs` in the Makefile.

### Usage

To use the module you have to load it with the desired port for the control channel. This is required to be done as root

	sudo insmod applertp.ko port=5008

__Remember__: The control port and the next higher have to be free, otherwise the module will fail to listen on these ports.

If you omit the port parameter, it will listen on 5008 and 5009 by default.

You should use an even port number to avoid any incompatibilities.

After loading the module it will register a new alsa sequencer card and a client with capabilities of accepting midi events.

You can verify this by listing the possible output ports of alsa with

	aconnect -o
	 
where there should be one entry like

	Client ##: 'AppleMidi' [Typ=Kernel]
    	0 'AppleMidi Port-0'

Now you can route any midi input channel to this by

	aconnect source:port applemidi:0

replacing source:port with the client number and port (most probably 0) and applemidi with the client number of the AppleMidi output.

### Clients

Your system is now also listening on two udp ports.

On any supporting other system you can now connect to this service by its hostname or IP and the port you have assigned.

After connecting, the midi events will be received over the network and can be further processed.

If you connect multiple clients to one server, all of them will receive the midi events.


### Unload

If you are ready and no longer need the service you can shut it down with

	sudo rmmod applertp
	
Therefore you must not have any aconnect clients connected. You can flush all connections easily by

	aconnect -x

### Remarks

####Wifi

__DON'T USE WIFI__
because the CDMA/CA algorithm will not deliver the network packets as deterministic as a fully switched ethernet, where ideally no collisions happen.

To test this, you can run the small program in

	cmidid-kerneldriver / playground / userspace_midi / equi.c
 
compiled with `gcc -std=gnu99 -lasound -o equi equi.c` (you may need the libasound2-dev package) which sends equidistant notes. Connect this in aconnect to the applemidi driver and listen over Wifi. Then you can hear how equidistant these notes are received.

#### IPv6

The server is not capable of handling IPv6 addresses.

#### Journal

_This feature is not yet implemented._

As UDP does not acknowledge Packets, there can be undetected loss. The option to use TCP instead was not chosen because of the overall overhead, which leads to delays.
To compensate for this, the rtpMIDI standard specifies a journal which logs all sent events for every client. The AppleMIDI session management then periodically sends feedback packets to acknowledge the reception of packets up to some sequence number. All earlier journal logs can then be discarded.


## Building an Instrument

###Kernel Modules

If you sacrifice a raspberry to your fancy new instrument, you probably do not want to set everything up after each boot.
It is possible to put a compiled version of the kernel modules in the
	
	/lib/modules/`uname -r`/build/

directory so it can be found by `modprobe` and be loaded at boot time. The configuration for this depends on you distribution (most probably `/etc/modules` or `/etc/modprobe.d`).


###Hardware

For building an actual hardware instrument it is necessary to dive a tiny little bit into electronics. This is also means, that you can __destroy__ real hardware (raspberry pi) if you screw things up.

The buttons are best attached by a pull-up circuit:

![Pull up circuit](/documentation/circuits/pull-up.png)

Be careful! Use the 3.3V pin and not the 5V, otherwise you would break your GPIO.






