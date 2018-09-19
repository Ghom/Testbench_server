#ifndef SERIAL_H
#define SERIAL_H

int SetInterfaceAttribs(int fd, int speed, int parity, int waitTime);
void start(void);

#endif //SERIAL_H