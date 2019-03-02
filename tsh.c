
/* 
 * tsh - A tiny shell program with job control
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.>
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
 * <Write main's function header documentation. What does main do?>
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
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
 *
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

            // Reset standard I/O for built-in commands.
            if (token.builtin != BUILTIN_NONE) {
                set_std_io();
            }
    }

    return;
}

/*
 * Forks and executes a child process of the shell based on the parsed command line.
 *
 * Suspends the shell, if job created is a foreground job, until the foreground job
 * is stopped, terminated or exits.
 *
 * If the job created runs in the background, a notification is printed containing
 * the job id, process id and the command line of the background job.
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
