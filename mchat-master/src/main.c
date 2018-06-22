#include <stdio.h>
#include "curses_ui.h"

int main(int argc, char *argv[])
{
	ui_init(NULL);
	ui_run();
	ui_destroy();
	return 0;
}
