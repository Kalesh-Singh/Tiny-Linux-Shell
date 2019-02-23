//
// Created by kalesh on 2/19/19.
//

#include "sighandlers.h"

void debugPrint(int sig) {
    Sio_puts("\nHANDLER: ");
    switch (sig) {
        case 2:
            Sio_puts("sigint_handler");
            break;
        case 10:
            Sio_puts("sigusr1_handler");
            break;
        case 17:
            Sio_puts("sigchld_handler");
            break;
        case 20:
            Sio_puts("sigtstp_handler");
            break;
    }
    Sio_puts(" called.\n\n");
}

/*
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) {
#ifdef DEBUG
    debugPrint(sig);
#endif
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    int wstatus;
    pid_t pid = Waitpid(-1, &wstatus, WUNTRACED);

    if (pid > 0) {
        pid_t fg_pid = fgpid(job_list);     // Signal sigsuspend to return if the fg process changed state.
        if (WIFSIGNALED(wstatus)) {         // Child terminated by a signal
            int sig = WTERMSIG(wstatus);
            int jid = pid2jid(job_list, pid);
            printMsg(jid, pid, sig);
            deletejob(job_list, pid);
        }
        if (WIFEXITED(wstatus)) {               // If child exited normally by returning from main or calling exit()
            deletejob(job_list, pid);
        } else if (WIFSTOPPED(wstatus)) {       // If child stopped by a signal
            int sig = WSTOPSIG(wstatus);
            struct job_t *stopped_job = getjobpid(job_list, pid);
            stopped_job->state = ST;
            printMsg(stopped_job->jid, stopped_job->pid, sig);
        }
        if (WIFSIGNALED(wstatus) || WIFEXITED(wstatus) || WIFSTOPPED(wstatus)) {
            if (pid == fg_pid) {
                raise(SIGUSR1);
            }
        }
    }
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}

/*
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) {
#ifdef DEBUG
    debugPrint(sig);
#endif
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);

    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
#ifdef DEBUG
    else {
        Sio_puts("No fg job\n");
    }
#endif
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}

/*
 * <What does sigtstp_handler do?>
 */
void sigtstp_handler(int sig) {
#ifdef DEBUG
    debugPrint(sig);
#endif
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);

    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
#ifdef DEBUG
    else {
        Sio_puts("No fg job\n");
    }
#endif
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}


/*
 * <What does  the sigusr1_handler do?>
 */
void sigusr1_handler(int sig) {
#ifdef DEBUG
    debugPrint(sig);
#endif
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    fg_interrupt = 1;
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}