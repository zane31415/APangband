#ifndef APINTERFACE_H
#define APINTERFACE_H

char SetAPSettings();

void ConnectAP();
void DisconnectAP();

void WriteAPState();
void ReadAPState();

void PollServer();

char isDeathLink();
char SendDeathLink();
char AnnounceAPVictory(char isFullVictory);

#endif