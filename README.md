## CMIIDI Kernel Driver & Linux applemidi

CMIDID (Configurable MIDI Driver) is an experimental Linux kernel driver which 
can be used to build custom lowcost MIDI keyboards with breadboards, jumpwires
and buttons.

This repositoy includes another kernel driver which implements the RTP MIDI
(aka AppleMIDI) realtime protocol.

It's possible to use several Linux machines with CMIDID drivers to send
MIDI events over the network and combine the events via AppleMIDI into a
single MIDI output.

