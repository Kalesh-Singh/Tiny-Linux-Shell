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
    char* jid_str = tokens->argv[1] + 1;
    char* ep;
    int jid = strtol(jid_str, &ep, 10);
    change_signal_mask(SIG_BLOCK);
    struct job_t* job = getjobjid(job_list, jid);
    printf("[%d] (%d) %s\n", jid, job->pid, job->cmdline);
    if (job->state == ST) {
        job->state = BG;
        Kill(job->pid, SIGCONT);
    }
    change_signal_mask(SIG_UNBLOCK);
}
