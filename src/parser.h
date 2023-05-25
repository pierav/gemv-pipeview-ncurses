#pragma once

#include <stdint.h>

typedef struct inst_state {
    size_t time;
    char iser;  // Init Stage Endstage Retire
    char *stage;
    char *text;
} inst_state_t;

typedef struct inst {
    char valid;
    size_t start_time;
    size_t end_time;
    bool flushed;
    char *text_display;

    inst_state_t *states;
    size_t nb_states;
} inst_t;

typedef struct db {
    char *filename;     /* DB source filename */
    size_t start_time;  /* Cycles */
    size_t end_time;    /* Cycles */
    size_t nb_inst;     /* Number of instructions */
    inst_t *insts;      /* the instructions sorted by id */
} db_t;

db_t *parse(char *filename);
