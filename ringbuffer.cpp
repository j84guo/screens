#include "ringbuffer.h"

#include <algorithm>

#include <stdio.h>
#include <string.h>


RingBuffer::RingBuffer():
    RingBuffer(512)
{}

RingBuffer::RingBuffer(size_t capacity):
  _capacity(capacity),
  _size(0),
  _start(0),
  _end(0)
{
  _buffer = new char[_capacity];
}

RingBuffer::RingBuffer(const RingBuffer &other):
  _capacity(other._capacity),
  _size(other._size),
  _start(other._start),
  _end(other._end)
{
  _buffer = new char[_capacity];
  memcpy(_buffer, other._buffer, _capacity);
}

/* Copy and swap ensures construction of new state occurs before destruction of
   current state */
RingBuffer &RingBuffer::operator=(RingBuffer other)
{
  swapWith(other);
  return *this;
}

void RingBuffer::swapWith(RingBuffer &other)
{
  std::swap(_buffer, other._buffer);
  std::swap(_capacity, other._capacity);
  std::swap(_size, other._size);
  std::swap(_start, other._start);
  std::swap(_end, other._end);
}

/* Move construction makes insertion into vector more efficient */
RingBuffer::RingBuffer(RingBuffer &&other)
{
  printf("Move constructing RingBuffer\r\n");
  _buffer = other._buffer;
  _capacity = other._capacity;
  _size = other._size;
  _start = other._start;
  _end = other._end;

  other._buffer = nullptr;
}

RingBuffer::~RingBuffer()
{
  /* like free(), delete is a NO-OP on null pointers */
  delete[] _buffer;
}

size_t RingBuffer::size()
{
  return _size;
}

size_t RingBuffer::capacity()
{
  return _capacity;
}

/* If we overwrite the previous _start, the  _start simply follows the new
   _end */
void RingBuffer::write(char *from, size_t len)
{
  bool overflow = len > (_capacity - _size);
  size_t i = 0;

  while (i < len) {
    size_t blockLen = std::min(len - i, _capacity - _end);
    memcpy(_buffer + _end, from + i, blockLen);

    _end += blockLen;
    if (_end == _capacity) {
      _end = 0;
    }
    i += blockLen;
  }

  if (overflow) {
    _start = _end;
    _size = _capacity;
  } else {
    _size += len;
  }
}

size_t RingBuffer::read(char *into, size_t len)
{
  size_t toRead = std::min(len, _size);
  if (!toRead) {
    return 0;
  }

  if (_end > _start) {
    memcpy(into, _buffer + _start, toRead);
    _start += toRead;
  } else {
    size_t firstLen = _capacity - _start;
    memcpy(into, _buffer + _start, firstLen);

    size_t secondLen = _end;
    memcpy(into + firstLen, _buffer, secondLen);
    _start = secondLen;
  }

  _size -= toRead;
  return toRead;
}

void demoRingBuffer()
{
  RingBuffer ring(5);

  char inbuf[] = {1, 2, 3, 4};
  ring.write(inbuf, 4);
  ring.write(inbuf, 4);

  char outbuf[5];
  size_t res = ring.read(outbuf, ring.size());
  for (size_t i=0; i<res; ++i) {
    printf("%hhd\n", outbuf[i]);
  }
}

/*
int main()
{
  RingBuffer buf(1024);
  RingBuffer copy = buf;
  RingBuffer other(512);
  other = copy;
}
*/
