//
// Created by kalesh on 2/19/19.
//

#include "sighandlers.h"

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
void sigchld_handler(int sig) {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    int wstatus;
    pid_t pid;

    while ((pid = waitpid(WAIT_ANY, &wstatus, WUNTRACED | WNOHANG)) > 0) {

        if (WIFSIGNALED(wstatus)) {         // If child terminated by a signal
            int sig = WTERMSIG(wstatus);
            int jid = pid2jid(job_list, pid);
            printMsg(jid, pid, sig);
            deletejob(job_list, pid);
        }
        if (WIFEXITED(wstatus)) {           // If child exited normally by returning from main or calling exit()
            deletejob(job_list, pid);
        } else if (WIFSTOPPED(wstatus)) {   // If child stopped by a signal
            int sig = WSTOPSIG(wstatus);
            struct job_t *stopped_job = getjobpid(job_list, pid);
            stopped_job->state = ST;
            printMsg(stopped_job->jid, stopped_job->pid, sig);
        }
    }

    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}

/*
 * Catches SIGTSTP signals and forwards them to the
 * process group of the current foreground job.
 *
 * If there is no current foreground job, no action is taken.
 */
void sigtstp_handler(int sig) {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);

    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}

/*
 * Catches SIGINT signals and forwards them to the
 * process group of the current foreground job.
 *
 * If there is no current foreground job, no action is taken.
 */
void sigint_handler(int sig) {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);

    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        Kill(-fg_pid, sig);
    }
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}
