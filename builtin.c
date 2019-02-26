//
// Created by kalesh on 2/19/19.
//

#include "builtin.h"

void quit() {
    exit(0);
}

void jobs() {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    listjobs(job_list, STDOUT_FILENO);
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
}

void bg(struct cmdline_tokens *tokens) {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    int jid = cmdjid_to_int(tokens->argv[1]);
    struct job_t *job = getjobjid(job_list, jid);
    printf("[%d] (%d) %s\n", jid, job->pid, job->cmdline);
    if (job != NULL &&job->state == ST) {
        job->state = BG;
        Kill(-job->pid, SIGCONT);
    }
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
}

void fg(struct cmdline_tokens *tokens) {
    sigset_t old_mask;      // Has SIGINT, SIGTSTP and SIGCHLD Unblocked
    Sigprocmask(SIG_BLOCK, &job_control_mask, &old_mask);

    int jid = cmdjid_to_int(tokens->argv[1]);
    struct job_t *job = getjobjid(job_list, jid);

    if (job != NULL && (job->state == ST || job->state == BG)) {
        if (job->state == ST) {
            Kill(-job->pid, SIGCONT);
        }

        job->state = FG;

         while (fgpid(job_list)) {
             Sigsuspend(&old_mask);
         }
    }

    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    return;
}
