#ifndef APPLEMIDI_H
#define APPLEMIDI_H

#define APPLEMIDI_PROTOCOL_SIGNATURE          0xffff

#define APPLEMIDI_COMMAND_INVITATION          0x494e /** "IN" on control & rtp port */
#define APPLEMIDI_COMMAND_INVITATION_REJECTED 0x4e4f /** "NO" on control & rtp port */
#define APPLEMIDI_COMMAND_INVITATION_ACCEPTED 0x4f4b /** "OK" on control & rtp port */
#define APPLEMIDI_COMMAND_ENDSESSION          0x4259 /** "BY" on control port */
#define APPLEMIDI_COMMAND_SYNCHRONIZATION     0x434b /** "CK" on rtp port */
#define APPLEMIDI_COMMAND_RECEIVER_FEEDBACK   0x5253 /** "RS" on control port */

#endif