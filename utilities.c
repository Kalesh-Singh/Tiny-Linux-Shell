//
// Created by kalesh on 2/19/19.
//

#include "utilities.h"

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

sigset_t change_signal_mask(int how) {
    sigset_t sig_set;
    sigset_t old_set;
    Sigemptyset(&sig_set);
    Sigaddset(&sig_set, SIGCHLD);
    Sigaddset(&sig_set, SIGINT);
    Sigaddset(&sig_set, SIGTSTP);
    Sigprocmask(how, &sig_set, &old_set);
    return old_set;
}

void restore_signal_defaults() {
    Signal(SIGCHLD, SIG_DFL);
    Signal(SIGINT, SIG_DFL);
    Signal(SIGTSTP, SIG_DFL);
}
