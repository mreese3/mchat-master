#ifndef CURSES_UI_STATE_H
#define CURSES_UI_STATE_H

/*
 * This file contains definitions for the internal functions and state of the mchat curses ui.
 *
 * IMPORTANT NOTES ON WRITING NEW COMMANDS
 *
 * New commands can be easily implemented by including this file and using add_cmd() during init.
 * Each command should have a command string, syntax string, and help message defined as const char pointers
 * (ex: const char *xxx_string) as well as a function that matches the cmd_function definition.  These functions
 * should not be destructive, by calling endwin() or delwin on any main windows, etc., and should clean up
 * after themselves by freeing and malloc'd memory and deleting any windows they created.  If a command function
 * creates a new window, it should call werase(stdscr) and refresh() before returning.
 *
 * A special note on capturing input: command functions must return KEY_RESIZE if they received KEY_RESIZE while
 * running.  This ensures that ui_resize() is called when the main loop continues execution.
 *
 * One last note: The commands in builtin_cmds.c are considered to be part of the main mchat curses program.  There
 * should be very few commands implemented here.  Extra commands should be in a separate file with an appropriate
 * loader command.  In the future, dynamic loading is going to be implemented, so these extra files should be created
 * with that in mind.
 *
 */

#include <ncurses.h>
#include <mchatv1.h>

#define CURSES_UI_MAX_POSSIBLE_COMMANDS 1024
#define CURSES_UI_MAX_POSSIBLE_RUNNABLES 1024

// UI state tracking structure
// Used by the main UI program and cmd functions
typedef struct ui_state ui_state_t;
typedef int (*cmd_function)(ui_state_t *s, char *str);
typedef int (*runnable)(ui_state_t *s);

struct ui_state {
    // mchat struct pointer
    mchat_t *mchat;

    // Window pointers
    WINDOW *input_win;
    WINDOW *chat_win;
    WINDOW *status_win;

    // Screen Geometry
    unsigned int max_line;
    unsigned int max_col;

    // input_win cursor position and next keystroke to process
    unsigned int iw_line;
    unsigned int iw_col;
    unsigned int iw_col_prompt;
    unsigned int iw_col_start;
    int iw_next;

    // input_win options
    char *iw_prompt;
    char iw_cmd_escape;

    // chat_win line position
    unsigned int cw_line;

    // chat_win options
    char *cw_print_fmt;

    // input buffer
    char input_buf[MCHAT_LIMIT_MAX_MESSAGE_SIZE];
    unsigned int input_buf_len;

    // status line buffers
    char status_line_buf[1024];
    char status_line_urg_buf[1024];

    // status line options
    char status_line_blink : 1;		// normal status line should blink (default: false)
    char status_line_dim : 1;		// normal status line should be dim (default: false)
    char status_line_multipage : 1;	// normal status line should display mutliple pages (not implmented)
    char status_line_empty : 1;		// status line should not be printed (default: false)
    char status_line_is_urg : 1;	// status line should display urgent buffer (default: false)
    char status_line_urg_nodismiss : 1;	// status line should not be dismissed next keystroke (defailt: false)
    char status_line_urg_blink : 1;	// urgent status line should blink (default: false)
    char status_line_urg_flag4 : 1;	// reserved for future use

    // run flag (1 is running, 0 is ready to exit)
    unsigned int running;

    // command list
    unsigned int cmd_count;
    const char *cmd_names[CURSES_UI_MAX_POSSIBLE_COMMANDS];
    const char *cmd_syntax[CURSES_UI_MAX_POSSIBLE_COMMANDS];
    const char *cmd_help[CURSES_UI_MAX_POSSIBLE_COMMANDS];
    cmd_function cmd_funcs[CURSES_UI_MAX_POSSIBLE_COMMANDS];

    // main-loop runnables (not implemented yet)
    //unsigned int runnable_count;
    //runnable runnable_funcs[CURSES_UI_MAX_POSSIBLE_RUNNABLES];
};


// functions that are available to cmds are declared here
void chat_win_print(char *nickname, char *message);
void status_line_set(char *str, ...);
void status_line_urg_set(int now, char *str, ...);
void status_line_urg_unset();

// general cmd functions
int is_cmd(char *cmdstr);
int run_cmd(char *cmdstr);
int run_cmd_by_id(int id);
void add_cmd(const char *cmdstr, const char *syntax, const char *help, cmd_function func);

void load_builtin_cmds(ui_state_t *state);


#endif // CURSES_UI_STATE_H
