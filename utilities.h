//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_UTILITIES_H
#define TINY_LINUX_SHELL_UTILITIES_H

//#define DEBUG       // Uncomment for debugging printouts.

#include "tsh_helper.h"
#include <string.h>
#include <stdarg.h>



/* Restores the signals (...) to their default behaviors. */
void restore_signal_defaults(int argc, ...);

/* Creates a signal set of the signals passed in the arguments */
sigset_t create_mask(int argc, ...);


/* Helper function for signal safe output messages */
void printMsg(int jid, pid_t pid, int sig);

/* Converts cmd jid arguments to integers */
int cmdjid_to_int(char* cmdjid);

#endif //TINY_LINUX_SHELL_UTILITIES_H
