
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
 *      a signal which is  not blocked. */
void change_signal_mask(int how);

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
void run_in_fg(const char *cmdline, struct cmdline_tokens *token);   // Creates and runs a process in the foreground.
void run_in_bg(const char *cmdline, struct cmdline_tokens *token);   // Creates and runs a process in the background.
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
            run_in_bg(cmdline, &token);
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
                    run_in_fg(cmdline, &token);
                    break;
            }
    }

    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) {
    return;
}

/* 
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) {
    change_signal_mask(SIG_BLOCK);
    pid_t fg_pid = fgpid(job_list);
    struct job_t *job = getjobpid(job_list, fg_pid);
    change_signal_mask(SIG_UNBLOCK);
    Kill(-fg_pid, SIGINT);
    char *f_str = "Job [%d] (%d) terminated by signal %d\n";
    char str[MAXLINE];
    sprintf(str, f_str, job->jid, job->pid, sig);
    Fputs(str, stdout);
    return;
}

/*
 * <What does sigtstp_handler do?>
 */
void sigtstp_handler(int sig) {
    return;
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

void run_in_fg(const char *cmdline, struct cmdline_tokens *token) {
    change_signal_mask(SIG_BLOCK);
    pid_t pid = Fork();

    if (pid == 0) {
        Setpgid(0, 0);
        change_signal_mask(SIG_UNBLOCK);
        restore_signal_defaults();
        Execve(token->argv[0], token->argv, environ);
    } else {
        addjob(job_list, pid, FG, cmdline);
        change_signal_mask(SIG_UNBLOCK);
        Waitpid(pid, NULL, WUNTRACED);
        change_signal_mask(SIG_BLOCK);
        deletejob(job_list, pid);
        change_signal_mask(SIG_UNBLOCK);
    }
}

void run_in_bg(const char *cmdline, struct cmdline_tokens *token) {
    change_signal_mask(SIG_BLOCK);
    pid_t pid = Fork();

    if (pid == 0) {
        Setpgid(0, 0);
        change_signal_mask(SIG_UNBLOCK);
        restore_signal_defaults();
        Execve(token->argv[0], token->argv, environ);
    } else {
        addjob(job_list, pid, BG, cmdline);
        struct job_t *job = getjobpid(job_list, pid);
        change_signal_mask(SIG_UNBLOCK);
        int jid = job->jid;

        char *f_str = "[%d] (%d) %s\n";
        char str[MAXLINE];
        sprintf(str, f_str, jid, pid, cmdline);
        Fputs(str, stdout);

        Waitpid(pid, NULL, WNOHANG);
    }
}

void change_signal_mask(int how) {
    sigset_t sig_set;
    Sigemptyset(&sig_set);
    Sigaddset(&sig_set, SIGCHLD);
    Sigaddset(&sig_set, SIGINT);
    Sigaddset(&sig_set, SIGTSTP);
    Sigprocmask(how, &sig_set, NULL);
}

void restore_signal_defaults() {
    Signal(SIGCHLD, SIG_DFL);
    Signal(SIGINT, SIG_DFL);
    Signal(SIGTSTP, SIG_DFL);
    Signal(SIGQUIT, SIG_DFL);
}
