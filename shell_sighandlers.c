//
// Created by kalesh on 2/19/19.
//

#include "shell_sighandlers.h"

/*
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) {
    int wstatus;
    pid_t pid = Waitpid(-1, &wstatus, WUNTRACED | WNOHANG);

    if (pid > 0) {
        change_signal_mask(SIG_BLOCK);
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
            pid_t fg_pid = fgpid(job_list);     // Signal sigsuspend to return if the fg process changed state.
            if (pid == fg_pid) {
                raise(SIGUSR1);
            }
        }
        change_signal_mask(SIG_UNBLOCK);
    }
    return;
}

/*
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) {
    change_signal_mask(SIG_BLOCK);
    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
    change_signal_mask(SIG_UNBLOCK);
    return;
}

/*
 * <What does sigtstp_handler do?>
 */
void sigtstp_handler(int sig) {
    change_signal_mask(SIG_BLOCK);
    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
    change_signal_mask(SIG_UNBLOCK);
    return;
}


/*
 * <What does  the sigusr1_handler do?>
 */
void sigusr1_handler(int sig) {
    return;
}