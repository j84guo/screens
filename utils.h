#ifndef UTILS_H
#define UTILS_H

#include <string>


std::string strError(int err);
void sysError(const std::string &name);

int maxFds();
bool daemonizeStddes(std::string path="");
bool resetStddes(int fd);

void unsetTerminalRawIO();
bool setTerminalRawio();

int makePTY();

#endif
