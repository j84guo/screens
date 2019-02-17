#include "ringbuffer.h"

#include <stdio.h>
#include <string.h>

static size_t min(size_t a, size_t b)
{
  return a < b ? a : b;
}

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

RingBuffer::~RingBuffer()
{
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
    size_t blockLen = min(len - i, _capacity - _end);
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
  size_t toRead = min(len, _size);

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
