#include "shell.h"

#ifndef killpg
int killpg(pid_t pgrp, int sig);
#endif

job_t jobs[MAX_JOBS];
int next_job_id = 1;
pid_t shell_pgid;
int shell_terminal;
int shell_is_interactive = 0;

static char history[MAX_HISTORY][MAX_LINE];
static int history_count = 0;
static int history_start = 0;

static char *builtin_cmds[] = {
    "goto", "where", "say", "tasks", "fg", "bg", "stop",
    "quit", "set", "env", "let", "alias", "unalias", "history", "create", NULL
};

static int (*builtin_funcs[])(int, char **) = {
    builtin_cd, builtin_pwd, builtin_echo, builtin_jobs, builtin_fg, builtin_bg,
    builtin_stop, builtin_exit, builtin_set, builtin_env, builtin_export,
    builtin_alias, builtin_unalias, builtin_history, builtin_mkdir, NULL
};

char *strdup_safe(const char *s) {
    if (!s) return NULL;
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

void add_history(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    
    if (history_count > 0 && strcmp(history[(history_start + history_count - 1) % MAX_HISTORY], cmd) == 0) {
        return;
    }
    
    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count], cmd, MAX_LINE - 1);
        history[history_count][MAX_LINE - 1] = '\0';
        history_count++;
    } else {
        strncpy(history[history_start], cmd, MAX_LINE - 1);
        history[history_start][MAX_LINE - 1] = '\0';
        history_start = (history_start + 1) % MAX_HISTORY;
    }
}

char *get_history_item(int index) {
    if (index < 0 || index >= history_count) return NULL;
    int idx = (history_start + index) % MAX_HISTORY;
    return history[idx];
}

void load_history(void) {
    char histfile[MAX_PATH];
    char *home = getenv("HOME");
    if (!home) return;
    
    snprintf(histfile, sizeof(histfile), "%s/.lunex_history", home);
    
    FILE *fp = fopen(histfile, "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp) && history_count < MAX_HISTORY) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (line[0] != '\0') {
            strncpy(history[history_count], line, MAX_LINE - 1);
            history[history_count][MAX_LINE - 1] = '\0';
            history_count++;
        }
    }
    fclose(fp);
}

void save_history(void) {
    char histfile[MAX_PATH];
    char *home = getenv("HOME");
    if (!home) return;
    
    snprintf(histfile, sizeof(histfile), "%s/.lunex_history", home);
    
    FILE *fp = fopen(histfile, "w");
    if (!fp) return;
    
    for (int i = 0; i < history_count; i++) {
        fprintf(fp, "%s\n", get_history_item(i));
    }
    fclose(fp);
}

char *expand_variables(const char *input) {
    static char result[MAX_LINE];
    int j = 0;
    int tilde_handled = 0;
    
    if (input[0] == '~' && (input[1] == '/' || input[1] == '\0' || !isalnum((unsigned char)input[1]))) {
        tilde_handled = 1;
        if (input[1] == '/' || input[1] == '\0') {
            char *home = getenv("HOME");
            if (home) {
                strncpy(result, home, MAX_LINE - 1);
                j = strlen(home);
                if (input[1] == '/') input += 2;
                else input++;
            } else {
                result[j++] = '~';
                input++;
            }
        } else {
            int k = 0;
            char username[MAX_LINE] = {0};
            input++;
            while (input[k] && input[k] != '/' && k < MAX_LINE - 1) {
                username[k++] = input[k];
            }
            input += k;
            struct passwd *pw = getpwnam(username);
            if (pw) {
                strncpy(result, pw->pw_dir, MAX_LINE - 1);
                j = strlen(pw->pw_dir);
            } else {
                result[j++] = '~';
                for (int m = 0; username[m]; m++) result[j++] = username[m];
            }
            if (input[0] == '/') input++;
        }
    }
    
    if (!tilde_handled) {
        result[0] = '\0';
    }
    
    for (int i = 0; input[i] && j < MAX_LINE - 1; i++) {
        if (input[i] == '\\' && input[i + 1]) {
            result[j++] = input[i++];
            result[j++] = input[i];
        } else if (input[i] == '\'') {
            result[j++] = input[i];
        } else if (input[i] == '"') {
            result[j++] = input[i];
        } else if (input[i] == '$') {
            if (input[i + 1] == '{') {
                char var_name[MAX_ARGS] = {0};
                int k = 0;
                i += 2;
                while (input[i] && input[i] != '}' && k < MAX_ARGS - 1) {
                    var_name[k++] = input[i++];
                }
                if (input[i] == '}') i++;
                char *value = getenv(var_name);
                if (value) {
                    strncpy(result + j, value, MAX_LINE - j - 1);
                    j += strlen(value);
                }
            } else if (input[i + 1] == '$') {
                pid_t pid = getpid();
                char pid_str[32];
                snprintf(pid_str, sizeof(pid_str), "%d", pid);
                strncpy(result + j, pid_str, MAX_LINE - j - 1);
                j += strlen(pid_str);
                i++;
            } else if (input[i + 1] == '?') {
                char status_str[32];
                snprintf(status_str, sizeof(status_str), "%d", 0);
                strncpy(result + j, status_str, MAX_LINE - j - 1);
                j += strlen(status_str);
                i++;
            } else if (input[i + 1] && (isalnum((unsigned char)input[i + 1]) || input[i + 1] == '_')) {
                char var_name[MAX_ARGS] = {0};
                int k = 0;
                i++;
                while (input[i] && (isalnum((unsigned char)input[i]) || input[i] == '_')) {
                    var_name[k++] = input[i++];
                }
                i--;
                char *value = getenv(var_name);
                if (value) {
                    strncpy(result + j, value, MAX_LINE - j - 1);
                    j += strlen(value);
                }
            } else {
                result[j++] = input[i];
            }
        } else {
            result[j++] = input[i];
        }
    }
    result[j] = '\0';
    return result;
}

void trim_whitespace(char *str) {
    if (!str) return;
    char *end;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    *(end + 1) = 0;
}

void init_shell(void) {
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);
    
    setbuf(stdout, NULL);
    
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    
    shell_pgid = getpid();
    
    if (shell_is_interactive) {
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("setpgid");
        }
    }
    
    setup_signals();
    
    printf("\033[38;5;45m");
    printf("░  ░░░░░░░░  ░░░░  ░░   ░░░  ░░        ░░  ░░░░  ░\n");
    printf("▒  ▒▒▒▒▒▒▒▒  ▒▒▒▒  ▒▒    ▒▒  ▒▒  ▒▒▒▒▒▒▒▒▒  ▒▒  ▒▒\n");
    printf("▓  ▓▓▓▓▓▓▓▓  ▓▓▓▓  ▓▓  ▓  ▓  ▓▓      ▓▓▓▓▓▓    ▓▓▓\n");
    printf("█  ████████  ████  ██  ██    ██  █████████  ██  ██\n");
    printf("█        ███      ███  ███   ██        ██  ████  █\n");
    printf("\033[0m");
    printf("\n");
    printf("  \033[38;5;208m❯\033[0m \033[38;5;45mLUNEX\033[0m \033[38;5;240m────────────────────────────────────\033[0m\n");
    printf("\n");
    printf("  \033[38;5;250mCommands:\033[0m\n");
    printf("    \033[38;5;82mwhere\033[0m   \033[38;5;240m│\033[0m  Print working directory\n");
    printf("    \033[38;5;82msay\033[0m    \033[38;5;240m│\033[0m  Print text to terminal\n");
    printf("    \033[38;5;82mgoto\033[0m   \033[38;5;240m│\033[0m  Change directory\n");
    printf("    \033[38;5;82mcreate\033[0m \033[38;5;240m│\033[0m  Create directory\n");
    printf("    \033[38;5;82mtasks\033[0m  \033[38;5;240m│\033[0m  List background jobs\n");
    printf("    \033[38;5;82mquit\033[0m   \033[38;5;240m│\033[0m  Exit the shell\n");
    printf("    \033[38;5;82mlet\033[0m    \033[38;5;240m│\033[0m  Set environment variable\n");
    printf("\n");
    printf("  \033[38;5;250mFeatures:\033[0m\n");
    printf("    \033[38;5;82mPipes\033[0m  \033[38;5;240m│\033[0m  \033[38;5;45mcmd1 | cmd2 | cmd3\033[0m\n");
    printf("    \033[38;5;82mRedir\033[0m  \033[38;5;240m│\033[0m  \033[38;5;45m> >> < 2>\033[0m\n");
    printf("    \033[38;5;82mJobs\033[0m   \033[38;5;240m│\033[0m  \033[38;5;45mfg bg stop tasks\033[0m\n");
    printf("\n");
    printf("\033[38;5;240m────────────────────────────────────────────────────────────────\033[0m");
    printf("\n");
    fflush(stdout);
    
    shell_pgid = getpid();
    
    if (shell_is_interactive) {
        while (tcgetpgrp(shell_terminal) != shell_pgid) {
            pid_t pgid = tcgetpgrp(shell_terminal);
            if (pgid == -1) {
                shell_is_interactive = 0;
                break;
            }
            killpg(getpgrp(), SIGTTIN);
        }
        
        if (shell_is_interactive && setpgid(shell_pgid, shell_pgid) < 0) {
            perror("setpgid");
        }
    }
    
    setup_signals();
    
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].pid = 0;
        jobs[i].job_id = 0;
        jobs[i].state = JOB_DONE;
    }
    
    load_history();
    atexit(save_history);
}

void setup_signals(void) {
    struct sigaction sa;
    
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

void handle_sigchld(int sig) {
    (void)sig;
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        job_t *job = find_job_by_pid(pid);
        if (job) {
            if (WIFEXITED(status)) {
                job->state = JOB_DONE;
                job->status = WEXITSTATUS(status);
                if (!job->is_background) {
                    printf("\n[%d] Done\t%s\n", job->job_id, job->command);
                    fflush(stdout);
                }
            } else if (WIFSIGNALED(status)) {
                job->state = JOB_TERMINATED;
                job->status = WTERMSIG(status);
                printf("\n[%d] Terminated\t%s\n", job->job_id, job->command);
                fflush(stdout);
            } else if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
                job->status = WSTOPSIG(status);
                printf("\n[%d] Stopped\t%s\n", job->job_id, job->command);
                fflush(stdout);
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
            }
        }
    }
}

void handle_sigint(int sig) {
    (void)sig;
}

void handle_sigtstp(int sig) {
    (void)sig;
}

char *get_prompt(void) {
    static char prompt[MAX_PATH + 64];
    char cwd[MAX_PATH];
    char *user = getenv("USER");
    char *home = getenv("HOME");
    
    if (!user) user = "user";
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "/");
    }
    
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(prompt, sizeof(prompt), "~%s", cwd + strlen(home));
    } else {
        snprintf(prompt, sizeof(prompt), "%s", cwd);
    }
    
    static char final_prompt[512];
    snprintf(final_prompt, sizeof(final_prompt), "\033[38;5;208m❯\033[0m \033[38;5;45m%s\033[0m \033[38;5;240m➜\033[0m ", prompt);
    
    return final_prompt;
}

char *read_line(void) {
    static char line[MAX_LINE];
    
    if (fgets(line, MAX_LINE, stdin) == NULL) {
        if (feof(stdin) || ferror(stdin)) {
            if (shell_is_interactive) {
                printf("\nexit\n");
                return NULL;
            }
            printf("\nexit\n");
            exit(0);
        }
        return NULL;
    }
    
    return line;
}

char *strip_quotes(char *str) {
    if (!str) return str;
    int len = strlen(str);
    if (len < 2) return str;
    
    if ((str[0] == '"' && str[len-1] == '"') || (str[0] == '\'' && str[len-1] == '\'')) {
        str[len-1] = '\0';
        return str + 1;
    }
    return str;
}

static char *tokenize_line(const char *line, int *token_count) {
    static char tokens[MAX_ARGS][MAX_LINE];
    for (int i = 0; i < MAX_ARGS; i++) {
        tokens[i][0] = '\0';
    }
    
    int count = 0;
    char token_buf[MAX_LINE] = {0};
    int buf_pos = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    for (int i = 0; line[i] && count < MAX_ARGS; i++) {
        if (line[i] == '\\' && line[i + 1] && !in_single_quote) {
            if (!in_double_quote) {
                token_buf[buf_pos++] = line[++i];
            } else {
                token_buf[buf_pos++] = line[i];
                if (line[i + 1]) {
                    token_buf[buf_pos++] = line[i + 1];
                    i++;
                }
            }
            continue;
        } else if (line[i] == '\'') {
            if (in_double_quote) {
                token_buf[buf_pos++] = line[i];
            } else {
                in_single_quote = !in_single_quote;
                token_buf[buf_pos++] = line[i];
            }
        } else if (line[i] == '"') {
            if (in_single_quote) {
                token_buf[buf_pos++] = line[i];
            } else if (i > 0 && line[i-1] == '\\') {
                token_buf[buf_pos++] = line[i];
            } else if (in_double_quote) {
                in_double_quote = 0;
                token_buf[buf_pos++] = line[i];
            } else {
                in_double_quote = 1;
                token_buf[buf_pos++] = line[i];
            }
        } else if (!in_single_quote && !in_double_quote) {
            if (line[i] == ';' || line[i] == '|' || line[i] == '&') {
                if (buf_pos > 0) {
                    token_buf[buf_pos] = '\0';
                    strncpy(tokens[count++], token_buf, MAX_LINE - 1);
                    buf_pos = 0;
                    token_buf[0] = '\0';
                }
                if (line[i] == '&' && line[i + 1] == '&') {
                    token_buf[buf_pos++] = '&';
                    token_buf[buf_pos++] = '&';
                    i++;
                } else if (line[i] == '|' && line[i + 1] == '|') {
                    token_buf[buf_pos++] = '|';
                    token_buf[buf_pos++] = '|';
                    i++;
                } else {
                    token_buf[buf_pos++] = line[i];
                }
                token_buf[buf_pos] = '\0';
                strncpy(tokens[count++], token_buf, MAX_LINE - 1);
                buf_pos = 0;
                token_buf[0] = '\0';
                continue;
            }
            if (line[i] == ' ' || line[i] == '\t') {
                if (buf_pos > 0) {
                    token_buf[buf_pos] = '\0';
                    strncpy(tokens[count++], token_buf, MAX_LINE - 1);
                    buf_pos = 0;
                    token_buf[0] = '\0';
                }
                continue;
            }
        }
        
        token_buf[buf_pos++] = line[i];
    }
    
    if (buf_pos > 0) {
        token_buf[buf_pos] = '\0';
        strncpy(tokens[count++], token_buf, MAX_LINE - 1);
    }
    
    *token_count = count;
    return (char *)tokens;
}

static char *process_token(const char *token) {
    static char result[MAX_LINE];
    int j = 0;
    int len = strlen(token);
    char quote_char = 0;
    
    if (len >= 2) {
        if ((token[0] == '"' && token[len-1] == '"') || (token[0] == '\'' && token[len-1] == '\'')) {
            quote_char = token[0];
        }
    }
    
    if (quote_char != '\'' && token[0] == '~' && (token[1] == '/' || token[1] == '\0' || !isalnum((unsigned char)token[1]))) {
        char *home = getenv("HOME");
        if (home) {
            strncpy(result, home, MAX_LINE - 1);
            j = strlen(home);
            if (token[1] == '/') {
                result[j++] = '/';
                token += 2;
            } else {
                token++;
            }
        } else {
            result[j++] = '~';
            token++;
        }
    }
    
    int start = (quote_char != 0) ? 1 : 0;
    int end = (quote_char != 0) ? len - 1 : len;
    
    for (int i = start; i < end; i++) {
        if (token[i] == '\\' && token[i + 1] && quote_char != '\'') {
            i++;
            if (token[i] == '\\') {
                result[j++] = '\\';
            } else if (token[i] == '"') {
                result[j++] = '"';
            } else if (token[i] == '\'') {
                result[j++] = '\'';
            } else if (token[i] == '$') {
                result[j++] = '$';
            } else if (token[i] == 'n') {
                result[j++] = '\n';
            } else if (token[i] == 't') {
                result[j++] = '\t';
            } else if (token[i] == 'r') {
                result[j++] = '\r';
            } else if (token[i] == '0') {
                result[j++] = '\0';
            } else {
                result[j++] = token[i];
            }
            continue;
        } else if (token[i] == '$' && quote_char != '\'') {
            if (token[i + 1] == '{') {
                char var_name[MAX_ARGS] = {0};
                int k = 0;
                i += 2;
                while (token[i] && token[i] != '}' && k < MAX_ARGS - 1) {
                    var_name[k++] = token[i++];
                }
                if (token[i] == '}') i++;
                char *value = getenv(var_name);
                if (value) {
                    strncpy(result + j, value, MAX_LINE - j - 1);
                    j += strlen(value);
                }
            } else if (token[i + 1] == '$') {
                char pid_str[32];
                snprintf(pid_str, sizeof(pid_str), "%d", getpid());
                strncpy(result + j, pid_str, MAX_LINE - j - 1);
                j += strlen(pid_str);
                i++;
            } else if (token[i + 1] == '?') {
                char status_str[32];
                snprintf(status_str, sizeof(status_str), "%d", 0);
                strncpy(result + j, status_str, MAX_LINE - j - 1);
                j += strlen(status_str);
                i++;
            } else if (token[i + 1] && (isalnum((unsigned char)token[i + 1]) || token[i + 1] == '_')) {
                char var_name[MAX_ARGS] = {0};
                int k = 0;
                i++;
                while (token[i] && (isalnum((unsigned char)token[i]) || token[i] == '_')) {
                    var_name[k++] = token[i++];
                }
                i--;
                char *value = getenv(var_name);
                if (value) {
                    strncpy(result + j, value, MAX_LINE - j - 1);
                    j += strlen(value);
                }
            } else {
                result[j++] = token[i];
            }
        } else {
            result[j++] = token[i];
        }
    }
    result[j] = '\0';
    return result;
}

static int handle_var_assignment(const char *assignment) {
    char *eq = strchr(assignment, '=');
    if (!eq) return 0;
    
    char name[MAX_ARGS] = {0};
    char value[MAX_LINE] = {0};
    strncpy(name, assignment, eq - assignment < MAX_ARGS - 1 ? eq - assignment : MAX_ARGS - 1);
    strncpy(value, eq + 1, MAX_LINE - 1);
    
    size_t vlen = strlen(value);
    if (vlen > 0 && ((value[0] == '"' && value[vlen-1] == '"') || (value[0] == '\'' && value[vlen-1] == '\''))) {
        value[vlen-1] = '\0';
        if (vlen > 1) {
            memmove(value, value + 1, strlen(value));
        }
    }
    
    setenv(name, value, 1);
    return 1;
}

command_t *parse_command(char *line, int *cmd_count) {
    command_t *cmds = malloc(sizeof(command_t) * MAX_ARGS);
    if (!cmds) return NULL;
    
    for (int i = 0; i < MAX_ARGS; i++) {
        cmds[i].args[0] = NULL;
        cmds[i].input_file = NULL;
        cmds[i].output_file = NULL;
        cmds[i].append_file = NULL;
        cmds[i].stderr_file = NULL;
        cmds[i].stderr_append_file = NULL;
        cmds[i].background = 0;
        cmds[i].pipe_to = -1;
        cmds[i].compound_type = 0;
    }
    
    int token_count = 0;
    char (*tokens)[MAX_LINE] = (char (*)[MAX_LINE])tokenize_line(line, &token_count);
    
    int cmd_idx = 0;
    int arg_idx = 0;
    
    for (int i = 0; i < token_count; i++) {
        char *t = tokens[i];
        
        if (strcmp(t, "|") == 0 && strcmp(t, "||") != 0) {
            cmds[cmd_idx].args[arg_idx] = NULL;
            cmd_idx++;
            arg_idx = 0;
            cmds[cmd_idx].pipe_to = cmd_idx - 1;
            cmds[cmd_idx].compound_type = 0;
            cmds[cmd_idx].background = 0;
            continue;
        }
        
        if (strcmp(t, ";") == 0) {
            cmds[cmd_idx].args[arg_idx] = NULL;
            cmds[cmd_idx].compound_type = 1;
            cmd_idx++;
            arg_idx = 0;
            cmds[cmd_idx].compound_type = 0;
            cmds[cmd_idx].background = 0;
            cmds[cmd_idx].pipe_to = -1;
            continue;
        }
        
        if (strcmp(t, "&&") == 0 && strlen(t) == 2) {
            cmds[cmd_idx].args[arg_idx] = NULL;
            cmds[cmd_idx].compound_type = 2;
            cmd_idx++;
            arg_idx = 0;
            cmds[cmd_idx].compound_type = 0;
            cmds[cmd_idx].background = 0;
            cmds[cmd_idx].pipe_to = -1;
            continue;
        }
        
        if (strcmp(t, "||") == 0 && strlen(t) == 2) {
            cmds[cmd_idx].args[arg_idx] = NULL;
            cmds[cmd_idx].compound_type = 3;
            cmd_idx++;
            arg_idx = 0;
            cmds[cmd_idx].compound_type = 0;
            cmds[cmd_idx].background = 0;
            cmds[cmd_idx].pipe_to = -1;
            continue;
        }
        
        if (strcmp(t, "<") == 0) {
            i++;
            if (i < token_count) {
                cmds[cmd_idx].input_file = strdup_safe(strip_quotes(process_token(tokens[i])));
            }
            continue;
        }
        
        if (strcmp(t, ">") == 0) {
            i++;
            if (i < token_count) {
                cmds[cmd_idx].output_file = strdup_safe(strip_quotes(process_token(tokens[i])));
            }
            continue;
        }
        
        if (strcmp(t, ">>") == 0) {
            i++;
            if (i < token_count) {
                cmds[cmd_idx].append_file = strdup_safe(strip_quotes(process_token(tokens[i])));
            }
            continue;
        }
        
        if (strcmp(t, "2>") == 0) {
            i++;
            if (i < token_count) {
                cmds[cmd_idx].stderr_file = strdup_safe(strip_quotes(process_token(tokens[i])));
            }
            continue;
        }
        
        if (strcmp(t, "2>>") == 0) {
            i++;
            if (i < token_count) {
                cmds[cmd_idx].stderr_append_file = strdup_safe(strip_quotes(process_token(tokens[i])));
            }
            continue;
        }
        
        if (strcmp(t, "&") == 0) {
            int is_last = (i == token_count - 1);
            int next_is_cmd = 0;
            if (!is_last && i + 1 < token_count) {
                char *next = tokens[i + 1];
                if (strcmp(next, ";") != 0 && strcmp(next, "|") != 0 &&
                    strcmp(next, "&&") != 0 && strcmp(next, "||") != 0 &&
                    strcmp(next, "&") != 0) {
                    next_is_cmd = 1;
                }
            }
            if (is_last || next_is_cmd) {
                cmds[cmd_idx].background = 1;
                continue;
            }
        }
        
        cmds[cmd_idx].args[arg_idx] = strdup_safe(strip_quotes(process_token(t)));
        arg_idx++;
    }
    
    cmds[cmd_idx].args[arg_idx] = NULL;
    
    *cmd_count = cmd_idx + 1;
    
    if (*cmd_count > 0 && cmds[*cmd_count - 1].background) {
        for (int i = 0; i < *cmd_count; i++) {
            cmds[i].background = 1;
        }
    }
    
    return cmds;
}

int is_builtin(char *cmd) {
    if (!cmd) return -1;
    for (int i = 0; builtin_cmds[i]; i++) {
        if (strcmp(cmd, builtin_cmds[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int run_builtin(int argc, char **argv) {
    int idx = is_builtin(argv[0]);
    if (idx >= 0 && builtin_funcs[idx]) {
        return builtin_funcs[idx](argc, argv);
    }
    return -1;
}

void execute_pipeline(command_t *cmd, int fd_in, int fd_out, int close_pipe_in, int close_pipe_out) {
    int builtin_idx = is_builtin(cmd->args[0]);
    
    if (builtin_idx >= 0) {
        int saved_stdin = -1;
        int saved_stdout = -1;
        
        if (fd_in != STDIN_FILENO) {
            saved_stdin = dup(STDIN_FILENO);
            dup2(fd_in, STDIN_FILENO);
            if (close_pipe_in) close(fd_in);
        }
        if (fd_out != STDOUT_FILENO) {
            saved_stdout = dup(STDOUT_FILENO);
            dup2(fd_out, STDOUT_FILENO);
            if (close_pipe_out) close(fd_out);
        }
        
        if (cmd->input_file && fd_in == STDIN_FILENO) {
            saved_stdin = dup(STDIN_FILENO);
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd >= 0) {
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
        }
        
        if (cmd->output_file && fd_out == STDOUT_FILENO) {
            saved_stdout = dup(STDOUT_FILENO);
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
        
        if (cmd->append_file && fd_out == STDOUT_FILENO) {
            saved_stdout = dup(STDOUT_FILENO);
            int fd = open(cmd->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
        
        if (cmd->stderr_file) {
            int fd = open(cmd->stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        
        if (cmd->stderr_append_file) {
            int fd = open(cmd->stderr_append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        
        int argc = 0;
        while (cmd->args[argc]) argc++;
        int result = builtin_funcs[builtin_idx](argc, cmd->args);
        
        fflush(stdout);
        
        if (saved_stdin >= 0) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        
        return;
    }
    
    pid_t pid;
    
    pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return;
    }
    
    if (pid == 0) {
        setpgid(getpid(), shell_pgid);
        
        if (fd_in != STDIN_FILENO) {
            dup2(fd_in, STDIN_FILENO);
            if (close_pipe_in) close(fd_in);
        }
        if (fd_out != STDOUT_FILENO) {
            dup2(fd_out, STDOUT_FILENO);
            if (close_pipe_out) close(fd_out);
        }
        
        if (cmd->input_file && fd_in == STDIN_FILENO) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        if (cmd->output_file && fd_out == STDOUT_FILENO) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        if (cmd->append_file && fd_out == STDOUT_FILENO) {
            int fd = open(cmd->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open append file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        if (cmd->stderr_file) {
            int fd = open(cmd->stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open stderr file");
                exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        if (cmd->stderr_append_file) {
            int fd = open(cmd->stderr_append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open stderr append file");
                exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTSTP, &sa, NULL);
        
        execvp(cmd->args[0], cmd->args);
        perror(cmd->args[0]);
        exit(127);
    }
    
    if (shell_is_interactive) {
        setpgid(pid, shell_pgid);
    }
    
    if (fd_in != STDIN_FILENO && close_pipe_in) {
        close(fd_in);
    }
    if (fd_out != STDOUT_FILENO && close_pipe_out) {
        close(fd_out);
    }
    
    if (!cmd->background) {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFSTOPPED(status)) {
            job_t *job = find_job_by_pid(pid);
            if (job) {
                job->state = JOB_STOPPED;
            }
        }
    } else {
        char cmd_str[MAX_LINE] = "";
        for (int i = 0; cmd->args[i]; i++) {
            if (i > 0) strcat(cmd_str, " ");
            strcat(cmd_str, cmd->args[i]);
        }
        add_job(pid, cmd_str, 1, JOB_RUNNING);
        
        if (shell_is_interactive) {
            printf("[%d] %d\n", next_job_id - 1, pid);
            fflush(stdout);
        }
    }
}

int execute_single_command(command_t *cmd) {
    if (!cmd || !cmd->args[0]) return 0;
    
    int builtin_idx = is_builtin(cmd->args[0]);
    if (builtin_idx >= 0) {
        int argc = 0;
        while (cmd->args[argc]) argc++;
        
        if (builtin_idx == 7) {
            builtin_exit(argc, cmd->args);
            return 0;
        }
        
        int saved_stdout = -1;
        int saved_stdin = -1;
        
        if (cmd->output_file) {
            saved_stdout = dup(STDOUT_FILENO);
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else {
                perror("open output");
            }
        } else if (cmd->append_file) {
            saved_stdout = dup(STDOUT_FILENO);
            int fd = open(cmd->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else {
                perror("open append");
            }
        }
        
        if (cmd->input_file) {
            saved_stdin = dup(STDIN_FILENO);
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd >= 0) {
                dup2(fd, STDIN_FILENO);
                close(fd);
            } else {
                perror("open input");
            }
        }
        
        if (cmd->stderr_file) {
            int fd = open(cmd->stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            } else {
                perror("open stderr");
            }
        }
        
        if (cmd->stderr_append_file) {
            int fd = open(cmd->stderr_append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            } else {
                perror("open stderr append");
            }
        }
        
        int result = builtin_funcs[builtin_idx](argc, cmd->args);
        
        fflush(stdout);
        
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stdin >= 0) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        
        return result;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    
    if (pid == 0) {
        setpgid(getpid(), shell_pgid);
        
        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        if (cmd->output_file) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        if (cmd->append_file) {
            int fd = open(cmd->append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open append file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        if (cmd->stderr_file) {
            int fd = open(cmd->stderr_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open stderr file");
                exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        if (cmd->stderr_append_file) {
            int fd = open(cmd->stderr_append_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open stderr append file");
                exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTSTP, &sa, NULL);
        
        execvp(cmd->args[0], cmd->args);
        perror(cmd->args[0]);
        exit(127);
    }
    
    if (shell_is_interactive) {
        setpgid(pid, shell_pgid);
    }
    
    if (!cmd->background) {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return 0;
    } else {
        char cmd_str[MAX_LINE] = "";
        for (int i = 0; cmd->args[i]; i++) {
            if (i > 0) strcat(cmd_str, " ");
            strcat(cmd_str, cmd->args[i]);
        }
        add_job(pid, cmd_str, 1, JOB_RUNNING);
        
        if (shell_is_interactive) {
            printf("[%d] %d\n", next_job_id - 1, pid);
            fflush(stdout);
        }
    }
    return 0;
}

void execute_command(command_t *cmds, int cmd_count) {
    if (cmd_count == 0) return;
    
    if (!cmds[0].args[0]) return;
    
    int last_status = 0;
    
    int has_pipe = 0;
    for (int i = 0; i < cmd_count; i++) {
        if (cmds[i].pipe_to >= 0) {
            has_pipe = 1;
            break;
        }
    }
    
    if (has_pipe) {
        int pipefd[2];
        int fd_in = STDIN_FILENO;
        
        for (int i = 0; i < cmd_count - 1; i++) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return;
            }
            
            execute_pipeline(&cmds[i], fd_in, pipefd[1], (fd_in != STDIN_FILENO), 1);
            
            if (fd_in != STDIN_FILENO) close(fd_in);
            fd_in = pipefd[0];
        }
        
        execute_pipeline(&cmds[cmd_count - 1], fd_in, STDOUT_FILENO, 1, 0);
        return;
    }
    
    for (int i = 0; i < cmd_count; i++) {
        if (i > 0) {
            int prev_compound = cmds[i - 1].compound_type;
            
            if (prev_compound == 2 && last_status != 0) continue;
            if (prev_compound == 3 && last_status == 0) continue;
        }
        
        if (!cmds[i].args[0]) continue;
        
        last_status = execute_single_command(&cmds[i]);
    }
}

void add_job(pid_t pid, char *command, int is_background, job_state_t state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].job_id = next_job_id++;
            jobs[i].state = state;
            jobs[i].is_background = is_background;
            jobs[i].status = 0;
            strncpy(jobs[i].command, command, MAX_LINE - 1);
            jobs[i].command[MAX_LINE - 1] = 0;
            return;
        }
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].job_id = 0;
            jobs[i].state = JOB_DONE;
            return;
        }
    }
}

void update_job_status(pid_t pid, int status, job_state_t state) {
    job_t *job = find_job_by_pid(pid);
    if (job) {
        job->status = status;
        job->state = state;
    }
}

job_t *find_job_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

job_t *find_job_by_id(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id) {
            return &jobs[i];
        }
    }
    return NULL;
}

void list_jobs(void) {
    int found = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0 && jobs[i].state != JOB_DONE) {
            found = 1;
            char *state_str;
            switch (jobs[i].state) {
                case JOB_RUNNING: state_str = "Running"; break;
                case JOB_STOPPED: state_str = "Stopped"; break;
                case JOB_TERMINATED: state_str = "Terminated"; break;
                default: state_str = "Unknown"; break;
            }
            printf("[%d] %s\t%s%s\n", 
                   jobs[i].job_id, 
                   state_str, 
                   jobs[i].command,
                   jobs[i].is_background ? " &" : "");
        }
    }
    if (!found) {
        printf("No jobs.\n");
    }
}

void wait_for_job(pid_t pid) {
    int status;
    waitpid(pid, &status, 0);
}

void put_job_in_foreground(job_t *job, int cont) {
    if (!job) return;
    
    job->is_background = 0;
    
    if (cont) {
        if (kill(-job->pid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }
    
    wait_for_job(job->pid);
}

void put_job_in_background(job_t *job, int cont) {
    if (!job) return;
    
    job->is_background = 1;
    
    if (cont) {
        if (kill(-job->pid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }
    
    printf("[%d] %s &\n", job->job_id, job->command);
}

int builtin_cd(int argc, char **argv) {
    char *path;
    
    if (argc == 1) {
        path = getenv("HOME");
        if (!path) {
            struct passwd *pw = getpwuid(getuid());
            path = pw->pw_dir;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        path = getenv("OLDPWD");
        if (!path) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
    } else {
        path = argv[1];
    }
    
    char oldpwd[MAX_PATH];
    getcwd(oldpwd, sizeof(oldpwd));
    
    if (chdir(path) < 0) {
        perror("cd");
        return 1;
    }
    
    setenv("OLDPWD", oldpwd, 1);
    char cwd[MAX_PATH];
    getcwd(cwd, sizeof(cwd));
    setenv("PWD", cwd, 1);
    
    return 0;
}

int builtin_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
        return 0;
    }
    perror("pwd");
    return 1;
}

int builtin_echo(int argc, char **argv) {
    int n_flag = 0;
    int start = 1;
    
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        n_flag = 1;
        start = 2;
    }
    
    for (int i = start; i < argc; i++) {
        if (i > start) printf(" ");
        printf("%s", argv[i]);
    }
    
    if (!n_flag) printf("\n");
    
    return 0;
}

int builtin_jobs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    list_jobs();
    return 0;
}

int builtin_fg(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "fg: job id required\n");
        return 1;
    }
    
    int job_id = atoi(argv[1] + 1);
    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
    } else {
        job_id = atoi(argv[1]);
    }
    
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "fg: %s: no such job\n", argv[1]);
        return 1;
    }
    
    put_job_in_foreground(job, job->state == JOB_STOPPED);
    return 0;
}

int builtin_bg(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "bg: job id required\n");
        return 1;
    }
    
    int job_id;
    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
    } else {
        job_id = atoi(argv[1]);
    }
    
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "bg: %s: no such job\n", argv[1]);
        return 1;
    }
    
    put_job_in_background(job, job->state == JOB_STOPPED);
    return 0;
}

int builtin_stop(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "stop: job id required\n");
        return 1;
    }
    
    int job_id;
    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
    } else {
        job_id = atoi(argv[1]);
    }
    
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "stop: %s: no such job\n", argv[1]);
        return 1;
    }
    
    if (kill(-job->pid, SIGSTOP) < 0) {
        perror("stop");
        return 1;
    }
    
    job->state = JOB_STOPPED;
    return 0;
}

int builtin_kill(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "kill: usage: kill [-sigspec] pid\n");
        return 1;
    }
    
    int sig = SIGTERM;
    int start = 1;
    
    if (argv[1][0] == '-') {
        if (argv[1][1] == '-') {
            start = 2;
        } else {
            sig = atoi(argv[1] + 1);
            if (sig == 0) {
                if (strcmp(argv[1] + 1, "TERM") == 0) sig = SIGTERM;
                else if (strcmp(argv[1] + 1, "KILL") == 0) sig = SIGKILL;
                else if (strcmp(argv[1] + 1, "STOP") == 0) sig = SIGSTOP;
                else if (strcmp(argv[1] + 1, "CONT") == 0) sig = SIGCONT;
            }
            start = 2;
        }
    }
    
    for (int i = start; i < argc; i++) {
        pid_t pid = atoi(argv[i]);
        if (pid > 0) {
            if (kill(pid, sig) < 0) {
                perror("kill");
                return 1;
            }
        }
    }
    
    return 0;
}

int builtin_exit(int argc, char **argv) {
    int status = 0;
    if (argc > 1) {
        status = atoi(argv[1]);
    }
    exit(status);
}

int builtin_set(int argc, char **argv) {
    if (argc == 1) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("%s\n", *env);
        }
        return 0;
    }
    
    if (argc == 2 && strchr(argv[1], '=')) {
        char *name = strtok(argv[1], "=");
        char *value = strtok(NULL, "=");
        if (name && value) {
            setenv(name, value, 1);
        }
        return 0;
    }
    
    fprintf(stderr, "set: usage: set [var=value]\n");
    return 1;
}

int builtin_env(int argc, char **argv) {
    (void)argc;
    (void)argv;
    extern char **environ;
    for (char **env = environ; *env; env++) {
        printf("%s\n", *env);
    }
    return 0;
}

int builtin_export(int argc, char **argv) {
    if (argc < 2) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strchr(argv[i], '=')) {
            char *name = strtok(argv[i], "=");
            char *value = strtok(NULL, "=");
            if (name && value) {
                setenv(name, value, 1);
            }
        } else {
            char *name = argv[i];
            char *value = getenv(name);
            if (value) {
                printf("export %s=%s\n", name, value);
            } else {
                printf("export %s\n", name);
            }
        }
    }
    return 0;
}

int builtin_alias(int argc, char **argv) {
    static char alias_buffer[4096] = {0};
    
    if (argc == 1) {
        if (alias_buffer[0]) {
            printf("%s", alias_buffer);
        }
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        strcat(alias_buffer, argv[i]);
        strcat(alias_buffer, " ");
    }
    strcat(alias_buffer, "\n");
    
    return 0;
}

int builtin_unalias(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (argc < 2) {
        fprintf(stderr, "unalias: usage: unalias name\n");
        return 1;
    }
    return 0;
}

int builtin_history(int argc, char **argv) {
    int n = 10;
    
    if (argc > 1) {
        if (strcmp(argv[1], "-c") == 0) {
            history_count = 0;
            history_start = 0;
            return 0;
        } else if (strcmp(argv[1], "-d") == 0 && argc > 2) {
            int idx = atoi(argv[2]);
            if (idx >= 0 && idx < history_count) {
                for (int i = idx; i < history_count - 1; i++) {
                    strncpy(history[(history_start + i) % MAX_HISTORY], 
                            history[(history_start + i + 1) % MAX_HISTORY], MAX_LINE - 1);
                }
                history_count--;
            }
            return 0;
        } else if (argv[1][0] == '-' && isdigit((unsigned char)argv[1][1])) {
            n = atoi(argv[1] + 1);
        } else if (isdigit((unsigned char)argv[1][0])) {
            n = atoi(argv[1]);
        }
    }
    
    if (history_count == 0) {
        printf("No history.\n");
        return 0;
    }
    
    int start = (n > history_count) ? 0 : history_count - n;
    for (int i = start; i < history_count; i++) {
        printf("  %d  %s\n", i + 1, get_history_item(i));
    }
    
    return 0;
}

int builtin_mkdir(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }
    
    int p_flag = 0;
    int start = 1;
    
    if (argc > 1 && strcmp(argv[1], "-p") == 0) {
        p_flag = 1;
        start = 2;
    }
    
    for (int i = start; i < argc; i++) {
        if (p_flag) {
            char cmd[MAX_PATH + 32];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", argv[i]);
            int ret = system(cmd);
            if (ret != 0) {
                return 1;
            }
        } else {
            if (mkdir(argv[i], 0755) < 0) {
                perror("mkdir");
                return 1;
            }
        }
    }
    
    return 0;
}

int main(int argc, char **argv) {
    char *line;
    command_t *cmds;
    int cmd_count;
    
    if (argc > 1 && strcmp(argv[1], "--web") == 0) {
        init_shell();
        start_web_server();
        return 0;
    }
    
    init_shell();
    
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            perror(argv[1]);
            exit(1);
        }
        
        char buffer[MAX_LINE];
        while (1) {
            line = fgets(buffer, MAX_LINE, fp);
            if (!line) break;
            
            trim_whitespace(line);
            if (line[0] == '#' || line[0] == 0) continue;
            
            char line_copy[MAX_LINE];
            strncpy(line_copy, line, MAX_LINE - 1);
            line_copy[MAX_LINE - 1] = 0;
            
            cmds = parse_command(line_copy, &cmd_count);
            if (cmds && cmds[0].args[0]) {
                execute_command(cmds, cmd_count);
            }
            
            if (cmds) {
                if (cmd_count > 0) {
                    for (int i = 0; i < cmd_count && i < MAX_ARGS; i++) {
                        for (int j = 0; j < MAX_ARGS; j++) {
                            if (cmds[i].args[j]) {
                                free(cmds[i].args[j]);
                                cmds[i].args[j] = NULL;
                            }
                        }
                        if (cmds[i].input_file) {
                            free(cmds[i].input_file);
                            cmds[i].input_file = NULL;
                        }
                        if (cmds[i].output_file) {
                            free(cmds[i].output_file);
                            cmds[i].output_file = NULL;
                        }
                        if (cmds[i].append_file) {
                            free(cmds[i].append_file);
                            cmds[i].append_file = NULL;
                        }
                        if (cmds[i].stderr_file) {
                            free(cmds[i].stderr_file);
                            cmds[i].stderr_file = NULL;
                        }
                        if (cmds[i].stderr_append_file) {
                            free(cmds[i].stderr_append_file);
                            cmds[i].stderr_append_file = NULL;
                        }
                    }
                }
                free(cmds);
                cmds = NULL;
            }
            
            for (int i = 0; i < MAX_JOBS; i++) {
                if (jobs[i].state == JOB_DONE || jobs[i].state == JOB_TERMINATED) {
                    jobs[i].pid = 0;
                }
            }
        }
        
        fclose(fp);
        return 0;
    }
    
    while (1) {
        if (shell_is_interactive) {
            char *prompt = get_prompt();
            printf("%s", prompt);
            fflush(stdout);
        }
        
        line = read_line();
        if (!line) {
            if (shell_is_interactive) break;
            continue;
        }
        
        trim_whitespace(line);
        if (line[0] == 0) continue;
        
        if (line[0] == '#') continue;
        
        add_history(line);
        
        char line_copy[MAX_LINE];
        strncpy(line_copy, line, MAX_LINE - 1);
        line_copy[MAX_LINE - 1] = 0;
        
        cmds = parse_command(line_copy, &cmd_count);
        if (!cmds) {
            continue;
        }
        
        if (cmds[0].args[0]) {
            execute_command(cmds, cmd_count);
        }
        
        if (cmd_count > 0) {
            for (int i = 0; i < cmd_count && i < MAX_ARGS; i++) {
                for (int j = 0; j < MAX_ARGS; j++) {
                    if (cmds[i].args[j]) {
                        free(cmds[i].args[j]);
                        cmds[i].args[j] = NULL;
                    }
                }
                if (cmds[i].input_file) {
                    free(cmds[i].input_file);
                    cmds[i].input_file = NULL;
                }
                if (cmds[i].output_file) {
                    free(cmds[i].output_file);
                    cmds[i].output_file = NULL;
                }
                if (cmds[i].append_file) {
                    free(cmds[i].append_file);
                    cmds[i].append_file = NULL;
                }
                if (cmds[i].stderr_file) {
                    free(cmds[i].stderr_file);
                    cmds[i].stderr_file = NULL;
                }
                if (cmds[i].stderr_append_file) {
                    free(cmds[i].stderr_append_file);
                    cmds[i].stderr_append_file = NULL;
                }
            }
        }
        free(cmds);
        cmds = NULL;
        
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].state == JOB_DONE || jobs[i].state == JOB_TERMINATED) {
                jobs[i].pid = 0;
            }
        }
    }
    
    return 0;
}
