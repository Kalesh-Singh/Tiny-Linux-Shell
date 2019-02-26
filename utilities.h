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
extern sigset_t job_control_mask;   // Signal set of the job control signals.
extern int in_fd;                   // Input file descriptor.
extern int out_fd;                  // Output file descriptor.

/*
 * Restores signals to their default behaviors.
 *
 * Allows specifying signals that already have their default behavior,
 * in which case no action is taken for that signal.
 *
 * @param argc an integer specifying the number of signals the function will take.
 * @param ... a comma separated list of integers that specify the signals that will be restored to default behavior.
 * @return void
 */
void restore_signal_defaults(int argc, ...);

/*
 * Creates a signal set of the signals passed in the arguments.
 *
 * @param argc an integer  specifying the number of signals the function will take.
 * @param ... a comma separated list of integers that specify which signals will be int the mask.
 * @return a signal mask, of type sigset_t, containing the signals specified.
 */
sigset_t create_mask(int argc, ...);


/* Prints a message to the shell when a job is terminated or stopped by a signal.
 *
 * This function is signal safe and is intended for use in a signal handler.
 *
 * @param jid the jod id of the job that was terminated or stopped.
 * @param pid the process id of the job that was terminated or stopped.
 * @param sig expected to be either SIGINT or SIGTSTP.
 * @return void.
 */
void printMsg(int jid, pid_t pid, int sig);

/*
 * Converts a job id specified on the command line in the form of %jid to an integer.
 *
 * @param cmdjid the job id as specified on the command line i.e including the % symbol.
 * @return an integer equivalent of the command line job id.
 */
int cmdjid_to_int(char* cmdjid);

/*
 * Redirects I/O from a file descriptor to another file.
 *
 * For output redirection if file does not exists it will be created.
 * However, file must exists already for input redirection.
 *
 * @param from expected to be either STDOUT_FILENO or STDIN_FILENO.
 * @param to the name of the file that the descriptor will be redirected to.
 */
void redirect_io(int from, char* to);

/*
 * Sets the file descriptor for input to STDIN_FILENO and
 * the file descriptor for output to STDOUT_FILENO.
 */
void set_std_io(void);

#endif //TINY_LINUX_SHELL_UTILITIES_H
