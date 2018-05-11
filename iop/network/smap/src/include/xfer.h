#ifndef XFER_H
#define XFER_H


#include "main.h"


int smap_transmit(void *buf, size_t size);
int HandleRxIntr(struct SmapDriverData *SmapDrivPrivData);
int HandleTxReqs(struct SmapDriverData *SmapDrivPrivData);


#endif
