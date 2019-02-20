//
// Created by kalesh on 2/19/19.
//

#include "shell_sighandlers.h"

/*
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) {
    int wstatus;
    pid_t pid = Waitpid(-1, &wstatus, WUNTRACED|WNOHANG);

    if (pid > 0) {
        change_signal_mask(SIG_BLOCK);
        if (WIFSIGNALED(wstatus)) {
            int sig = WTERMSIG(wstatus);
            if (sig == SIGINT) {
                int jid = pid2jid(job_list, pid);
                printMsg(jid, pid, sig);
                deletejob(job_list, pid);
            }
        }
        if (WIFEXITED(wstatus)) {               // If child exited.
            deletejob(job_list, pid);
        } else if (WIFSTOPPED(wstatus)) {       // If child stopped.
            struct job_t *stopped_job = getjobpid(job_list, pid);
            stopped_job->state = ST;
            printMsg(stopped_job->jid, stopped_job->pid, SIGTSTP);
        }
        pid_t fg_pid = fgpid(job_list);
        if (pid == fg_pid) {
            raise(SIGUSR1);
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
        Kill(-fg_pid, SIGINT);
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
        Kill(-fg_pid, SIGTSTP);
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