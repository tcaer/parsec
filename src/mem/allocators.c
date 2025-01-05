#include "parsec.h"

// MARK Arena impls

Allocator *Arena_create(char *memory, size_t capacity) {
  BumpArena *self = malloc(sizeof(BumpArena));
  self->capacity = capacity;
  self->offset = 0;
  self->memory = memory;

  Allocator *arena = malloc(sizeof(Allocator));
  arena->arena = self;
  arena->release = Arena_release;
  arena->alloc = Arena_alloc;
  arena->free = Arena_free;

  return arena;
}

void Arena_release(Allocator *arena) {
  free(arena->arena);
  free(arena);
}

void *Arena_alloc(void *_self, size_t size) {
  BumpArena *self = _self;

  assert(self->offset + size < self->capacity);

  void *alloced = self->memory + self->offset;
  self->offset += size;
  return alloced;
}

void Arena_free(void *_self, void *ptr) {
  // noop
}
