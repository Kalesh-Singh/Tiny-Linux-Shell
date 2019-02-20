//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_BUILTIN_H
#define TINY_LINUX_SHELL_BUILTIN_H

#include "tsh_helper.h"
#include "utilities.h"
#include <stdlib.h>
#include <string.h>

void quit();        // Terminates the shell process
void jobs();
void bg(struct cmdline_tokens *tokens);

#endif //TINY_LINUX_SHELL_BUILTIN_H
