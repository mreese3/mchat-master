#define _GNU_SOURCE
#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <mchatv1.h>
#include "curses_ui_internal.h"

/* built-in commands implemented
 * -help - get help on command
 * -quit - graceful exit
 * -clear - clear chat window and input window
 * -nick - change nickname during runtime
 * -list - list loaded commands
 * -connect - connect to channel
 * -disconnect - disconnect from channel
 * -peerlist - list of peers we have seen
 *
 * built-in commands to implement
 * -loadcmd - load a new command
 * -loadrun - load a new runtime module
 * -addchannel - add a new channel
 * -delchannel - remove a new channel
 */


const char *help_string = "help";
const char *help_syntax = "\\HELP [COMMAND]";
const char *help_help = "Get help text for command";
int help_function(ui_state_t *state, char *str)
{
    // To Do: Handle paging if window is not large enough to contain an entire help message
    char *ptr = str + strlen(help_string);
    while (isspace(ptr[0])) ptr++;
    char *cmd;
    if (ptr[0] == 0)
        cmd = (char*)help_string;
    else
        cmd = ptr;

    if (is_cmd(cmd))
        cmd++;

    int cmdnum = -1;
    for (int i = 0; i < state->cmd_count; i++)
    {
        if (strncasecmp(cmd, state->cmd_names[i], strlen(state->cmd_names[i])) == 0)
            cmdnum = i;
    }

    if (cmdnum == -1)
    {
        status_line_urg_set(1, "\\HELP Could not find command %s", cmd);
        return -1;
    }

    WINDOW *help_win = newwin(state->max_line / 2, state->max_col / 2, state->max_line / 4, state->max_col / 4);
    int x, y, bx, by;
    getmaxyx(help_win, y, x);
    getbegyx(help_win, by, bx);
    WINDOW *text_win = subwin(help_win, y-2, x-2, by + 1, bx + 2);
    getmaxyx(text_win, y, x);

    box(help_win, 0, 0);
    touchwin(help_win);

    wattron(text_win, A_BOLD);
    char *header = "Command Help";
    mvwprintw(text_win, 0, (x / 2) - (strlen(header) / 2) - (strlen(state->cmd_names[cmdnum]) / 2) - 1, "%s: %s", header, state->cmd_names[cmdnum]);
    wattroff(text_win, A_BOLD);

    mvwprintw(text_win, 3, 0, "Syntax: %s", state->cmd_syntax[cmdnum]);
    mvwprintw(text_win, 4, 0, state->cmd_help[cmdnum]);

    char *footer = "Press any key to continue...";
    mvwprintw(text_win, y - 1, (x / 2) - (strlen(footer) / 2), footer);

    int ret = 0;
    while((ret = wgetch(help_win)) == ERR);

    delwin(text_win);
    delwin(help_win);
    werase(stdscr);
    refresh();
    return ret;
}


const char *quit_string = "quit";
const char *quit_syntax = "\\QUIT";
const char *quit_help = "Gracefully exit for mchat.  Takes no arguments";
int quit_function(ui_state_t *state, char *str)
{
    // Simple command to quit the program gracefully
    state->running = 0;
    return 0;
}


const char *clear_string = "clear";
const char *clear_syntax = "\\CLEAR";
const char *clear_help = "Clear the chat and input windows.  Takes no arguments";
int clear_function(ui_state_t *state, char *str)
{
    werase(state->chat_win);
    werase(state->input_win);
    state->cw_line = 1;
    state->iw_line = 0;
    state->iw_col = state->iw_col_start;
    status_line_urg_set(1, "Screen Cleared");
    return 0;
}


const char *nick_string = "nick";
const char *nick_syntax = "\\NICK [NEW_NICKNAME]";
const char *nick_help = "Get or set your chat nickname.  Takes a new nickname as an argument";
int nick_function(ui_state_t *state, char *str)
{
    if (strlen(str) == strlen(nick_string))
    {
        char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
        mchatv1_get_nickname(state->mchat, nick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
        status_line_urg_set(1, "Your nickname is %s", nick);
        return 0;
    }
    char *ptr = str + strlen(nick_string);
    while (isspace(ptr[0])) ptr++;
    int newlen = strlen(ptr);
    ptr[newlen] = '\0';
    if (newlen > MCHAT_LIMIT_MAX_NICKNAME_SIZE || newlen == 0)
    {
        status_line_urg_set(1, "\\NICK ERROR: Invalid Argument");
        return -1;
    }

    char oldnick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
    mchatv1_get_nickname(state->mchat, oldnick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
    mchatv1_set_nickname(state->mchat, ptr, newlen);

    char *msg;
    asprintf(&msg, "%s has changed their nickname to %s", oldnick, ptr);
    mchatv1_send_message(state->mchat, msg);
    free(msg);
    status_line_urg_set(1, "Your new nickname is %s", ptr);
    if (mchatv1_is_connected(state->mchat))
    {
        char channel[2048];
        mchatv1_get_channel(state->mchat, channel, 2048);
        status_line_set("Connected to %s as %s", channel, ptr);
    }
    else
        status_line_set("Disconnected");
    return 0;
}


const char *list_string = "list";
const char *list_syntax = "\\LIST";
const char *list_help = "List loaded commands.  Takes no arguments";
int list_function(ui_state_t *state, char  *str)
{
    // To Do: handle paging if command list is longer than one page
    WINDOW *list_win = newwin(state->max_line - 2, state->max_col - 2, 1, 1);
    int x, y, bx, by;
    getmaxyx(list_win, y, x);
    getbegyx(list_win, by, bx);
    WINDOW *cmd_win = subwin(list_win, y - 3, x / 4 - 1, by + 1, bx + 2);
    WINDOW *syntax_win = subwin(list_win, y - 3, x / 4 - 1, by + 1, bx + 2 + x / 4);
    WINDOW *help_win = subwin(list_win, y - 3, x / 2 - 4, by + 1, bx + 2 + x / 2);
    box(list_win, 0, 0);
    touchwin(list_win);
    wborder(cmd_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wborder(syntax_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wrefresh(cmd_win);
    wrefresh(syntax_win);
    wrefresh(help_win);

    // Print headings
    wattron(cmd_win, A_BOLD);
    wattron(syntax_win, A_BOLD);
    wattron(help_win, A_BOLD);
    mvwprintw(cmd_win, 0, 0, "Command Name");
    mvwprintw(syntax_win, 0, 0, "Command Syntax");
    mvwprintw(help_win, 0, 0, "Command Description");
    wattroff(cmd_win, A_BOLD);
    wattroff(syntax_win, A_BOLD);
    wattroff(help_win, A_BOLD);

    int line = 1;
    for (int i = 0; i < state->cmd_count; i++)
    {
        mvwprintw(cmd_win, line, 0, "\\%s", state->cmd_names[i]);
        mvwprintw(syntax_win, line, 0, state->cmd_syntax[i]);
        mvwprintw(help_win, line, 0, state->cmd_help[i]);
        line = getcury(help_win) + 1;
    }

    char *footer = "Press any key to continue...";
    mvwprintw(list_win, y - 2, (x / 2) - (strlen(footer) / 2), footer);

    int ret = 0;
    while ((ret = wgetch(list_win)) == ERR);
    delwin(cmd_win);
    delwin(syntax_win);
    delwin(help_win);
    delwin(list_win);
    werase(stdscr);
    refresh();
    return ret;
}


const char *connect_string = "connect";
const char *connect_syntax = "\\CONNECT [CHANNEL_NAME]";
const char *connect_help = "Connect to a defined channel (defaults to channel #mchat)";
int connect_function(ui_state_t *state, char *str)
{
    if (mchatv1_is_connected(state->mchat))
    {
        char channel_name[2048];
        mchatv1_get_channel(state->mchat, channel_name, 2048);
        status_line_urg_set(1, "Already connected to %s", channel_name);
        return -1;
    }

    char *channel = NULL;
    if (strlen(str) != strlen(connect_string))
    {
        char *channel = str + strlen(connect_string);
        while(isblank(*channel)) channel++;
        if (*channel != '#')
        {
            status_line_urg_set(1, "Channel names must start with the # symbol");
            return -1;
        }
    }
    int ret = mchatv1_connect(state->mchat, channel);
    if (ret != 0)
    {
        status_line_urg_set(1, "Could not connect to the requested channel");
        return -1;
    }
    char new_channel[2048];
    char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
    mchatv1_get_channel(state->mchat, new_channel, 2048);
    mchatv1_get_nickname(state->mchat, nick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
    mchatv1_send_message(state->mchat, "<Connected>");
    chat_win_print(nick, "<Connected>");
    status_line_set("Connected to %s as %s", new_channel, nick);
    return 0;
}


const char *disconnect_string = "disconnect";
const char *disconnect_syntax = "\\DISCONNECT";
const char *disconnect_help = "Disconnect from an mchat channel";
int disconnect_function(ui_state_t *state, char *ptr)
{
    // There may be a bug here (got a segfault once)
    // I have not been able to replicate it though -Sean
    if (!mchatv1_is_connected(state->mchat))
    {
        status_line_urg_set(1, "Already Disconnected!");
        return -1;
    }
    char channel[2048];
    char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
    mchatv1_get_nickname(state->mchat, nick, MCHAT_LIMIT_MAX_NICKNAME_SIZE);
    mchatv1_get_channel(state->mchat, channel, 2048);
    mchatv1_send_message(state->mchat, "<Disconnected>");
    mchatv1_disconnect(state->mchat);
    status_line_urg_set(1, "Disconnected from %s", channel);
    status_line_set("Diconnected");
    chat_win_print(nick, "<Disconnected>");
    return 0;
}


const char *peerlist_string = "peerlist";
const char *peerlist_syntax = "\\PEERLIST";
const char *peerlist_help = "show a list of seen peers on a network";
int peerlist_function(ui_state_t *state, char *ptr)
{

    WINDOW *list_win = newwin(state->max_line - 2, state->max_col - 2, 1, 1);
    int x, y, bx, by;
    getmaxyx(list_win, y, x);
    getbegyx(list_win, by, bx);
    WINDOW *name_win = subwin(list_win, y - 3, x / 3 - 1, by + 1, bx + 2);
    WINDOW *channel_win = subwin(list_win, y - 3, x / 3 - 1, by + 1, bx + 2 + x / 3);
    WINDOW *time_win = subwin(list_win, y - 3, x / 3 - 3, by + 1, bx + 2 + x / 3 + x / 3);
    box(list_win, 0, 0);
    touchwin(list_win);
    wborder(name_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wborder(channel_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wrefresh(name_win);
    wrefresh(channel_win);
    wrefresh(time_win);
    wattron(name_win, A_BOLD);
    wattron(channel_win, A_BOLD);
    wattron(time_win, A_BOLD);
    mvwprintw(name_win, 0, 0, "Nicknames");
    mvwprintw(channel_win, 0, 0, "Connected Channel");
    mvwprintw(time_win, 0, 0, "Last Seen");
    wattroff(name_win, A_BOLD);
    wattroff(channel_win, A_BOLD);
    wattroff(time_win, A_BOLD);

    char *footer = "Press any key to continue...";
    mvwprintw(list_win, y - 2, (x / 2) - (strlen(footer) / 2), footer);
    // Print headings
    int ret = 0;
    int line = 0;
    while ((ret = wgetch(list_win)) == ERR)
    {
        if (line > 1)
        {
            for (int i = 1; i < line; i++)
            {
                wmove(name_win, i, 0);
                wclrtoeol(name_win);
                wmove(channel_win, i, 0);
                wclrtoeol(channel_win);
                wmove(time_win, i, 0);
                wclrtoeol(time_win);
            }
        }
        wborder(name_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
        wborder(channel_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
        wattron(name_win, A_BOLD);
        wattron(channel_win, A_BOLD);
        wattron(time_win, A_BOLD);
        mvwprintw(name_win, 0, 0, "Nicknames");
        mvwprintw(channel_win, 0, 0, "Connected Channel");
        mvwprintw(time_win, 0, 0, "Last Seen");
        wattroff(name_win, A_BOLD);
        wattroff(channel_win, A_BOLD);
        wattroff(time_win, A_BOLD);
        line = 1;
        mchat_peerlist_t *pl;
        if (mchatv1_get_peerlist(state->mchat, &pl))
        {
            for (int i = 0; i < mchatv1_peerlist_get_size(pl); i++)
            {
                char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
                char chan[MCHAT_LIMIT_MAX_CHANNEL_NAME_SIZE];
                long t;
                long now = time(NULL);
                int r;
                if ((r = mchatv1_peer_get_peer(pl, i, nick, chan, MCHAT_LIMIT_MAX_NICKNAME_SIZE, MCHAT_LIMIT_MAX_CHANNEL_NAME_SIZE, &t)))
                    mvwprintw(name_win, line, 0, "Error: %d", r);
                else
                {
                    unsigned char ip[16];
                    mchatv1_peer_get_source_address(pl, i, ip, 16);
                    mvwprintw(name_win, line, 0, "%s (@%s)", nick, ip);
                    mvwprintw(channel_win, line, 0, chan);
                    mvwprintw(time_win, line, 0, "%d seconds ago", now - (t/1000000));
                }
                line = getcury(name_win) + 1;
            }
        }
        mchatv1_peerlist_destroy(&pl);

        wrefresh(name_win);
        wrefresh(channel_win);
        wrefresh(time_win);
    }
    delwin(name_win);
    delwin(channel_win);
    delwin(time_win);
    delwin(list_win);
    werase(stdscr);
    refresh();
    return ret;
    return 0;
}


const char *chanlist_string = "chanlist";
const char *chanlist_syntax = "\\CHANLIST";
const char *chanlist_help = "show a list of channels discovered through CDSC messages";
int chanlist_function(ui_state_t *state, char *ptr)
{

    WINDOW *list_win = newwin(state->max_line - 2, state->max_col - 2, 1, 1);
    int x, y, bx, by;
    getmaxyx(list_win, y, x);
    getbegyx(list_win, by, bx);
    WINDOW *name_win = subwin(list_win, y - 3, x / 3 - 1, by + 1, bx + 2);
    WINDOW *ip_win = subwin(list_win, y - 3, x / 3 - 1, by + 1, bx + 2 + x / 3);
    WINDOW *port_win = subwin(list_win, y - 3, x / 3 - 3, by + 1, bx + 2 + x / 3 + x / 3);
    box(list_win, 0, 0);
    touchwin(list_win);
    wborder(name_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wborder(ip_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
    wrefresh(name_win);
    wrefresh(ip_win);
    wrefresh(port_win);
    wattron(name_win, A_BOLD);
    wattron(ip_win, A_BOLD);
    wattron(port_win, A_BOLD);
    mvwprintw(name_win, 0, 0, "Nicknames");
    mvwprintw(ip_win, 0, 0, "Connected Channel");
    mvwprintw(port_win, 0, 0, "Last Seen");
    wattroff(name_win, A_BOLD);
    wattroff(ip_win, A_BOLD);
    wattroff(port_win, A_BOLD);

    char *footer = "Press any key to continue...";
    mvwprintw(list_win, y - 2, (x / 2) - (strlen(footer) / 2), footer);
    // Print headings
    int ret = 0;
    int line = 0;
    while ((ret = wgetch(list_win)) == ERR)
    {
        if (line > 1)
        {
            for (int i = 1; i < line; i++)
            {
                wmove(name_win, i, 0);
                wclrtoeol(name_win);
                wmove(ip_win, i, 0);
                wclrtoeol(ip_win);
                wmove(port_win, i, 0);
                wclrtoeol(port_win);
            }
        }
        wborder(name_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
        wborder(ip_win, ' ', 0, ' ', ' ', ' ', ' ', ' ',' ');
        wattron(name_win, A_BOLD);
        wattron(ip_win, A_BOLD);
        wattron(port_win, A_BOLD);
        mvwprintw(name_win, 0, 0, "Nicknames");
        mvwprintw(ip_win, 0, 0, "Connected Channel");
        mvwprintw(port_win, 0, 0, "Last Seen");
        wattroff(name_win, A_BOLD);
        wattroff(ip_win, A_BOLD);
        wattroff(port_win, A_BOLD);
        line = 1;
        mchat_peerlist_t *pl;
        if (mchatv1_get_peerlist(state->mchat, &pl))
        {
            for (int i = 0; i < mchatv1_peerlist_get_size(pl); i++)
            {
                char nick[MCHAT_LIMIT_MAX_NICKNAME_SIZE];
                char chan[MCHAT_LIMIT_MAX_CHANNEL_NAME_SIZE];
                long t;
                long now = time(NULL);
                int r;
                if ((r = mchatv1_peer_get_peer(pl, i, nick, chan, MCHAT_LIMIT_MAX_NICKNAME_SIZE, MCHAT_LIMIT_MAX_CHANNEL_NAME_SIZE, &t)))
                    mvwprintw(name_win, line, 0, "Error: %d", r);
                else
                {
                    unsigned char ip[16];
                    mchatv1_peer_get_source_address(pl, i, ip, 16);
                    mvwprintw(name_win, line, 0, "%s (@%s)", nick, ip);
                    mvwprintw(ip_win, line, 0, chan);
                    mvwprintw(port_win, line, 0, "%d seconds ago", now - (t/1000000));
                }
                line = getcury(name_win) + 1;
            }
        }
        mchatv1_peerlist_destroy(&pl);

        wrefresh(name_win);
        wrefresh(ip_win);
        wrefresh(port_win);
    }
    delwin(name_win);
    delwin(ip_win);
    delwin(port_win);
    delwin(list_win);
    werase(stdscr);
    refresh();
    return ret;
    return 0;
}

void load_builtin_cmds(ui_state_t *state)
{
    add_cmd(help_string, help_syntax, help_help, help_function);
    add_cmd(quit_string, quit_syntax, quit_help, quit_function);
    add_cmd(clear_string, clear_syntax, clear_help, clear_function);
    add_cmd(nick_string, nick_syntax, nick_help, nick_function);
    add_cmd(list_string, list_syntax, list_help, list_function);
    add_cmd(connect_string, connect_syntax, connect_help, connect_function);
    add_cmd(disconnect_string, disconnect_syntax, disconnect_help, disconnect_function);
    add_cmd(peerlist_string, peerlist_syntax, peerlist_help, peerlist_function);
}
