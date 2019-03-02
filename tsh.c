
/* 
 * tsh - A tiny shell program with job control
 *
 * The shell supports the notion of job control, which
 * allows users to move jobs back and forth between background
 * and foreground, and to change the process state (running,
 * stopped, terminated) of the processes in a job.
 *
 * Typing ctrl-c causes a SIGINT signal to be delivered to each
 * process in the foreground job. The default action for SIGINT
 * is to terminate the process.
 *
 * Typing ctrl-z causes a SIGTSTP signal to be delivered to each
 * process in the foreground job. The default action for SIGTSTP
 * is to place a process in the stopped state, where it remains
 * until it is awakened by th receipt of a SIGCONT signal.
 *
 * The shell also provides various built-in commands that support
 * job control:
 *
 * - quit     : terminates the shell.
 * - jobs     : Lists the running and stopped background jobs.
 * - bg <job> : Change a stopped job into a running foreground job.
 * - fg <job> : Change a stopped job or running background job
 *              into a running foreground job.
 *
 * The shell also supports the notion of I/O redirection which
 * allows users to redirect stdin and stdout to disk files.
 *
 * For example:
 *
 * tsh> /bin/ls > foo  - redirects the output of ls to a file
 *                       called foo.
 * tsh> /bin/cat < foo - displays the contents of file foo on
 *                       stdout.
 *
 * Each job can be identified by either a process ID (PID) or a
 * job ID (JID), which is a positive integer assigned by tsh.
 * JIDs should be denoted on the command line by the prefix ‘%’.
 *
 * For example, “%5” denotes JID 5, and “5” denotes PID 5.
 */

#include "tsh_helper.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
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


/* Global variables */
sigset_t job_control_mask;      // Signal set of the job control signals.
int in_fd = STDIN_FILENO;       // Input file descriptor.
int out_fd = STDOUT_FILENO;     // Output file descriptor.

/* Function prototypes */
void eval(const char *cmdline);
void run(const char *cmdline, struct cmdline_tokens *token, parseline_return parse_result);

/* Signal handlers */
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Built-in commands */
void quit();
void jobs();
void bg(struct cmdline_tokens *tokens);
void fg(struct cmdline_tokens *tokens);

/* Utility functions */
void restore_signal_defaults(int argc, ...);
sigset_t create_mask(int argc, ...);
void printMsg(int jid, pid_t pid, int sig);
int cmdjid_to_int(char* cmdjid);
void redirect_io(int from, char* to);
void set_std_io(void);

/*
 *  Creates the job control mask and registers the signal handlers, then
 *  prints the shell prompt "tsh>" and waits for the user input in a loop.
 *
 *  @return the exit status code of the shell.
 */
int main(int argc, char **argv) {
    job_control_mask = create_mask(3, SIGINT, SIGCHLD, SIGTSTP);

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

/*
 * Parses the command line and executes the appropriate
 * built-in command, or creates an appropriate foreground
 * or background job.
 *
 * @param cmdline the command line as entered in the shell.
 * @return void.
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;

    // Parse command line
    parse_result = parseline(cmdline, &token);

    switch (parse_result) {
        case PARSELINE_EMPTY:
        case PARSELINE_ERROR:
            return;
        case PARSELINE_BG:
        case PARSELINE_FG:
            // I/O Redirection for built-in commands.
            if (token.builtin != BUILTIN_NONE) {
                if (token.infile) {
                    redirect_io(STDIN_FILENO, token.infile);
                }
                if (token.outfile) {
                    redirect_io(STDOUT_FILENO, token.outfile);
                }
            }

            switch (token.builtin) {
                case BUILTIN_QUIT:
                    quit();
                case BUILTIN_FG:
                    fg(&token);
                    break;
                case BUILTIN_BG:
                    bg(&token);
                    break;
                case BUILTIN_JOBS:
                    jobs();
                    break;
                case BUILTIN_NONE:
                    run(cmdline, &token, parse_result);
                    break;
            }

            // Reset standard I/O for the shell
            if (token.builtin != BUILTIN_NONE) {
                set_std_io();
            }
            break;
    }
    return;
}

/*
 * Forks and executes a child process based on the parsed command line.
 *
 * Suspends the shell, if job created is a foreground job, until the foreground job
 * is stopped, terminated or exits.
 *
 * If the job created runs in the background, a notification is printed containing
 * the job id, process id and the command line of the background job.
 *
 * Handles I/O redirection for created jobs if necessary.
 *
 * @param cmdline the command line entered in the shell.
 * @param token the tokens parsed from the command line.
 * @parse_result the parse result returned from the parseline call.
 * @return void
 *
 */
void run(const char *cmdline, struct cmdline_tokens *token, parseline_return parse_result) {
    sigset_t old_mask;       // Mask with SIGINT, SIGTSTP and SIGCHLD Unblocked
    Sigprocmask(SIG_BLOCK, &job_control_mask, &old_mask);

    pid_t pid = Fork();
    if (pid == 0) {         // Child process
        Setpgid(0, 0);      // Place child process in a new process group.

        // Redirect input
        if (token->infile) {
            redirect_io(STDIN_FILENO, token->infile);
        }

        // Redirect output
        if (token->outfile) {
            redirect_io(STDOUT_FILENO, token->outfile);
        }

        Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
        restore_signal_defaults(3, SIGINT, SIGTSTP, SIGCHLD);
        Execve(token->argv[0], token->argv, environ);

    } else {                // Parent process

        if (parse_result == PARSELINE_BG) {
            addjob(job_list, pid, BG, cmdline);
            int jid = pid2jid(job_list, pid);
            printf("[%d] (%d) %s\n", jid, pid, cmdline);
        } else if (parse_result == PARSELINE_FG) {
            addjob(job_list, pid, FG, cmdline);
            while (fgpid(job_list)) {   // While there is a foreground job
                Sigsuspend(&old_mask);
            }
        }

        Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
    }
}

/*****************
 * Signal handlers
 *****************/
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

        struct job_t *job = getjobpid(job_list, pid);

        if (WIFSIGNALED(wstatus)) {         // If child terminated by a signal
            int sig = WTERMSIG(wstatus);
            printMsg(job->jid, job->pid, sig);
            deletejob(job_list, pid);
        } else if (WIFEXITED(wstatus)) {    // If child exited normally by returning from main or calling exit()
            deletejob(job_list, pid);
        } else if (WIFSTOPPED(wstatus)) {   // If child stopped by a signal
            int sig = WSTOPSIG(wstatus);
            job->state = ST;
            printMsg(job->jid, job->pid, sig);
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

/*******************
 * Built-in commands
 ******************/
/*
 * Terminates the shell
 * @return void
 */
void quit() {
    exit(0);
}

/*
 * Lists the running and stopped background jobs.
 * @return void
 */
void jobs() {
    Sigprocmask(SIG_BLOCK, &job_control_mask, NULL);
    listjobs(job_list, STDOUT_FILENO);
    Sigprocmask(SIG_UNBLOCK, &job_control_mask, NULL);
}

/*
 * Changes a stopped job to a running foreground job.
 * @param tokens from parsing the command line.
 * @return void
 */
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

/*
 * Changes a stopped job or running background job
 * into a running foreground job.
 * @param tokens from parsing the command line.
 * @return void
 */
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

/*******************
 * Utility functions
 ******************/
/*
 * Restores signals to their default behaviors.
 *
 * Allows specifying signals that already have their default behavior,
 * in which case no action is taken for that signal.
 *
 * @param argc an integer specifying the number of signals the function will take.
 * @param ... a comma separated list of integers that specify the signals that will
 *            be restored to default behavior.
 * @return void
 */
void restore_signal_defaults(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
    int i;
    for (i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
        Signal(sig, SIG_DFL);
    }
    va_end(sigs);
}

/*
 * Creates a signal set of the signals passed in the arguments.
 *
 * @param argc an integer  specifying the number of signals the function will take.
 * @param ... a comma separated list of integers that specify which signals
 *            will be in the mask.
 * @return a signal mask, of type sigset_t, containing the signals specified.
 */
sigset_t create_mask(int argc, ...) {
    va_list sigs;
    va_start(sigs, argc);
    sigset_t mask;
    Sigemptyset(&mask);
    int i;
    for (i = 0; i < argc; i++) {
        int sig = va_arg(sigs, int);
        Sigaddset(&mask, sig);
    }
    va_end(sigs);
    return mask;
}

/* Prints a message to the shell when a job is terminated or stopped by a signal.
 *
 * This function is signal safe and is intended for use in a signal handler.
 *
 * @param jid the jod id of the job that was terminated or stopped.
 * @param pid the process id of the job that was terminated or stopped.
 * @param sig expected to be either SIGINT or SIGTSTP.
 * @return void.
 */
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

/*
 * Converts a job id specified on the command line in the form of %jid to an integer.
 *
 * @param cmdjid the job id as specified on the command line i.e including the % symbol.
 * @return an integer equivalent of the command line job id.
 */
int cmdjid_to_int(char *cmdjid) {
    char *jid_str = cmdjid + 1;
    char *ep;
    return strtol(jid_str, &ep, 10);
}

/*
 * Redirects I/O from a file descriptor to another file.
 *
 * For output redirection if file does not exists it will be created.
 * However, file must exists already for input redirection.
 *
 * @param from expected to be either STDOUT_FILENO or STDIN_FILENO.
 * @param to the name of the file that the descriptor will be redirected to.
 */
void redirect_io(int from, char* to) {
    int file;

    switch (from) {
        case STDIN_FILENO:
            if ((file = open(to, O_RDONLY)) < 0) {
                unix_error("open error");
                exit(1);
            }
            in_fd = file;
            break;
        case STDOUT_FILENO:
            if ((file = open(to, O_CREAT | O_WRONLY)) < 0) {
                unix_error("open error");
                exit(1);
            }
            out_fd = file;
            break;
    }

    Dup2(file, from);
}

/*
 * Sets the file descriptor for input to STDIN_FILENO and
 * the file descriptor for output to STDOUT_FILENO.
 */
void set_std_io(void) {
    if (in_fd != STDIN_FILENO) {
        Dup2(in_fd, STDIN_FILENO);
        close(in_fd);
        in_fd = STDIN_FILENO;
    }
    if (out_fd != STDOUT_FILENO) {
        Dup2(in_fd, STDOUT_FILENO);
        close(out_fd);
        out_fd = STDOUT_FILENO;
    }
}
