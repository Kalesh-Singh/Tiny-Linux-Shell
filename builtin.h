//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_BUILTIN_H
#define TINY_LINUX_SHELL_BUILTIN_H

#include "tsh_helper.h"
#include "utilities.h"
#include <stdlib.h>

/*
 * Terminates the shell
 * @return void
 */
void quit();

/*
 * Lists the running and stopped background jobs.
 * @return void
 */
void jobs();

/*
 * Changes a stopped job to a running foreground job.
 * @param tokens from parsing the command line.
 * @return void
 */
void bg(struct cmdline_tokens *tokens);

/*
 * Changes a stopped job or running background job
 * into a running foreground job.
 * @param tokens from parsing the command line.
 * @return void
 */
void fg(struct cmdline_tokens *tokens);

#endif //TINY_LINUX_SHELL_BUILTIN_H
