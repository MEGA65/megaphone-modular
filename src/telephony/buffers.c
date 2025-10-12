#include "includes.h"
#include "records.h"
#include "buffers.h"

struct shared_buffers buffers;

unsigned char buffers_lock(unsigned char module)
{
  if (buffers.lock && buffers.lock != module) return 99;
  buffers.lock = module;
  return 0;
}

unsigned char buffers_unlock(unsigned char module)
{
  if (buffers.lock!=module) return 98;
  buffers.lock = LOCK_FREE;
  return 0;
}
