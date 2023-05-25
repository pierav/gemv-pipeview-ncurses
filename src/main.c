#include <assert.h>
#include <math.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

char *render_data[1024];
uint64_t render_size;

void HSLToRGB(float H, float S, float L, float rgb[]);

int render_init(char *filename) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(filename, "r");
    if (fp == NULL) exit(EXIT_FAILURE);

    render_size = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        render_data[render_size] = malloc(len);
        memcpy(render_data[render_size], line, len);
        render_size++;
        if (render_size >= 1024) break;
    }

    fclose(fp);
    if (line) free(line);
    return 0;
}

char *render_line(uint64_t index) {
    if (index < render_size) return render_data[index];
    return "";
}

float color_saturation = 0.4;
float color_lightness = 0.4;
// Color
const unsigned int offset = 16 + 1;  // 0 reserved + 16 reserved
const unsigned int nb_color = 100;   // Palette size

void palette_init() {
    // Fill [1:16] with DEFAULT_COLOR/BLACK
    for (size_t c = 1; c < offset; c++) {  // Palette size
        init_pair(c, c, COLOR_BLACK);
    }
    // Fill [17:+100] with WHITE/Palette(x)
    float rgb[3];
    for (size_t dark = 0; dark <= 1; dark++) {
        size_t dark_offset = offset + (dark * nb_color);
        for (size_t c = 0; c < nb_color; c++) {  // Palette size
            size_t ci = c + dark_offset;         // Curses index
            float coef = (float)c / nb_color;
            HSLToRGB(coef, color_saturation, color_lightness / (dark + 1), rgb);
            init_color(ci, rgb[0] * 1000, rgb[1] * 1000, rgb[2] * 1000);
            init_pair(ci, COLOR_WHITE, ci);
        }
    }
}

unsigned int palette_get_pair(int coef100, bool dark) {
    return offset + coef100 + (dark ? nb_color : 0);
}

bool inst_char_display(char c, size_t base_time, size_t printtime,
                       size_t cur_time, size_t printcount, size_t draww) {
    if (printcount >= draww) return true;
    if (printtime == cur_time) {
        attron(A_REVERSE);
        attron(A_STANDOUT);
    }
    printw("%c", c);
    if (printtime == cur_time) {
        attroff(A_REVERSE);
        attroff(A_STANDOUT);
    }
    return true;
}

void inst_display(inst_t *inst, size_t base_time, size_t cur_time, size_t winw,
                  size_t draww) {
    char stage = 'X';
    unsigned int stage_color = 1;

    size_t delta_time = 0;

    size_t printcount = 0;
    size_t printtime = base_time;

    // Allign to base time
    for (; printtime < inst->start_time; printcount++, printtime++) {
        inst_char_display(' ', base_time, printtime, cur_time, printcount,
                          draww);
    }

    for (size_t i = 0; i < inst->nb_states; i++) {
        inst_state_t *s = &inst->states[i];

        if (s->iser == 'R') break;

        switch (s->iser) {
            case 'I':
                stage = 'X';  // Error
                break;
            case 'S': {               // Start
                stage = s->stage[0];  // First stage char
                int32_t hash = 5381;
                for (char *c = s->stage; *c != '\0'; c++) {
                    hash += (hash << 5);
                    hash ^= *c;
                }
                stage_color = palette_get_pair(hash % 100, inst->flushed);
                break;
            }
            case 'E':  // End State; stall
                stage = '.';
                break;
            case 'R':
                stage = 'X';  // Error
                break;
        }
        attron(COLOR_PAIR((stage_color)));
        delta_time = inst->states[i + 1].time - inst->states[i].time;
        // printw("%d ", delta_time);
        for (size_t j = 0; j < delta_time; j++) {
            inst_char_display(stage, base_time, printtime, cur_time, printcount,
                              draww);
            printcount++;
            printtime++;
        }
        // printw("%08ld: .%c [%s] : %s", s->time, s->iser, s->stage, s->text);
    }

    attron(COLOR_PAIR(7));
    for (; printcount < draww; printcount++, printtime++) {
        inst_char_display(' ', base_time, printtime, cur_time, printcount,
                          draww);
    }
    printw(" %20s", inst->text_display);
    printw("                                       ");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FILE>\n", argv[0]);
        exit(1);
    }
    char *filename = argv[1];
    // render_init(filename);

    // Create database
    db_t *db = parse(filename);

    // ncurses init
    initscr();                  /* start the curses mode    */
    keypad(stdscr, TRUE);       /* Enable all KEYS          */
    start_color();              /* Init curser colors       */
    assert(has_colors());       /* For now assert colors    */
    assert(can_change_color()); /* ^                        */
    palette_init();             /* Initiate my palette      */
    // cbreak();
    // nodelay(stdscr, TRUE); /* No delaying */
    // init_pair(1, COLOR_WHITE, COLOR_BLACK);
    // init_pair(2, COLOR_BLACK, COLOR_CYAN);
    int x = 0, y = 0;  // Cursor position;
    int row, col;      /* to store the number of rows and *
                        * the number of colums of the screen */

    getmaxyx(stdscr, row, col); /* get the number of rows and columns */
    WINDOW *win = newwin(20, 20, 20, 20);
    refresh();

    uint16_t scr_split = 0;

    uint16_t scr_header_offset = 1;
    uint16_t scr_footer_offset = 1;

    int ch;

    size_t cur_inst = 0;
    size_t cur_time = 0;

    while ((ch = getch()) != KEY_F(2)) {
        getmaxyx(stdscr, row, col); /* get the number of rows and columns */
        // Split screen in 2:
        // Update sizes

        scr_split = col * 2 / 3;
        uint16_t win_height = row - scr_header_offset - scr_footer_offset;

        wresize(win, win_height, col - scr_split);
        mvwin(win, scr_footer_offset, scr_split);
        box(win, 0, 0); /* 0, 0 gives default characters  */

        // Gey input
        switch (ch) {
            case KEY_LEFT:
                x--;
                cur_time--;
                break;

            case KEY_RIGHT:
                x++;
                cur_time++;
                break;

            case KEY_UP:
                y++;
                break;
            case KEY_DOWN:
                y--;
                break;
            case ' ':
                y -= col;
                break;
        }

        // Header
        attron(COLOR_PAIR(palette_get_pair(75, 0)));
        mvprintw(0, 0, "Konata-ncurses");
        printw(" --- %s ---", db->filename);
        printw(" I(%ld / %ld)", cur_inst, db->nb_inst);
        printw(" C(%ld / [%ld:%ld])", cur_time, db->start_time, db->end_time);
        for (int i = 0; i < col; i++) {
            printw(" ");
        }

        // Data

        size_t draww = scr_split * 3 / 4;

        size_t init_row = 1, init_index = -y;
        size_t base_time;
        if (init_index >= db->nb_inst) {
            base_time = 0;
        } else {
            base_time = db->insts[init_index].start_time;
        }
        if (cur_time < base_time) {
            cur_time = base_time;
        }
        if (cur_time > base_time + draww - 1) {
            cur_time = base_time + draww - 1;
        }
        // cur_time = base_time + 10;
        cur_inst = init_index;
        for (size_t i = init_row, index = init_index; i < row - 1;
             i++, index++) {
            move(i, 0);
            if (index >= db->nb_inst) {  // Display blanck line
                attron(COLOR_PAIR(1));
                for (size_t c = 0; c <= scr_split / 8; c++) printw(".        ");
            } else {
                inst_display(&db->insts[index], base_time, cur_time, scr_split,
                             draww);
            }
            /*char *str = render_line(i - y);
            for (int j = 0; j < col && *str != '\0'; j++) {
                attron(COLOR_PAIR(color++ % 256));
                printw("%c", *str);
                str++;
            }*/
            // mvprintw(i, 0, render_line(i-y));
        }

        // Bottom
        attron(COLOR_PAIR(palette_get_pair(75, 0)));
        mvprintw(row - 1, 0, "This screen has %d rows and %d columns (%d, %d) ",
                 row, col, y, x);
        printw("COLORS = %d, COLOR_PAIRS = %d\n", COLORS, COLOR_PAIRS);
        printw("CH=%x", ch);
        for (int i = 0; i < col; i++) {
            printw(" ");
        }
        move(row / 2, 0);

        // Refresh
        wnoutrefresh(stdscr);
        wnoutrefresh(win); /* Show that box 		*/
        doupdate();
    }
    endwin();

    return 0;
}
