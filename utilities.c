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

int cmdjid_to_int(char* cmdjid) {
    char* jid_str = cmdjid + 1;
    char* ep;
    return strtol(jid_str, &ep, 10);
}

sigset_t create_mask(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
    sigset_t mask;
    Sigemptyset(&mask);
#ifdef DEBUG
    printf("Mask created: ");
#endif
    for (int i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
#ifdef DEBUG
        switch(sig) {
            case 2:
                printf("%10s", "SIGINT");
                break;
            case 20:
                printf("%10s", "SIGTSTP");
                break;
            case 17:
                printf("%10s", "SIGCHLD");
                break;
            case 10:
                printf("%10s", "SIGUSR1");
                break;
            default:
                printf("%10s", "UNKNOWN");
                break;
        }
#endif
        Sigaddset(&mask, sig);
    }
    va_end(sigs);
#ifdef DEBUG
    printf("\n\n");
#endif
    return mask;
}

void restore_signal_defaults(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
#ifdef DEBUG
    printf("Restored to defaults: ");
#endif
    for (int i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
 #ifdef DEBUG
        switch(sig) {
            case 2:
                printf("%10s", "SIGINT");
                break;
            case 20:
                printf("%10s", "SIGTSTP");
                break;
            case 17:
                printf("%10s", "SIGCHLD");
                break;
            case 10:
                printf("%10s", "SIGUSR1");
                break;
            default:
                printf("%10s", "UNKNOWN");
                break;
        }
 #endif
        Signal(sig, SIG_DFL);
    }
    va_end(sigs);
#ifdef DEBUG
    printf("\n\n");
#endif
}
