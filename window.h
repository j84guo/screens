#ifndef WINDOW_H
#define WINDOW_H

#include "ringbuffer.h"
#include "utils.h"

#include <utility>

#include <unistd.h>
#include <sys/types.h>


/* A list of Windows is maintained by the server

   PTY: only master FD needed
   Circular buffer: last N bytes written to stdout/stderr
   WID: window ID displayed to the user
   PID: process ID, used by server to detect exited children on any SIGCHLD
        (although assuming no unexpected termination child exit can be
         determined by reading EOF from its fdm) */
struct Window {
  Window(int WID, size_t capacity):
    buffer(capacity),
    WID(WID),
    PID(-1)
  {
    if ((fdm = makePTY()) == -1) {
      sysError("makePTY");
    }
  }

  Window(const Window &other) = delete;
  Window &operator=(Window other) = delete;

  Window(Window &&other):
    fdm(other.fdm),
    WID(),
    PID(),
    buffer(std::move(other.buffer))
  {
    other.fdm = -1;
  }

  ~Window()
  {
    if (fdm != -1) {
      close(fdm);
    }
  }

  int fdm;
  int WID;
  pid_t PID;
  RingBuffer buffer;
};

#endif
