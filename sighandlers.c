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
 *
 * If the child process that changed state  was the current foreground process,
 * the handler raises a SIGUSR1 signal.
 */
void sigchld_handler(int sig) {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    int wstatus;
    pid_t pid;

    while (true) {

        pid = waitpid(WAIT_ANY, &wstatus, WUNTRACED | WNOHANG);

        if (pid < 0) {
            if (errno != ECHILD) {              // If the error was not caused by no more child processes
                unix_error("waitpid error");    // print the error message.
            }
            break;
        }

        if (pid == 0) {
            break;
        }

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

/*
 * Catches SIGUSR1 signals.
 *
 * Upon receipt of a SIGUSR1 signal sets fg_interrupt to 1.
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