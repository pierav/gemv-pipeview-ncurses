#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_PARSE 0
#define DEBUG_DUMP 1

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Allocate and copy the input string */
char *salloc(char *str) {
    size_t n = strlen(str) + 1;
    char *ret = malloc(n);
    return strcpy(ret, str);
}

struct sig {
    char *str;
    uint32_t hash;
    struct sig *next;
};

/* Used to dedupilicate strings */
char *singleton(char *str) {
    // Compute hash (Dan Bernstein popuralized hash)
    uint32_t h = 5381;
    uint32_t len = strlen(str);
    char *s = str;
    while (len--) {
        /* h = 33 * h ^ s[i]; */
        h += (h << 5);
        h ^= *s++;
    }
    // Try to find str in static struct
    // TODO TREE
    // Create sentilenne with empty string
    static struct sig HEAD = {"", 0, NULL};
    struct sig *cur = &HEAD;
    do {
        if (cur->hash == h) {                  // Hash HIT
            if (strcmp(cur->str, str) == 0) {  // str match
                return cur->str;               // Return local str
            }
        }
        if (cur->next) {  // Jump to next
            cur = cur->next;
        } else {  // Allocate in case of end
            cur->next = malloc(sizeof(struct sig));
            cur = cur->next;
            cur->str = salloc(str);
            cur->hash = h;
            cur->next = NULL;
            return cur->str;  // Return newly allocated str
        }
    } while (1);
}

typedef struct cmd_C {
    unsigned char set;
    size_t value;
} cmd_C_t;

typedef struct cmd_I {
    size_t id;
    size_t id_sim;
    size_t id_thread;
} cmd_I_t;

typedef struct cmd_L {
    size_t id;
    uint8_t type;
    char *str;
} cmd_L_t;

typedef struct cmd_S {
    size_t id;
    size_t id_lane;
    char *stage;
} cmd_S_t, cmd_E_t;

typedef struct cmd_R {
    size_t id;
    size_t id_retire;
    uint8_t type;
} cmd_R_t;

typedef struct cmd_W {
    size_t id_consumer;
    size_t id_producer;
    int type;
} cmd_W_t;

typedef struct cmd {
    char id;
    union {
        cmd_C_t C;
        cmd_I_t I;
        cmd_L_t L;
        cmd_S_t S;
        cmd_E_t E;
        cmd_R_t R;
        cmd_W_t w;
    } astype;
} cmd_t;

// C=   CYCLE
//
// C	CYCLE
//
// I	INSN_ID_IN_FILE	INSN_ID_IN_SIM	THREAD_ID
// Initiate : Starts outputting commands for the specified instruction
//  -> INSN_ID_IN_FILE = <ID>
//  -> INSN_ID_IN_SIM = X
//  -> THREAD_ID X
//
// L	<ID>	<TYPE>	<TEXT>
// Add arbitrary comment text to an instruction
//
// S	<ID>	<LANE_ID>	<STAGE_NAME>
// Start a pipeline stage
//
// E	<ID>	<LANE_ID>	<STAGE_NAME>
// End a pipeline stage. This command can be omitted.
//
// R	<ID>	<RETIRE_ID>	 <TYPE>
// End command output for a specific instruction.
//
// W	<CONSUMER_ID>	<PRODUCER_ID>	<TYPE>
// Specify any dependencies
//
//
// Exemple:
// Kanata	0004    // File header and a version number
// C=	216                     // Start at cycle 216
// I	0	0	0	// Start an instruction 0
// L	0	0	12000d918 iBC(r17)              // Add label comment for
// the  instruction 0
// S	0	0	F       // Start F stage for the instruction 0
// C	1                       // Proceed 1 cycle
// S	0	0	X       // Start X stage for the instruction 0
// I	1	1	0       // Start an instruction 1
// L	1	0	12000d91c r4 = iALU(r3, r2)     // Add label comment for
// the  instruction 1
// S	1	0	F       // Start F stage in the instruction 0
// C	1                       // Proceed one cycle
// R	0	0	0       // Retire the instruction 0
// S	1	0	X       // Start X stage in the instruction 1
// C	1                       // Proceed one cycle
// R	1	1	1       // Flush the instruction 1

void cmd_print(cmd_t *cmd, FILE *f) {
    switch (cmd->id) {
        case 'C':
            fprintf(f, "%s\t%ld\n", cmd->astype.C.set ? "C=" : "C",
                    cmd->astype.C.value);
            break;
        case 'I':
            fprintf(f, "I\t%ld\t%ld\t%ld\n", cmd->astype.I.id,
                    cmd->astype.I.id_sim, cmd->astype.I.id_thread);
            break;
        case 'L':
            fprintf(f, "L\t%ld\t%d\t%s\n", cmd->astype.L.id, cmd->astype.L.type,
                    cmd->astype.L.str);
            break;
        case 'S':
        case 'E':
            fprintf(f, "%c\t%ld\t%ld\t%s\n", cmd->id, cmd->astype.S.id,
                    cmd->astype.S.id_lane, cmd->astype.S.stage);
            break;
        case 'R':
            fprintf(f, "R\t%ld\t%ld\t%d\n", cmd->astype.R.id,
                    cmd->astype.R.id_retire, cmd->astype.R.type);
            break;
        default:
            fprintf(stderr, "cmd_print: Invalid cmd ID: %c\n", cmd->id);
            exit(1);
    }
}

#define FT "%[^\t]"
#define FN "%[^\n]"

int cmd_parse(char *line, cmd_t *cmd) {
    char buffer[1024];
    // TODO: dynamic allocation
    if (strlen(line) >= sizeof(buffer)) {
        fprintf(stderr, "cmd_parse: line too long: %s\n", line);
        exit(1);
    }

    cmd->id = line[0];
    switch (cmd->id) {
        case 'C': {
            sscanf(line, FT "\t%ld", buffer, &cmd->astype.C.value);
            if (buffer[1] == '=') {
                cmd->astype.C.set = 1;
            }
            break;
        }
        case 'I': {
            sscanf(line, "%*c\t%ld\t%ld\t%ld", &cmd->astype.I.id,
                   &cmd->astype.I.id_sim, &cmd->astype.I.id_thread);
            break;
        }
        case 'L': {
            sscanf(line, "%*c\t%ld\t%hhd\t" FN, &cmd->astype.L.id,
                   &cmd->astype.L.type, buffer);
            cmd->astype.L.str = salloc(buffer);
            break;
        }
        case 'S':
        case 'E': {
            sscanf(line, "%*c\t%ld\t%ld\t" FN, &cmd->astype.S.id,
                   &cmd->astype.S.id_lane, buffer);
            cmd->astype.S.stage = singleton(buffer);
            break;
        }
        case 'R': {
            sscanf(line, "%*c\t%ld\t%ld\t%hhd", &cmd->astype.R.id,
                   &cmd->astype.R.id_retire, &cmd->astype.R.type);
            break;
        }
        default:
            fprintf(stderr, "cmd_parse: Invalid cmd ID: %c\n", cmd->id);
            exit(1);
    }
    return 0;
}

cmd_t *cmd_parse_file(char *filename, size_t *size) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        fprintf(stderr, "Invalid file: %s\n", filename);
        exit(1);
    }

    /* First pass: count lines: */
    {
        char buf[65536];  // Stack buffer
        int counter = 0;
        for (;;) {
            size_t res = fread(buf, 1, sizeof(buf), fp);
            for (size_t i = 0; i < res; i++)
                if (buf[i] == '\n') counter++;
            if (feof(fp)) break;
        }
        rewind(fp);
        *size = counter - 1;
    }

    // Allocate data
    cmd_t *cmds = malloc(*size * sizeof(cmd_t));

    /* Second pass: parse data */
    if (getline(&line, &len, fp) == -1) {
        fprintf(stderr, "Missing header file\n");
        exit(1);
    }

    if (strcmp("Kanata\t0004\n", line) != 0) {
        fprintf(stderr, "Bad file format: %s\n", line);
        exit(1);
    }

    size_t i = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        cmd_t *cmd = &cmds[i];

        // printf("%s", line);
        cmd_parse(line, cmd);

        if (DEBUG_PARSE) {
            char *buffer_debug = malloc(len);
            FILE *fpd = fmemopen(buffer_debug, len, "w");
            cmd_print(cmd, fpd);
            fclose(fpd);  // flush
            // printf("%s", buffer_debug);
            if (strcmp(line, buffer_debug) != 0) {
                fprintf(stderr, "Bad parsing: Line differs\n");
                exit(1);
            }
            free(buffer_debug);
        }

        i++;
    }

    fclose(fp);
    if (line) free(line);

    return cmds;
}

/* Database */

#include "parser.h"

void inst_dump(inst_t *inst) {
    printf("[%ld:%ld] %20s:\n", inst->start_time, inst->end_time,
           inst->text_display);
    for (size_t i = 0; i < inst->nb_states; i++) {
        inst_state_t *s = &inst->states[i];
        printf("---> %08ld: .%c [%s] : %s\n", s->time, s->iser, s->stage,
               s->text);
    }
}

void inst_state_append(inst_t *inst, size_t time, char iser, char *stage,
                       char *text) {
    inst->nb_states += 1;
    inst->states =
        realloc(inst->states, inst->nb_states * sizeof(inst_state_t));
    inst_state_t *s = &inst->states[inst->nb_states - 1];  // Last state

    s->time = time;
    s->iser = iser;
    s->stage = stage;
    s->text = text;
}

db_t *inst_create_database(cmd_t *cmds, size_t size) {
    // First pass: compute idx range
    size_t nb_inst = 0;
    for (size_t i = 0; i < size; i++) {
        if (cmds[i].id == 'I') {
            nb_inst = MAX(nb_inst, cmds[i].astype.I.id + 1);
        }
    }

    // Allocate db and insts
    db_t *db = calloc(sizeof(db_t), 1);
    assert(db);
    printf("Allocate %ld insts\n", nb_inst);
    inst_t *inst = calloc(sizeof(inst_t), nb_inst);
    assert(inst);

    // Second pass: generates insts
    uint64_t time = 0;

    for (size_t i = 0; i < size; i++) {
        cmd_t *cmd = &cmds[i];
        // printf("PROCESS => \n");
        // cmd_print(cmd, stdout);
        switch (cmds[i].id) {
            case 'C':
                if (cmd->astype.C.set) {
                    time = cmd->astype.C.value;
                    db->start_time = time;
                } else {
                    time += cmd->astype.C.value;
                }
                break;
            case 'I':
                inst[cmd->astype.I.id].valid = 1;
                inst[cmd->astype.I.id].start_time = time;
                break;
            case 'L': {
                if (cmd->astype.L.type == 0) {
                    // assert(inst[cmd->astype.L.id].text_display == NULL);
                    inst[cmd->astype.L.id].text_display = cmd->astype.L.str;
                } else {
                    // TODO
                }
                break;
            }
            case 'S': {
                inst_state_append(&inst[cmd->astype.S.id], time, 'S',
                                  cmd->astype.S.stage, "");
                break;
            }
            case 'E': {
                inst_state_append(&inst[cmd->astype.E.id], time, 'E',
                                  cmd->astype.E.stage, "");
                break;
            }
            case 'R': {
                inst_state_append(&inst[cmd->astype.R.id], time, 'R', " ", "");
                if (cmd->astype.R.type == 1) {
                    inst[cmd->astype.R.id].flushed = 1;
                }
                inst[cmd->astype.R.id].end_time = time;
                break;
            }
        }
    }
    db->nb_inst = nb_inst;
    db->insts = inst;
    db->end_time = time;
    return db;
}

db_t *parse(char *filename) {
    size_t size;
    cmd_t *cmds = cmd_parse_file(filename, &size);

#if DEBUG_PARSE
    printf("size = %ld\n", size);
    for (size_t i = 0; i < size; i++) {
        cmd_print(&cmds[i], stdout);
    }
#endif

    db_t *db = inst_create_database(cmds, size);
    db->filename = filename;
#if DEBUG_DUMP
    printf("Got %ld inst in [%ld:%ld] cycles <%s>\n", db->nb_inst,
           db->start_time, db->end_time, db->filename);
    for (size_t i = 0; i < db->nb_inst; i++) {
        inst_dump(&db->insts[i]);
    }
#endif

    return db;
}

__attribute__((weak)) int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }
    char *filename = argv[1];
    db_t *db = parse(filename);
    (void)db;
}
