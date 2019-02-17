#ifndef UTILS_H
#define UTILS_H

#include <string>


std::string str_error(int err);
void sys_error(const std::string &name);

int max_fds();
bool daemonize_stddes(std::string path="");
bool reset_stddes(int fd);

void unset_terminal_rawio();
int set_terminal_rawio();

#endif
