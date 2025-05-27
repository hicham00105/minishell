#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int status = 0;

#define C "\033[1;36m"
#define RED "\033[1;31m"
#define RST "\033[0m"

typedef struct {
    char *username;
    char *hostname;
    char *home_directory;
} Info;

Info user_info = {0};

void get_cwd(char *buffer, size_t size) {
    getcwd(buffer, size);

    if (strncmp(buffer, user_info.home_directory,
                strlen(user_info.home_directory)) == 0) {
        memmove(&buffer[1], &buffer[strlen(user_info.home_directory)],
                strlen(buffer) - strlen(user_info.home_directory) + 1);
        buffer[0] = '~';
    }
}

char *get_line() {
    if (isatty(fileno(stdin))) {
        char cwd[BUFSIZ];
        get_cwd(cwd, BUFSIZ);

        if (status != 0) {
            printf(C "%s" RST "@%s %s [" RED "%d" RST "]$ ", user_info.username,
                   user_info.hostname, cwd, status);
        } else {
            printf(C "%s" RST "@%s %s $ ", user_info.username,
                   user_info.hostname, cwd);
        }
    }

    char *line;
    size_t bufsize;
    int result = getline(&line, &bufsize, stdin);

    if (result == -1) {
        free(line);
        line = NULL;
        if (!feof(stdin))
            fprintf(stderr, "getline failed\n");
    }

    return line;
}
char **get_args(char *line) {
    if (line == NULL) {
        return NULL;
    }

    size_t bufsize = BUFSIZ;
    unsigned long position = 0;
    char **tokens = (char **)malloc(bufsize * sizeof(char *));

    if (tokens == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    for (char *token = strtok(line, "\t\n\v\f\r "); token != NULL;
         token = strtok(NULL, "\t\n\v\f\r ")) {
        tokens[position++] = token;
        if (position >= bufsize) {
            bufsize *= 2;
            tokens = realloc(tokens, bufsize * sizeof(char *));

            if (tokens == NULL) {
                fprintf(stderr, "realloc failed\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    tokens[position] = NULL;
    return (tokens);
}

int shell_cd(char **args) {
    char* dir = args[1];
    if (dir == NULL) {
        dir = user_info.home_directory;
    }

    int result = chdir(dir);

    if (result == -1) {
        fprintf(stderr, "failed to change directory\n");
        return 1;
    }

    return 0;
}

int shell_echo(char **args) {
    if (args == NULL || args[0] == NULL) {
        return 1;
    }

    bool newline = true;

    if (args[1] != NULL && strcmp(args[1], "-n") == 0) {
        newline = false;
    }

    for (int i = newline ? 2 : 1; args[i]; i++) {
        printf("%s", args[i]);

        if (args[i + 1] != NULL)
            printf(" ");
    }

    if (newline)
        printf("\n");

    return 0;
}

int shell_env(char **args) {
    extern char **environ;

    for (int i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }

    return 0;
}

int shell_exit(char **args) { exit(EXIT_SUCCESS); }

int (*get_builtin(char *cmd, char **name_out))(char **) {
    if (strcmp("echo", cmd) == 0) {
        *name_out = "echo";
        return shell_echo;
    } else if (strcmp("env", cmd) == 0) {
        *name_out = "env";
        return shell_env;
    } else if (strcmp("exit", cmd) == 0) {
        *name_out = "exit";
        return shell_exit;
    } else if (strcmp("cd", cmd) == 0) {
        *name_out = "cd";
        return shell_cd;
    } else {
        return NULL;
    }
}

void execute(char **args) {
    if (args == NULL || args[0] == NULL)
        return;

    char *builtin_name = NULL;
    int (*builtin)(char **) = get_builtin(args[0], &builtin_name);
    if (builtin != NULL) {
        int result = builtin(args);
        if (result != 0) {
            fprintf(stderr, "%s failed\n", builtin_name);
        }
        return;
    }

    pid_t process_id = fork();
    if (process_id < 0) {
        fprintf(stderr, "fork failed");
        exit(EXIT_FAILURE);
    }

    if (process_id == 0) {
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Shell failed to execute command: '%s'\n", args[0]);
        }
    } else {
        if (wait(&status) == -1)
            fprintf(stderr, "Wait failed\n");
        if (WIFEXITED(status))
            status = WEXITSTATUS(status);
    }
}

void populate_info() {
    extern char **environ;

    user_info.hostname = (char *)malloc(BUFSIZ * sizeof(char));
    if (user_info.hostname == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    for (char *entry = environ[i];
         entry != NULL &&
         (user_info.home_directory == NULL || user_info.username == NULL);
         entry = environ[++i]) {
        if (strncmp("USER=", entry, 5) == 0) {
            user_info.username = &entry[5];
        } else if (strncmp("HOME=", entry, 5) == 0) {
            user_info.home_directory = &entry[5];
        }
    }

    gethostname(user_info.hostname, BUFSIZ);
}

int main(void) {
    populate_info();

    char *line = get_line();
    char **args = get_args(line);

    while (line != NULL) {
        execute(args);

        line = get_line();
        args = get_args(line);
    }

    free(user_info.hostname);

    return 0;
}
