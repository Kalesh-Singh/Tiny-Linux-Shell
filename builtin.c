//
// Created by kalesh on 2/19/19.
//

#include "builtin.h"

void quit() {
    exit(0);
}

void jobs() {
    change_signal_mask(SIG_BLOCK);
    listjobs(job_list, STDOUT_FILENO);
    change_signal_mask(SIG_UNBLOCK);
}

void bg(struct cmdline_tokens *tokens) {
    int jid = cmdjid_to_int(tokens->argv[1]);
    change_signal_mask(SIG_BLOCK);
    struct job_t *job = getjobjid(job_list, jid);
    printf("[%d] (%d) %s\n", jid, job->pid, job->cmdline);
    if (job != NULL &&job->state == ST) {
        job->state = BG;
        Kill(job->pid, SIGCONT);
    }
    change_signal_mask(SIG_UNBLOCK);
}

void fg(struct cmdline_tokens *tokens) {
    int jid = cmdjid_to_int(tokens->argv[1]);
    change_signal_mask(SIG_BLOCK);
    struct job_t *job = getjobjid(job_list, jid);
    if (job != NULL && (job->state == ST || job->state == BG)) {
        if (job->state == ST) {
            Kill(job->pid, SIGCONT);
        }

        job->state = FG;

        sigset_t ourmask;
        Sigemptyset(&ourmask);
        Sigaddset(&ourmask, SIGUSR1);

        Sigsuspend(&ourmask);
    }
    change_signal_mask(SIG_UNBLOCK);
    return;
}
