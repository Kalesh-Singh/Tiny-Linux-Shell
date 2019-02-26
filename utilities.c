//
// Created by kalesh on 2/19/19.
//

#include "utilities.h"

/* Global variables */
int fg_interrupt = 0;           // Bool to check if fg job was interrupted by a signal or exited normally.
sigset_t job_control_mask;      // Signal set of the job control signals.
int in_fd = STDIN_FILENO;       // Input file descriptor.
int out_fd = STDOUT_FILENO;     // Output file descriptor.

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
void restore_signal_defaults(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
    int i;
    for (i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
        Signal(sig, SIG_DFL);
    }
    va_end(sigs);
}

/*
 * Creates a signal set of the signals passed in the arguments.
 *
 * @param argc an integer  specifying the number of signals the function will take.
 * @param ... a comma separated list of integers that specify which signals will be int the mask.
 * @return a signal mask, of type sigset_t, containing the signals specified.
 */
sigset_t create_mask(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
    sigset_t mask;
    Sigemptyset(&mask);
    int i;
    for (i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
        Sigaddset(&mask, sig);
    }
    va_end(sigs);
    return mask;
}

/* Prints a message to the shell when a job is terminated or stopped by a signal.
 *
 * This function is signal safe and is intended for use in a signal handler.
 *
 * @param jid the jod id of the job that was terminated or stopped.
 * @param pid the process id of the job that was terminated or stopped.
 * @param sig expected to be either SIGINT or SIGTSTP.
 * @return void.
 */
void printMsg(int jid, pid_t pid, int sig) {
    Sio_puts("Job [");
    Sio_putl(jid);
    Sio_puts("] (");
    Sio_putl(pid);
    Sio_puts(") ");
    if (sig == SIGINT) {
        Sio_puts("terminated");
    } else if (sig == SIGTSTP) {
        Sio_puts("stopped");
    }
    Sio_puts(" by signal ");
    Sio_putl(sig);
    Sio_puts("\n");
}

/*
 * Converts a job id specified on the command line in the form of %jid to an integer.
 *
 * @param cmdjid the job id as specified on the command line i.e including the % symbol.
 * @return an integer equivalent of the command line job id.
 */
int cmdjid_to_int(char *cmdjid) {
    char *jid_str = cmdjid + 1;
    char *ep;
    return strtol(jid_str, &ep, 10);
}

/*
 * Redirects I/O from a file descriptor to another file.
 *
 * For output redirection if file does not exists it will be created.
 * However, file must exists already for input redirection.
 *
 * @param from expected to be either STDOUT_FILENO or STDIN_FILENO.
 * @param to the name of the file that the descriptor will be redirected to.
 */
void redirect_io(int from, char* to) {
    int file;

    switch (from) {
        case STDIN_FILENO:
            file = open(to, O_RDONLY);
            break;
        case STDOUT_FILENO:
            file = open(to, O_CREAT | O_WRONLY);
            break;
    }

    if (file < 0) {
        unix_error("open error");
        exit(1);
    }

    switch (from) {
        case STDIN_FILENO:
            in_fd = file;
            break;
        case STDOUT_FILENO:
            out_fd = file;
            break;
    }

    Dup2(file, from);
}

/*
 * Sets the file descriptor for input to STDIN_FILENO and
 * the file descriptor for output to STDOUT_FILENO.
 */
void set_std_io(void) {
    if (in_fd != STDIN_FILENO) {
        Dup2(in_fd, STDIN_FILENO);
        close(in_fd);
        in_fd = STDIN_FILENO;
    }
    if (out_fd != STDOUT_FILENO) {
        Dup2(in_fd, STDOUT_FILENO);
        close(out_fd);
        out_fd = STDOUT_FILENO;
    }
}
