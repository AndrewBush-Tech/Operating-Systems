#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

#ifndef MAX_LINE_LENGTH
#define MAX_LINE_LENGTH 1024
#endif

// Global variables
int last_exit_status = 0;
pid_t last_bg_pid = -1;
char* input_redirection_file = NULL;
char* output_redirection_file = NULL;
pid_t foreground_pid = -1;
int is_reading_input = 0;

// Function prototypes
void background_processes();
void wordsplit(const char* line, char* words[], int* num_words);
void expand(char* words[], int num_words);
int parse_and_execute(char* words[], int num_words,const char* line);
void sigint_handler(int sig);

int main(int argc, char *argv[]) {
  FILE* input_stream = stdin;
  if (argc > 1) {
    input_stream = fopen(argv[1], "r");
    if (!input_stream) {
      perror("Error opening file");
      return 1;
    }
  }

  // Signal Handling for SIGINT
  struct sigaction sigint_action = {0};
  sigint_action.sa_handler = sigint_handler;
  sigaction(SIGINT, &sigint_action, NULL);

  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char* words[MAX_WORDS];
  int num_words;

  for (;;){
    background_processes();
    // Command prompt setup
    char *PS1 = getenv("PS1");
    PS1 = PS1 ? PS1 : "$ ";
    if (input_stream == stdin) {
      fputs(PS1, stderr);
    }
    // Read input and handle comments
    is_reading_input = 1;
    read = getline(&line, &len, input_stream);
    is_reading_input = 0;
    char *commentPos = strchr(line, '#');
    if (commentPos) {
      *commentPos = '\0';
    }
    if (read <= 0) {
      if (errno == EINTR) continue;
        break;
    }
    wordsplit(line, words, &num_words);
    expand(words, num_words);
    parse_and_execute(words, num_words, line);
  }

  free(line);
  if (input_stream != stdin) fclose(input_stream);
  return 0;
}

void background_processes() {
  int status;
  pid_t pid;
  // Checking and reporting exit status of background processes
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    if (WIFEXITED(status)) {
      fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)pid, WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      if (pid == foreground_pid) {
        printf("%jd\n", (intmax_t)pid);
        foreground_pid = -1;
      } else {
      kill(pid, SIGCONT);
      fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)pid);
      }
    }
  }
}

// wordsplit() - Split the input line into individual words.
void wordsplit(const char* line, char* words[], int* num_words) {
  char *token, *temp_line, *cursor;
  temp_line = strdup(line); // Duplicate the line to avoid modifying the original
  *num_words = 0;
  cursor = temp_line;
  while (*cursor && *num_words < MAX_WORDS) {
    while (*cursor == ' ' || *cursor == '\n') {
      cursor++; // Skip spaces and newlines
    }
    token = cursor;  
    while (*cursor && *cursor != ' ' && *cursor != '\n') {
      if (*cursor == '\\') {
        // Instead of skipping, we'll handle the escape sequences
        memmove(cursor, cursor+1, strlen(cursor)); // Shift everything one position to the left
        cursor++;
      } else {
        cursor++;
      }
    }
    if (token != cursor) {
      words[*num_words] = strndup(token, cursor - token);
      (*num_words)++;
    }
  }
  words[*num_words] = NULL; // Ensure NULL-termination
  // Detect and handle the '<' operator for input redirection here
  for (int i = 0; i < *num_words; i++) {
    if (strcmp(words[i], "<") == 0) {
      // Check if there is a word after '<'
      if (i + 1 < *num_words) {
        input_redirection_file = strdup(words[i+1]);
        // Remove < and filename from the words array
        free(words[i]);
        free(words[i+1]);
        for (int j = i; j < *num_words - 2; j++) {
          words[j] = words[j+2];
        }
        words[*num_words - 1] = NULL;
        words[*num_words - 2] = NULL;
        *num_words -= 2;
      }
      break;
    }
    for (int i = 0; i < *num_words; i++) {
      if (strcmp(words[i], ">>") == 0) {
        // Check if there is a word after '>>'
        if (i + 1 < *num_words) {
          output_redirection_file = strdup(words[i+1]);
          // Remove >> and filename from the words array
          free(words[i]);
          free(words[i+1]);
          for (int j = i; j < *num_words - 2; j++) {
            words[j] = words[j+2];
          }
          words[*num_words - 1] = NULL;
          words[*num_words - 2] = NULL;
          *num_words -= 2;
        }
        break;
      }
    }
  }
  for (int i = 0; i < *num_words; i++) {
    if (strcmp(words[i], ">") == 0) {
      // Check if there is a word after '>'
      if (i + 1 < *num_words) {
        output_redirection_file = strdup(words[i+1]);
        // Remove > and filename from the words array
        free(words[i]);
        free(words[i+1]);
        for (int j = i; j < *num_words - 2; j++) {
          words[j] = words[j+2];
        }
        words[*num_words - 1] = NULL;
        words[*num_words - 2] = NULL;
        *num_words -= 2;
      }
      break;
    }
  }
  free(temp_line);
}

// expand() - Handle parameter expansion.
void expand(char* words[], int num_words) {
  char pid_str[20];  // to store pid as string
  char exit_str[20]; // to store last exit status as string
  char last_bg_pid_str[20]; // to store last background pid as string
  sprintf(pid_str, "%d", getpid());
  sprintf(exit_str, "%d", last_exit_status);
  sprintf(last_bg_pid_str, "%d", last_bg_pid);
  for (int i = 0; i < num_words; i++) {
    if (strstr(words[i], "$$")) {
      char *expanded_word = (char *)malloc(MAX_LINE_LENGTH);
      char *token = strstr(words[i], "$$");
      strncpy(expanded_word, words[i], token - words[i]);
      strcat(expanded_word, pid_str);
      strcat(expanded_word, token + 2);
      free(words[i]);
      words[i] = expanded_word;
    } else if (strstr(words[i], "$?")) {
      char *expanded_word = (char *)malloc(MAX_LINE_LENGTH);
      char *token = strstr(words[i], "$?");
      strncpy(expanded_word, words[i], token - words[i]);
      strcat(expanded_word, exit_str);
      strcat(expanded_word, token + 2);
      free(words[i]);
      words[i] = expanded_word;
    }else if (strstr(words[i], "$!")) {
      char *expanded_word = (char *)malloc(MAX_LINE_LENGTH);
      char *token = strstr(words[i], "$!");
      strncpy(expanded_word, words[i], token - words[i]);
      strcat(expanded_word, last_bg_pid_str);
      strcat(expanded_word, token + 2);
      free(words[i]);
      words[i] = expanded_word;
    }else if (strstr(words[i], "$!")) {
      char *expanded_word = (char *)malloc(MAX_LINE_LENGTH);
      char *token = strstr(words[i], "$!");
      strncpy(expanded_word, words[i], token - words[i]);
      strcat(expanded_word, last_bg_pid_str); 
      strcat(expanded_word, token + 2);
      free(words[i]);
      words[i] = expanded_word;
    }
    for (int i = 0; i < num_words; i++) {
    // Check for patterns 
      if (words[i][0] == '$' && words[i][1] == '{') {
        char *end_brace = strchr(words[i], '}');
        if (end_brace) {
          *end_brace = '\0';  // Null terminate at the closing brace
          char *var_name = &words[i][2];  // Skip past ${ to get the variable name
          char *env_value = getenv(var_name);  // Get the environment variable's value
          if (env_value) {
            free(words[i]);
            words[i] = strdup(env_value);  // Replace the word with the environment variable's value
          } else {
            free(words[i]);
            words[i] = strdup("");  // If the environment variable isn't set, replace with an empty string
          }
        }
      }
    }
  }
}

// parse_and_execute() - Parse the list of words and execute them as commands.
int parse_and_execute(char* words[], int num_words, const char* line) {
  if (num_words == 0) {
    return 0; 
  }
  int run_in_background = 0;
  if (strcmp(words[num_words - 1], "&") == 0) {
    run_in_background = 1;
    free(words[num_words - 1]);
    words[num_words - 1] = NULL;
    num_words--;
  }
  // CD command
  if (strcmp(words[0], "cd") == 0) {
    if (num_words == 1) { // cd with no arguments goes to the home directory
      chdir(getenv("HOME"));
    } else { // Otherwise, go to the directory specified
      if(chdir(words[1]) != 0) {
        perror("smallsh");
      }
    }
    if (strcmp(words[0], "cd") == 0) {
      for (int i = 0; i < num_words; i++) {
        free(words[i]);
      }
      return 0; // Only return after handling cd command
    }
  }
  // Exit command
  if (strcmp(words[0], "exit") == 0) {
    int exit_status = 0; // default exit status
    if (num_words > 1) {
      exit_status = atoi(words[1]);
      if(exit_status == 0 && strcmp(words[1], "0") != 0) {
        // Handle invalid integer. For now, we'll just print an error
        fprintf(stderr, "smallsh: exit: %s: numeric argument required\n", words[1]);
        exit_status = 1;
      }
    }
    for (int i = 0; i < num_words; i++) {
      free(words[i]);
    }
    exit(exit_status);
  }
  // Actual execution
  pid_t pid = fork();
  if (pid == 0) {
    signal(SIGTSTP, SIG_DFL);  
    signal(SIGCONT, SIG_DFL);
    // Child process
    if (input_redirection_file) { 
      int in_fd = open(input_redirection_file, O_RDONLY);
      if (in_fd == -1) {
        perror("smallsh");
        exit(EXIT_FAILURE);
      }
      dup2(in_fd, STDIN_FILENO);
      close(in_fd);
      free(input_redirection_file);
      input_redirection_file = NULL; // Reset for next command
    }
    if (output_redirection_file) {
      int out_fd;
      if (strstr(line, ">>")) {
        out_fd = open(output_redirection_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
      } else {
        out_fd = open(output_redirection_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      }
      if (out_fd == -1) {
        perror("smallsh");
        exit(EXIT_FAILURE);
      }
      dup2(out_fd, STDOUT_FILENO);
      close(out_fd);
      free(output_redirection_file);
      output_redirection_file = NULL;
    }
    signal(SIGTSTP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    execvp(words[0], words);
    perror("smallsh");
    exit(1);
    } else if (pid < 0) {
      perror("fork failed");
    } else {
      if (run_in_background) {
        last_bg_pid = pid;
      } else {
        foreground_pid = pid;
        int status;
        waitpid(pid, &status, 0); // Wait for child process to terminate
      if (WIFEXITED(status)) {
        last_exit_status = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        last_exit_status = 128 + WTERMSIG(status);
      }
      foreground_pid = -1;
    }
  }
  if (input_redirection_file) {
    free(input_redirection_file);
    input_redirection_file = NULL;
  }
  if (output_redirection_file) {
    free(output_redirection_file);
    output_redirection_file = NULL;
  }
  for (int i = 0; i < num_words; i++) {
    free(words[i]); // Free the dynamically allocated memory
  }
  return 0;
}

// sigint_handler() - Handle the SIGINT signal.
void sigint_handler(int sig) {
  if (is_reading_input) {
    // If reading input, just print a new prompt
    fprintf(stderr, "\n");
    char *PS1 = getenv("PS1");
    if (PS1 == NULL) {
      PS1 = "$ ";
    }
    fputs(PS1, stderr);
    } else if (foreground_pid != -1) {
      // If a foreground process is running, kill it
      kill(foreground_pid, SIGINT);
    }
  }
  void sigcont_handler(int sig) {
    // If the foreground process was stopped, then continue it
    if (foreground_pid != -1) {
      kill(foreground_pid, SIGCONT);
    }
}
