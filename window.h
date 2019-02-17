#ifndef WINDOW_H
#define WINDOW_H

#include "ringbuffer.h"
#include <sys/types.h>


/* A list of Windows is maintained by the server
   PTY: only master FD needed
   Circular buffer: last N bytes written to stdout/stderr
   Number: window ID */
struct Window {
  Window(size_t bufCapacity): ringbuf(bufCapacity) {}

  int fdm;
  int ID;
  RingBuffer ringbuf;
};

#endif
