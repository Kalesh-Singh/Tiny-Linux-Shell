//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_UTILITIES_H
#define TINY_LINUX_SHELL_UTILITIES_H

//#define DEBUG       // Uncomment for debugging printouts.

#include "tsh_helper.h"
#include <string.h>
#include <stdarg.h>

/* Global variables */
extern int fg_interrupt;            // Bool to check if fg job was interrupted by a signal.
extern sigset_t job_control_mask;   // Signal set of the job control signals.
extern int in_fd;                   // Input file descriptor.
extern int out_fd;                  // Output file descriptor.

/* Restores the signals (...) to their default behaviors. */
void restore_signal_defaults(int argc, ...);

/* Creates a signal set of the signals passed in the arguments */
sigset_t create_mask(int argc, ...);


/* Helper function for signal safe output messages */
void printMsg(int jid, pid_t pid, int sig);

/* Converts cmd jid arguments to integers */
int cmdjid_to_int(char* cmdjid);

/* Redirect IO from the the fd (from) to the file named (to) */
void redirect_io(int from, char* to);

/* Sets io to stdin and stdout */
void set_std_io(void);

#endif //TINY_LINUX_SHELL_UTILITIES_H
