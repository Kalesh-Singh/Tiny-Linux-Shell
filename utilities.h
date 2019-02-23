//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_UTILITIES_H
#define TINY_LINUX_SHELL_UTILITIES_H

//#define DEBUG       // Uncomment for debugging printouts.

#include "tsh_helper.h"
#include <string.h>
#include <stdarg.h>

/* Blocks and unblocks the signal set:
 * { SIGCHLD, SIGINT, SIGTSTP }
 *
 * how - expects one of the following arguments.
 *  SIG_BLOCK
 *      The set of blocked signals is the union of the current
 *      set and the set argument.
 *  SIG_UNBLOCK
 *      The signals in set are removed from the current set of
 *      blocked signals.  It is permissible to attempt to unblock
 *      a signal which is  not blocked.
 *  Return Value:
 *      The old mask. */
sigset_t change_signal_mask(int how);

/* Restores the signals:
 *  {SIGCHLD, SIGINT, SIGTSTP, SIGQUIT }
 *  to their default behaviors. */
void restore_signal_defaults(int argc, ...);

void printMsg(int jid, pid_t pid, int sig);     // Helper for signal safe output.

int cmdjid_to_int(char* cmdjid); // Converts cmd jid arg to int.

sigset_t create_mask(int argc, ...);

#endif //TINY_LINUX_SHELL_UTILITIES_H
