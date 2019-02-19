#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <sys/types.h>


/* A circular binary queue which allows overwriting the oldest bytes */
class RingBuffer {
public:
  RingBuffer();
  RingBuffer(size_t capacity);
  RingBuffer(const RingBuffer &other);
  RingBuffer& operator=(RingBuffer other);
  RingBuffer(RingBuffer &&other);
  ~RingBuffer();

  size_t size();
  size_t capacity();
  void write(char *from, size_t len);
  size_t read(char *into, size_t len);

private:
  void swapWith(RingBuffer &other);

  /* _start indicates where to read from next
     _end indicates where to write to next */
  size_t _start;
  size_t _end;
  size_t _size;
  size_t _capacity;
  char *_buffer;
};

#endif
