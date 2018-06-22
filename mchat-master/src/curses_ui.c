#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>
#include <time.h>
#include "curses_ui.h"
#include "curses_ui_internal.h"
#include "curses_ui_defaults.h"
#include <mchatv1.h>

// Window sizes relative to screen size
// Could be changed in the future but for now a fixed ratio
#define chat_win_y(y) y - 9
#define input_win_y(y) 8

// Global UI State - See curses_ui_interal.h for details
static ui_state_t state;

void chat_win_print(char *nickname, char *message)
{
    time_t t = time(0);
    struct tm *ts = localtime(&t);
    mvwprintw(state.chat_win, state.cw_line, 2, state.cw_print_fmt, ts->tm_hour, ts->tm_min, ts->tm_sec,
        ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday, nickname, message);
    if (state.cw_line < chat_win_y(state.max_line) - 2)
        state.cw_line++;
    else
        scroll(state.chat_win);
}

void status_line_set(char *str, ...)
{
    if (str)
    {
        int len = strlen(str);
            if (len >= 1024)
            return;

        va_list args;
        va_start(args, str);
        vsprintf(state.status_line_buf, str, args);
        va_end(args);
    }
    if (!state.status_line_is_urg)
    {
        wattroff(state.status_win, A_BOLD);
        for (unsigned int i = 0; i < state.max_col; i++)
            mvwaddch(state.status_win, 0, i, ' ');
        mvwprintw(state.status_win, 0, 1, "Status: %s", state.status_line_buf);
    }
}


void status_line_urg_set(int now, char *str, ...)
{
    if (strlen(str) > 1024)
        return;
    va_list args;
    va_start(args, str);
    vsprintf(state.status_line_urg_buf, str, args);
    va_end(args);
    if (now)
    {
        wattron(state.status_win, A_BOLD);
        for (unsigned int i = 0; i < state.max_col; i++)
            mvwaddch(state.status_win, 0, i, ' ');
        mvwprintw(state.status_win, 0, 1, "Status: %s", state.status_line_urg_buf);
        state.status_line_is_urg = 1;
    }
}


void status_line_urg_unset()
{
    wattroff(state.status_win, A_BOLD);
    for (unsigned int i = 0; i < state.max_col; i++)
        mvwaddch(state.status_win, 0, i, ' ');

    mvwprintw(state.status_win, 0, 1, "Status: %s", state.status_line_buf);
    state.status_line_is_urg = 0;
}


int is_cmd(char *cmdstr)
{
    // This could be more sophisticated in the future
    if (cmdstr[0] == state.iw_cmd_escape && cmdstr[1] != state.iw_cmd_escape)
        return 1;
    else
        return 0;
}


int run_cmd(char *cmdstr)
{
    int found = -1;
    char *ptr = cmdstr;
    if (cmdstr[0] == state.iw_cmd_escape)
        ptr = cmdstr + 1;

    for (unsigned int i = 0; i < state.cmd_count; i++)
    {
        if (strncasecmp(state.cmd_names[i], ptr, strlen(state.cmd_names[i])) == 0)
            found = i;
    }

    if (found  == -1)
        return -4096;
    else
        return state.cmd_funcs[found](&state, ptr);
}

// Add new command to the UI - Should be used by init routine to plugins in the future
void add_cmd(const char *cmdstr, const char *syntax, const char *help, cmd_function func)
{
    state.cmd_names[state.cmd_count] = cmdstr;
    state.cmd_syntax[state.cmd_count] = syntax;
    state.cmd_help[state.cmd_count] = help;
    state.cmd_funcs[state.cmd_count] = func;
    state.cmd_count++;
}


// Risize the UI on screen change
void ui_resize()
{
    getmaxyx(stdscr, state.max_line, state.max_col);
    wresize(state.chat_win, chat_win_y(state.max_line), state.max_col);
    mvwin(state.chat_win, 0, 0);

    wresize(state.input_win, input_win_y(state.max_line), state.max_col);
    mvwin(state.input_win, chat_win_y(state.max_line), 0);

    wresize(state.status_win, 1, state.max_col);
    mvwin(state.status_win, state.max_line - 1, 0);

    wsetscrreg(state.chat_win, 1, chat_win_y(state.max_line) - 2);
    wsetscrreg(state.input_win, 1, input_win_y(state.max_line) - 2);

    if (state.cw_line > state.max_line - 2)
        state.cw_line = state.max_line - 2;

    if (state.iw_line > state.max_line - 2)
        state.iw_line = state.max_line - 2;

    if (state.iw_col > state.max_col - 2)
        state.iw_col = state.max_col - 2;

}

//Public functions

// Most of this function is dark ncurses voodoo magic - so do not touch!
void ui_init(char *nickname)
{
    memset(&state, 0, sizeof(ui_state_t));

    //set defaults
    state.cw_print_fmt = (char*)default_cw_print_fmt;
    state.iw_cmd_escape = (char)default_iw_cmd_escape;
    state.iw_prompt = (char*)default_iw_prompt;

    // set line and column stuff
    state.iw_col_prompt = 2;
    state.iw_col_start = strlen(state.iw_prompt) + state.iw_col_prompt;
    state.iw_line = 1;
    state.iw_col = state.iw_col_start;
    state.cw_line = 1;

    // Load built-in commands
    load_builtin_cmds(&state);
    // initialize ncurses
    initscr();
    getmaxyx(stdscr, state.max_line, state.max_col);
    cbreak();
    noecho();
    nonl();
    halfdelay(1);
    state.chat_win = newwin(chat_win_y(state.max_line), state.max_col, 0, 0);
    state.input_win = newwin(input_win_y(state.max_line), state.max_col, chat_win_y(state.max_line), 0);
    state.status_win = newwin(0, state.max_col, state.max_line - 1, 0);
    keypad(state.input_win, TRUE);
    scrollok(state.chat_win, TRUE);
    scrollok(state.input_win, TRUE);
    wsetscrreg(state.chat_win, 1, chat_win_y(state.max_line) - 2);
    wsetscrreg(state.input_win, 1, input_win_y(state.max_line) - 2);
    wattron(state.status_win, A_REVERSE);
    box(state.chat_win, 0, 0);
    box(state.input_win, 0, 0);
    mvwprintw(state.input_win, 1, 2, state.iw_prompt);
    wmove(state.input_win, state.iw_line, state.iw_col);
    wrefresh(state.chat_win);
    wrefresh(state.status_win);
    wrefresh(state.input_win);

    // finally start mchat
    state.mchat = mchatv1_init(NULL);
    status_line_set("Disconnected");
    state.running = 1;
}

// Our main event loop for the UI
void ui_run()
{
    while (state.running)
    {
        state.iw_next = mvwgetch(state.input_win, state.iw_line, state.iw_col);
        if (state.iw_next != ERR)
        {
            // Reset status_line
            if (state.status_line_is_urg && !state.status_line_urg_nodismiss)
                status_line_urg_unset();

            // Catch message length
            if (state.input_buf_len == MCHAT_LIMIT_MAX_MESSAGE_SIZE - 1)
            {
                status_line_urg_set(1, "Maximum Message Length");
            }
            // Printable ascii range
            else if (state.iw_next >= 32 && state.iw_next <= 126)
            {
                mvwaddch(state.input_win, state.iw_line, state.iw_col, state.iw_next);
                state.input_buf[state.input_buf_len] = (char)state.iw_next;
                if (state.iw_col != state.max_col - 2)
                    state.iw_col++;
                else
                {
                    state.iw_col = state.iw_col_prompt;
                    if (state.iw_line != input_win_y(state.max_line) - 2)
                        state.iw_line++;
                    else
                        scroll(state.input_win);
                }
                state.input_buf_len++;
            }
            //backspace
            else if (state.iw_next == KEY_BACKSPACE || state.iw_next == 127)
            {
                if (state.input_buf_len > 0)
                {
                    state.input_buf_len--;
                    state.input_buf[state.input_buf_len] = 0;
                    if (state.iw_col > state.iw_col_prompt)
                        state.iw_col--;
                    else
                    {
                        state.iw_line--;
                        state.iw_col = state.max_col - 3;
                    }
                    mvwaddch(state.input_win, state.iw_line, state.iw_col, ' ');
                }

            }
            // Enter Key
            else if (state.iw_next == KEY_ENTER || state.iw_next == 10 || state.iw_next == 13)
            {
                if (state.input_buf_len > 0)
                {
                    state.input_buf[state.input_buf_len] = '\0';
                    if (is_cmd(state.input_buf))
                    {
                        int ret = run_cmd(state.input_buf);
                        if (ret == -4096)
                            status_line_urg_set(1, "Unknown Command: %s", state.input_buf);
                        else if (ret == KEY_RESIZE)
                            ui_resize();
                    }
                    else
                    {
                        mchatv1_send_message(state.mchat, state.input_buf);
                        char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
                        mchatv1_get_nickname(state.mchat, nick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
                        chat_win_print(nick, state.input_buf);
                    }
                    if (state.iw_line != input_win_y(state.max_line) - 2)
                        state.iw_line++;
                    else
                        scroll(state.input_win);
                    mvwaddstr(state.input_win, state.iw_line, state.iw_col_prompt, state.iw_prompt);
                    waddch(state.input_win, ' ');
                    state.iw_col = state.iw_col_start;
                    state.input_buf_len = 0;
                }
            }
            //handle terminal resizes
            else if (state.iw_next == KEY_RESIZE)
            {
                ui_resize();
            }
            // Unknown Key mesg
            else
            {
                status_line_urg_set(1, "Unknown Key: 0x%x", state.iw_next);
            }
        }

        mchat_message_t *mesg;
        if (mchatv1_recv_message(state.mchat, &mesg) > 0)
        {
            char recv_nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
            char recv_mesg[MCHAT_LIMIT_MAX_MESSAGE_SIZE];
            memset(recv_nick, 0, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
            memset(recv_mesg, 0, MCHAT_LIMIT_MAX_MESSAGE_SIZE);
            mchatv1_message_get_body(mesg, recv_mesg, MCHAT_LIMIT_MAX_MESSAGE_SIZE);
            mchatv1_message_get_nickname(mesg, recv_nick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
            mchatv1_message_destroy(&mesg);
            chat_win_print(recv_nick, recv_mesg);
        }

        refresh();
        box(state.chat_win, 0, 0);
        box(state.input_win, 0, 0);
        wrefresh(state.chat_win);
        status_line_set(NULL);
        wrefresh(state.status_win);
        wrefresh(state.input_win);
    }
}

// Time to die
void ui_destroy()
{
    mchatv1_send_message(state.mchat, "<Diconnected>");
    mchatv1_destroy(&state.mchat);
    endwin();
}
