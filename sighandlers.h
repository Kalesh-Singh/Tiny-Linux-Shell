//
// Created by kalesh on 2/19/19.
//

#ifndef TINY_LINUX_SHELL_SIGHANDLERS_H
#define TINY_LINUX_SHELL_SIGHANDLERS_H

//#define DEBUG           // Uncomment for debugging printouts

#include "tsh_helper.h"
#include "utilities.h"

/*
 * Catches SIGCHLD signals from both foreground and background processes
 * launched by the shell.
 *
 * Upon receipt of a SIGCHLD signal the handler reaps all child processes
 * that have finished. or were terminated or stopped by a signal.
 *
 * If the child process was stopped, its state in the job list is updated,
 * and a message is printed in the shell to notify the user.
 *
 * If the child process was terminated by a signal it is deleted from the
 * job list, and a message is printed in the shell to notify the user.
 *
 * If the child process exited normally, it is deleted from the job list
 * but no notification message is printed.
 */
void sigchld_handler(int sig);

/*
 * Catches SIGTSTP signals and forwards them to the
 * process group of the current foreground job.
 *
 * If there is no current foreground job, no action is taken.
 */
void sigtstp_handler(int sig);

/*
 * Catches SIGINT signals and forwards them to the
 * process group of the current foreground job.
 *
 * If there is no current foreground job, no action is taken.
 */
void sigint_handler(int sig);

#endif //TINY_LINUX_SHELL_SIGHANDLERS_H
