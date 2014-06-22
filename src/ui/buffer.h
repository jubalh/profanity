#ifndef UI_BUFFER_H
#define UI_BUFFER_H

#include "config.h"

//#include "ui/window.h"
#define BUFF_SIZE 1000

typedef struct prof_buff_entry_t {
  char show_char;
  GTimeVal tstamp;
  int flags;
  int attrs;
  char *from;
  char *message;
} ProfBuffEntry;

typedef struct prof_buff_t {
  ProfBuffEntry entry[BUFF_SIZE];
  int wrap;
  int current;
} ProfBuff;


ProfBuff* buffer_create();
void buffer_free(ProfBuff* buffer);
void buffer_push(ProfBuff* buffer, const char show_char, GTimeVal *tstamp, int flags, int attrs, const char * const from, const char * const message);
int buffer_yield(ProfBuff* buffer, int line, ProfBuffEntry** list);
#endif
