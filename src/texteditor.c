#include "parsec.h"

// MARK defines

#define GAP_SIZE 20

// MARK GapBuffer impls

void GapBuffer_init(GapBuffer *self) {
  self->buffer = calloc(GAP_SIZE, sizeof(char));
  self->gap_start = self->buffer;
  self->gap_end = self->buffer + GAP_SIZE;
  self->buffer_end = self->gap_end;
}

void GapBuffer_destroy(GapBuffer *self) { free(self->buffer); }

static inline size_t GapBuffer_full_length(GapBuffer *self) {
  return self->buffer_end - self->buffer;
}

static inline size_t GapBuffer_gap_length(GapBuffer *self) {
  return self->gap_start - self->gap_end;
}

void GapBuffer_move_chars(GapBuffer *self, char *dest, char *src, size_t len) {
  if (dest == src || len == 0)
    return;

  if (src > dest) {
    if (src + len >= self->buffer_end)
      return;
    for (; len > 0; len--) {
      *(dest++) = *(src++);
    }
  } else {
    src += len;
    dest += len;
    for (; len > 0; len--) {
      *(--dest) = *(--src);
    }
  }
}

void GapBuffer_move_gap_to_selection(GapBuffer *self, Selection *sel) {
  char *head = self->buffer + sel->head;

  // The gap is already at the head
  if (head == self->gap_start)
    return;

  // Shift gap left
  if (head < self->gap_start) {
    size_t len = GapBuffer_gap_length(self);
    GapBuffer_move_chars(self, head + len, head, len);
    self->gap_start = head;
    self->gap_end = head + len;
  } else {
    size_t len = head - self->gap_end;
    GapBuffer_move_chars(self, self->gap_start, self->gap_end,
                         head - self->gap_end);
    self->gap_start += len;
    self->gap_end = head;
  }
}

void GapBuffer_extend_buffer(GapBuffer *self, size_t size) {
  char *orig = self->buffer;
  size_t new_size = GapBuffer_full_length(self) + size;
  self->buffer = realloc(self->buffer, new_size);

  size_t mem_offset = self->buffer - orig;

  self->buffer_end += mem_offset;
  self->gap_start += mem_offset;
  self->gap_end += mem_offset;
}

void GapBuffer_extend_gap(GapBuffer *self) {
  if (self->gap_end - self->gap_start > 0)
    return;

  GapBuffer_extend_buffer(self, GAP_SIZE);
  GapBuffer_move_chars(self, self->gap_end + GAP_SIZE, self->gap_end,
                       self->buffer_end - self->gap_end);

  self->gap_end += GAP_SIZE;
  self->buffer_end += GAP_SIZE;
}

void GapBuffer_put_char(GapBuffer *self, Selection *sel, char c) {
  GapBuffer_move_gap_to_selection(self, sel);
  GapBuffer_extend_gap(self);

  *(self->gap_start++) = c;
}

void GapBuffer_delete_chars(GapBuffer *self, Selection *sel) {
  GapBuffer_move_gap_to_selection(self, sel);

  if (self->gap_start == self->buffer)
    return;

  *(--self->gap_start) = 0;
}

// MARK TextEditor impls

void TextEditor_init(TextEditor *self) {
  memset(&self->selection, 0, sizeof(Selection));
  GapBuffer_init(&self->buffer);
}

void TextEditor_handle_key(TextEditor *self, char c) {
  switch ((int)c) {
  // Backspace
  case 127:
    GapBuffer_delete_chars(&self->buffer, &self->selection);
    self->selection.anchor = --self->selection.head;
    break;
  default:
    GapBuffer_put_char(&self->buffer, &self->selection, c);
    self->selection.anchor = ++self->selection.head;
    break;
  }
}
