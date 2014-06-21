#ifndef ALSA_H
#define ALSA_H

struct ALSADriver;

struct ALSADriver * ALSARegisterClient(void);

void ALSADeleteClient(struct ALSADriver *);

#endif