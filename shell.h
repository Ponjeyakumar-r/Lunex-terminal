#ifndef SHELL_H
#define SHELL_H

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <termios.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_JOBS 32
#define MAX_PATH 256
#define MAX_HISTORY 100

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
    JOB_TERMINATED
} job_state_t;

typedef struct {
    pid_t pid;
    int job_id;
    job_state_t state;
    char command[MAX_LINE];
    int is_background;
    int status;
} job_t;

typedef struct {
    char *args[MAX_ARGS];
    char *input_file;
    char *output_file;
    char *append_file;
    char *stderr_file;
    char *stderr_append_file;
    int background;
    int pipe_to;
    int compound_type;
} command_t;

extern job_t jobs[MAX_JOBS];
extern int next_job_id;
extern pid_t shell_pgid;
extern int shell_terminal;
extern int shell_is_interactive;

void init_shell(void);
char *read_line(void);
command_t *parse_command(char *line, int *cmd_count);
void execute_command(command_t *cmds, int cmd_count);
void execute_pipeline(command_t *cmd, int fd_in, int fd_out, int close_pipe_in, int close_pipe_out);

int builtin_cd(int argc, char **argv);
int builtin_pwd(int argc, char **argv);
int builtin_echo(int argc, char **argv);
int builtin_jobs(int argc, char **argv);
int builtin_fg(int argc, char **argv);
int builtin_bg(int argc, char **argv);
int builtin_stop(int argc, char **argv);
int builtin_kill(int argc, char **argv);
int builtin_exit(int argc, char **argv);
int builtin_set(int argc, char **argv);
int builtin_env(int argc, char **argv);
int builtin_export(int argc, char **argv);
int builtin_alias(int argc, char **argv);
int builtin_unalias(int argc, char **argv);
int builtin_history(int argc, char **argv);
int builtin_mkdir(int argc, char **argv);

int is_builtin(char *cmd);
int run_builtin(int argc, char **argv);

void add_job(pid_t pid, char *command, int is_background, job_state_t state);
void remove_job(pid_t pid);
void update_job_status(pid_t pid, int status, job_state_t state);
job_t *find_job_by_pid(pid_t pid);
job_t *find_job_by_id(int job_id);
void list_jobs(void);
void wait_for_job(pid_t pid);
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);

void handle_sigchld(int sig);
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void setup_signals(void);

char *get_prompt(void);
void trim_whitespace(char *str);
char *strdup_safe(const char *s);
char *expand_variables(const char *input);
char *strip_quotes(char *str);

void add_history(const char *cmd);
void load_history(void);
void save_history(void);
char *get_history_item(int index);

#endif
