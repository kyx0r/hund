#ifndef PROMPT_H
#define PROMPT_H

#include <ncurses.h>
#include <panel.h>
//#include <form.h>
#include <syslog.h>
#include <string.h>

int prompt(char prompt[], size_t bufsize, char buf[bufsize]);

#endif
