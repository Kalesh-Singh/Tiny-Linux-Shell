//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_SHELL_SIGHANDLERS_H
#define TINY_LINUX_SHELL_SHELL_SIGHANDLERS_H

#include "tsh_helper.h"
#include "utilities.h"

void sigchld_handler(int sig);

void sigtstp_handler(int sig);

void sigint_handler(int sig);

void sigquit_handler(int sig);

#endif //TINY_LINUX_SHELL_SHELL_SIGHANDLERS_H
