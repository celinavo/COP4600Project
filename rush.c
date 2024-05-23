#include <stdlib.h>
#include <stdio.h>
#include <string.h> // for strsep()
#include <sys/wait.h> // for wait()/waitpid()
#include <unistd.h> // for fork(), exec()
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

char error_message[] = "An error has occurred\n";

void print_prompt(){ // prints rush
    printf("rush> ");
    fflush(stdout);
}

int empty_whitespace(const char* str){
    while (*str){
        if (!isspace((unsigned char)*str)) return 0;
        str++;
    }
    return 1;
}

int read_input(char* pInput){ // modifies pointer holding user input line to current user inputex
    char *buffer = pInput;
    size_t buffer_size = 255;
    size_t num_char;
    num_char = getline(&buffer, &buffer_size, stdin);
    // printf("right here: num_Char = %ld\n", num_char);

    while (num_char == 1 || empty_whitespace(pInput)){
        // printf("made it");
        print_prompt();
        num_char = getline(&buffer, &buffer_size, stdin);
    }
    
    if ((pInput)[num_char-1] == '\n') pInput[num_char-1] = '\0';

    return 0;
}

int get_commands(char* pInput, char*** pList){ // parameters: string*, pointer* to list* of strings*
    const char* delimiter = "&";
    int i = 0;
    char *token;

    #define STRING_SIZE 64
    *pList = malloc(STRING_SIZE * sizeof(char*));    
    token = strtok(pInput, delimiter); // separate string into commands divided by '&'
    
    while (token != NULL){
        // printf("%s\n", token); // test code
        (*pList)[i] = malloc(strlen(token) + 1); // allocate memory in ith element of array of pointers
        strcpy((*pList)[i], token);
        // (*pList)[i] = token; // place token in ith element of array of pointers
        // printf("%s\n", (*pList)[i]); // test code
        i++;
        token = strtok(NULL, delimiter);
    }
    return i;
}

int get_arguments(char* cmd, char*** cmd_tokens, int* redirectIndex){ // parameters: string*, pointer* to list* of strings* 
    const char* delimieter = " \t";
    int i = 0; // num_tokens
    char *token;

    #define STRING_SIZE 64
    *cmd_tokens = malloc(STRING_SIZE * sizeof(char*));
    token = strtok(cmd, delimieter);
    int numArgsAfterRedirect = 0;
    while (token != NULL){
        (*cmd_tokens)[i] = malloc(strlen(token)+1); // allocate space for that token in cmd_tokens
        strcpy((*cmd_tokens)[i], token); // place token in cmd_tokens
        if (!strcmp((*cmd_tokens)[i], ">")){
            if ((*redirectIndex == -1)){ // if redirect index has NOT already been encountered
                // printf("%s at index %d is a redirect\n", token, i); // test code
                *redirectIndex = i;
            } else { // mark error if more than one redirect in an argument
                return -1; // return -1 arguments if error
            }
        }
        if (*redirectIndex != -1){
            numArgsAfterRedirect++;
            // printf("num args: %d", numArgsAfterRedirect);
            if (numArgsAfterRedirect > 2){
                return -1;
            }
        }
        // (*cmd_tokens)[i] = token;
        i++;
        token = strtok(NULL, delimieter);
    }

    return i;
}

int execute_command(const char* path, char** cmd_tokens, int num_tokens, int redirectIndex){
    
    // append command to path
    char command[256];
    strcpy(command, path);
    strcat(command, "/");
    strcat(command, cmd_tokens[0]);
    // determine if path is accessible ----> if not accessable, report error
    int cannotAccess; // 0 if can access
    cannotAccess = access(command, X_OK);
    // printf("cannotAccess %s: %d\n", command, cannotAccess); //test code
    // determine with path set to /usr/bin 
    if (cannotAccess){
        char usrcmd[] = "/usr/";
        strcat(usrcmd, command);
        cannotAccess = access(usrcmd, X_OK); 
        // printf("cannotAccess2 %s: %d\n", usrcmd, cannotAccess); // test code
        if (cannotAccess){ // if cannot find executable, report error
            write(STDERR_FILENO, error_message, strlen(error_message));
            fflush(stdout);
            return 1;
        } else {
            strcpy(command, usrcmd);
        }
    }

    // redirection
    if (redirectIndex > 0){
        int file = open(cmd_tokens[redirectIndex+1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (file == -1) {
            // printf("Could not open\n");
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }
        // printf("File created\n");
        // fflush(stdout);
        char ** before_redirect = malloc(redirectIndex * sizeof(char*)); // remove > ... from arguments -> new arguments in before_redirect
        for (int i = 0; i < redirectIndex; i++){
            before_redirect[i] = cmd_tokens[i];
        }
        dup2(file, 1);
        execv(command, before_redirect);
        fflush(stdout);
    }
    execv(command, cmd_tokens);
    fflush(stdout);
}

// ***** Built-in Commands *****
int shell_exit(char** cmd_tokens, int num_tokens){
    if (num_tokens == 1) exit(0);
    else write(STDERR_FILENO, error_message, strlen(error_message));
    fflush(stdout);
    return 1;
}

int shell_cd(char** cmd_tokens, int num_tokens){
    int success = 0;
    if (num_tokens == 2){
        success = chdir(cmd_tokens[1]);
        if (success == -1){
            write(STDERR_FILENO, error_message, strlen(error_message));
            fflush(stdout);
            return 1;
        }
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        fflush(stdout);
        return 1;
    }
}

int shell_path(char* path, char** cmd_tokens, int num_tokens){
    strcpy(path, "");
    if (num_tokens > 1){
        for (int i = 1; i < num_tokens; i++){
            strcat(path, cmd_tokens[i]);
        }
    }
    // printf("%s", path); //test code
}

int execute_built_in(char* path, char** cmd_tokens, int num_tokens){
    char *bc[3] = {"exit", "cd", "path"};

    if (!strcmp(cmd_tokens[0], bc[0])) shell_exit(cmd_tokens, num_tokens);
    if (!strcmp(cmd_tokens[0], bc[1])) shell_cd(cmd_tokens, num_tokens);
    if (!strcmp(cmd_tokens[0], bc[2])) shell_path(path, cmd_tokens, num_tokens);
    // printf("PATH AFTER CHANGE: %s", path); // test code

    return 0;
}

int main(int argc, char *argv[]){
    // printf("NUM ARGS %d", argc);
    if (argc != 1){
        write(STDERR_FILENO, error_message, strlen(error_message));
        return 1;
    }
    
    // initial path
    char path[256] = "/bin";

    int status;
    while (1){
        // print prompt
        print_prompt();

        // read user input
        char inputLine[255]; // stores line of input
        char *pInput = inputLine; // pointer to line of input
        read_input(pInput);
        if (strlen(pInput) == 0){
            write(STDERR_FILENO, error_message, strlen(error_message)); // if user input is empty, avoid segementation fault
            fflush(stdout);
            return 1;
        }

        // parse input into commands
        char **cmds; // list of pointers to commands
        int num_cmds;
        num_cmds = get_commands(pInput, &cmds); // pass address of pointer of list to function
        // printf("*%d*", num_cmds); //test code

        // for built-in processes (num_cmds = 1) OR NUM_CMDS =1 
        if (num_cmds == 1){
            // parse command into arguments
            char **cmd_tokens;
            int num_tokens;
            int redirectIndex = -1;
            num_tokens = get_arguments(cmds[0], &cmd_tokens, &redirectIndex);
            if (num_tokens == -1){
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            // printf("%d %d", redirectIndex, num_tokens); // test code
            // for (int j = 0; j < num_tokens; j++) printf("%s\n", cmd_tokens[j]);    // test code

            // check if built-in command
            char *bc[3] = {"exit", "cd", "path"};
            int isBc = 0; // if 1, then cmd_tokens[0] is a built-in command
            for (int i = 0; i < 3; i++){
                if (!strcmp(cmd_tokens[0], bc[i])) isBc = 1;
            }

            if (isBc) execute_built_in(path, cmd_tokens, num_tokens);
            else{
                pid_t child_pid = fork();
                if (child_pid == -1) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    fflush(stdout);
                }
                else if (child_pid == 0) {
                    execute_command(path, cmd_tokens, num_tokens, redirectIndex);
                    exit(0);
                }
                else waitpid(child_pid, NULL, 0);
            }
            continue;
        }
        
        // allocate array to store child processes
        pid_t* child_pids = malloc(num_cmds * sizeof(pid_t));

        // fork child processes
        for (int i = 0; i < num_cmds; i++){
            child_pids[i] = fork();

            if (child_pids[i] == -1){ // if failed to create new process
                write(STDERR_FILENO, error_message, strlen(error_message));
                fflush(stdout);

            } else if (child_pids[i] == 0) { // child process
                // parse command into arguments
                char **cmd_tokens;
                int num_tokens;
                int redirectIndex = -1;
                num_tokens = get_arguments(cmds[i], &cmd_tokens, &redirectIndex);
                if (num_tokens == -1){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
                }
                // for (int j = 0; j < num_tokens; j++) printf("%s\n", cmd_tokens[j]);            

                // execute commmands
                wait(NULL);
                execute_command(path, cmd_tokens, num_tokens, redirectIndex);
                free(cmd_tokens);
                _exit(0);
                // write(STDERR_FILENO, error_message, strlen(error_message));

            }
        }
        for (int i = 0; i < num_cmds; ++i) waitpid(child_pids[i], NULL, 0); // parent waits for all child processes after forking
        free(child_pids);
    }
}