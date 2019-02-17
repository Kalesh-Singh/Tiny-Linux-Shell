
/* 
 * tsh - A tiny shell program with job control
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.>
 */

#include "tsh_helper.h"

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);

void sigtstp_handler(int sig);

void sigint_handler(int sig);

void sigquit_handler(int sig);

// ------- Signals ----------
/* Blocks and unblocks the signal set:
 * { SIGCHLD, SIGINT, SIGTSTP }
 *
 * how - expects one of the following arguments.
 *  SIG_BLOCK
 *      The set of blocked signals is the union of the current
 *      set and the set argument.
 *  SIG_UNBLOCK
 *      The signals in set are removed from the current set of
 *      blocked signals.  It is permissible to attempt to unblock
 *      a signal which is  not blocked.
 *  Return Value:
 *      The old mask. */
sigset_t change_signal_mask(int how);

/* Restores the signals:
 *  {SIGCHLD, SIGINT, SIGTSTP, SIGQUIT }
 *  to their default behaviors. */
void restore_signal_defaults();
//----------------------------------------

//------- Built In Commands --------
void quit();        // Terminates the shell process
void jobs();
//----------------------------------

//-------Foreground and Background Jobs---------
void run(const char *cmdline, struct cmdline_tokens *token, parseline_return parse_result);

void run_in_fg(const char *cmdline, struct cmdline_tokens *token);   // Creates and runs a process in the foreground.
void run_in_bg(const char *cmdline, struct cmdline_tokens *token);   // Creates and runs a process in the background.
void printMsg(int jid, pid_t pid, int sig);
//---------------------------------------------

/*
 * <Write main's function header documentation. What does main do?>
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE_TSH];  // Cmdline for fgets
    bool emit_prompt = true;    // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    Dup2(STDOUT_FILENO, STDERR_FILENO);

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
            case 'h':                   // Prints help message
                usage();
                break;
            case 'v':                   // Emits additional diagnostic info
                verbose = true;
                break;
            case 'p':                   // Disables prompt printing
                emit_prompt = false;
                break;
            default:
                usage();
        }
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles ctrl-c
    Signal(SIGTSTP, sigtstp_handler);  // Handles ctrl-z
    Signal(SIGCHLD, sigchld_handler);  // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Initialize the job list
    initjobs(job_list);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            app_error("fgets error");
        }

        if (feof(stdin)) {
            // End of file (ctrl-d)
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }

        // Remove the trailing newline
        cmdline[strlen(cmdline) - 1] = '\0';

        // Evaluate the command line
        eval(cmdline);

        fflush(stdout);
    }

    return -1; // control never reaches here
}


/* Handy guide for eval:
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg),
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.
 * Note: each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */

/* 
 * <What does eval do?>
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;
    sigset_t ourmask;
    // TODO: remove the line below! It's only here to keep the compiler happy
    Sigemptyset(&ourmask);

    // Parse command line
    parse_result = parseline(cmdline, &token);

    switch (parse_result) {
        case PARSELINE_EMPTY:
        case PARSELINE_ERROR:
            return;
        case PARSELINE_BG:
            run(cmdline, &token, parse_result);
            break;
        case PARSELINE_FG:
            switch (token.builtin) {
                case BUILTIN_QUIT:
                    quit();
                case BUILTIN_FG:
                    break;
                case BUILTIN_BG:
                    break;
                case BUILTIN_JOBS:
                    jobs();
                    break;
                case BUILTIN_NONE:
                    run(cmdline, &token, parse_result);
                    break;
            }
    }

    return;
}

void run(const char *cmdline, struct cmdline_tokens *token, parseline_return parse_result) {
    sigset_t old_mask = change_signal_mask(SIG_BLOCK);
    pid_t pid = Fork();
    if (pid == 0) {
        Setpgid(0, 0);
        change_signal_mask(SIG_UNBLOCK);
        restore_signal_defaults();
        Execve(token->argv[0], token->argv, environ);
    } else {
        job_state state;
        if (parse_result == PARSELINE_BG) {
            state = BG;
            addjob(job_list, pid, state, cmdline);
            int jid = pid2jid(job_list, pid);
            printf("[%d] (%d) %s\n", jid, pid, cmdline);
        } else if (parse_result == PARSELINE_FG) {
            state = FG;
            addjob(job_list, pid, state, cmdline);
            Sigsuspend(&old_mask);
        }
        // NOTE: The signals must be unblocked AFTER the call to sigsuspend
        // else the behavior is unpredictable.
        change_signal_mask(SIG_UNBLOCK);
    }
}

/*****************
 * Signal handlers
 *****************/

/* 
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) {
    int wstatus;
    pid_t pid = Waitpid(-1, &wstatus, WUNTRACED|WNOHANG);

    change_signal_mask(SIG_BLOCK);
    if (WIFEXITED(wstatus)) {               // If child exited.
        deletejob(job_list, pid);
    } else if (WIFSTOPPED(wstatus)) {       // If child stopped.
        struct job_t *stopped_job = getjobpid(job_list, pid);
        stopped_job->state = ST;
    }
    change_signal_mask(SIG_UNBLOCK);
    return;
}

/* 
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) {
    change_signal_mask(SIG_BLOCK);
    pid_t fg_pid = fgpid(job_list);
    if (fg_pid > 0) {
        int fg_jid = pid2jid(job_list, fg_pid);
        Kill(-fg_pid, SIGINT);
        printMsg(fg_jid, fg_pid, sig);
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
        int fg_jid = pid2jid(job_list, fg_pid);
        Kill(-fg_pid, SIGTSTP);
        printMsg(fg_jid, fg_pid, sig);
    }
    change_signal_mask(SIG_UNBLOCK);
    return;
}

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

/*****************
 Built-in Commands
 *****************/

void quit() {
    exit(0);
}

void jobs() {
    change_signal_mask(SIG_BLOCK);
    listjobs(job_list, STDOUT_FILENO);
    change_signal_mask(SIG_UNBLOCK);
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
    Signal(SIGQUIT, SIG_DFL);
}
