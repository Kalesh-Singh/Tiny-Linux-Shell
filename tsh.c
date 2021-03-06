
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
#include "utilities.h"
#include "sighandlers.h"
#include "builtins.h"

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
void run(const char *cmdline, struct cmdline_tokens *token, parseline_return parse_result);

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
