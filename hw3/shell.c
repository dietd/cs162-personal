#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_cd, "cd", "changes the working directory to input"},
  {cmd_pwd, "pwd", "prints working directory"}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

int cmd_cd(struct tokens *tokens) {
	char * directory = tokens_get_token(tokens, 1);
	chdir(directory);
	return 1;
}

int cmd_pwd(unused struct tokens *tokens) {
	char cwd[1024];
	getcwd(cwd, sizeof(cwd));
	fprintf(stdout, "%s\n", cwd);
	return 1;
}


/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

void run_program(char * path, char ** cmd) {
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGCONT, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	if(execv(path, cmd) < 0) {
                char buf[1024];
                char * envpath = strdup(getenv("PATH"));
                char * p = strtok(envpath, ":");
                while (p != NULL) {
                        strcpy(buf, p);
                        strcpy(buf + strlen(p), "/");
                        strcpy(buf + strlen(p) + 1, path);
                        if (execv(buf, cmd) >= 0) {
                                break;
                        }
                        p = strtok(NULL, ":");
                }
                free(envpath);
       }
}

int run_cmd(struct tokens * tokens, int fd_in, int fd_out) {
    /* Find which built-in function to run. */	


      int fundex = lookup(tokens_get_token(tokens,0));
      if (fundex >= 0) {

	int save_in = dup(STDIN_FILENO);
	int save_out = dup(STDOUT_FILENO);

	if (fd_in != STDIN_FILENO) {
		dup2(fd_in, STDIN_FILENO);
		close(fd_in);	
	}

	if (fd_out != STDOUT_FILENO) {
		dup2(fd_out, STDOUT_FILENO);
		close(fd_out);
	}

       	cmd_table[fundex].fun(tokens);

	dup2(save_in, STDIN_FILENO);
	dup2(save_out, STDOUT_FILENO);

	return 0;
      }

      int cpid = fork();
      if (cpid == 0) {

	      setpgid(0, 0);

	      //tcsetpgrp(0, getpgrp());
	      
	      if (fd_in != STDIN_FILENO) {
	      	dup2(fd_in, STDIN_FILENO);
		close(fd_in);
	      }

	      if (fd_out != STDOUT_FILENO) {
	      	dup2(fd_out, STDOUT_FILENO);
		close(fd_out);
	      }

	      int fundex = lookup(tokens_get_token(tokens, 0));

	      if (fundex >= 0) {
	      	cmd_table[fundex].fun(tokens);
		return 0;
	      }

	      //SETUP REDIRECTION
	      size_t len = tokens_get_length(tokens);
	      bool in = false;
	      bool out = false;
	      if (len > 2) {
	      	if (strcmp(tokens_get_token(tokens, len - 2), ">") == 0) {
			out = true;
			len -= 2;
		} else if (strcmp(tokens_get_token(tokens, len - 2), "<") == 0) {
			in = true;
			len -= 2;
		}
	      }

	  
	      if (out) {
	      	int fd = open(tokens_get_token(tokens, tokens_get_length(tokens) - 1),
				O_WRONLY | O_APPEND | O_CREAT, 0777);
		if (fd < 0)
			perror("ERROR FINDING FILE");
		dup2(fd, STDOUT_FILENO);
		close(fd);
	      }

	      if (in) {
		int fd = open(tokens_get_token(tokens, tokens_get_length(tokens) - 1),
				O_RDONLY);
		if (fd < 0)
			perror("ERROR FINDING FILE");
		dup2(fd, STDIN_FILENO);
		close(fd);
	      } 

 	      char * cmd[len];
              for (int i = 0; i < len; i += 1) {
                 cmd[i] = tokens_get_token(tokens, i);
              }
              cmd[len] = NULL;

	      run_program(cmd[0], cmd);
	      
	      return 0;
      
      } else if (cpid > 0){

	      if (fd_in != STDIN_FILENO) {
                close(fd_in);
              }

              if (fd_out != STDOUT_FILENO) {
                close(fd_out);
              }

	      setpgid(cpid, cpid);
	      
	      tcsetpgrp(shell_terminal, cpid);

	      int status;
	      waitpid(cpid, &status, WUNTRACED);
	    
	      tcsetpgrp(shell_terminal, shell_pgid); 

      } else {
      	perror("can't run the executable");
      }
    
    return 0;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    signal(SIGTTOU, SIG_IGN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }

  	signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGCONT, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    size_t arr_size = 1;
    struct tokens ** tokens_arr = malloc(sizeof(struct tokens *) * arr_size);

    size_t cur_size = 0;
    char * cur;
    char * save;
    for (cur = strtok_r(line, "|", &save); cur != NULL; cur = strtok_r(NULL, "|", &save)) {
	   struct tokens * t = tokenize(cur);
	   if (arr_size == cur_size) {
		arr_size = arr_size * 2;
	   	tokens_arr = realloc(tokens_arr, arr_size * sizeof(struct tokens *));
	   }
	   tokens_arr[cur_size] = t;
	   cur_size += 1;
    }

    /*Make some pipes*/
    int fd[2 * (cur_size - 1)];
    for (int i = 0; i < 2 * (cur_size - 1); i += 2) {
    	pipe(fd + i);
    }

    /*Run the command, pipes if included*/
    if (cur_size > 1) {
	run_cmd(tokens_arr[0], STDIN_FILENO, fd[1]);
	int cur_read = 0;
    	for (int i = 1; i < cur_size - 1; i += 1) {
		run_cmd(tokens_arr[i], fd[cur_read], fd[cur_read + 3]);
		cur_read += 2;
	}
	run_cmd(tokens_arr[cur_size - 1], fd[2 * (cur_size - 1) - 2], STDOUT_FILENO);
    } else { 
    	run_cmd(tokens_arr[0], STDIN_FILENO, STDOUT_FILENO);
    }

    /*if (true) {
	   struct tokens * tokens = tokens_arr[0];
	   int cpid = fork();
	   if (cpid == 0) {
		int len = tokens_get_length(tokens);

		close(fd[0]);
	   	dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);

		char * cmd[len + 1];
              	for (int i = 0; i < len; i += 1) {
                 	cmd[i] = tokens_get_token(tokens, i);
              	}
              	cmd[len] = NULL;
              	
		char buf[16] = "go bears";
		write(STDOUT_FILENO, buf, 16);
		return 0;
	   } else {
		waitpid(cpid, NULL, 0);
	   }

	   tokens = tokens_arr[1];

	   int rc = fork();
	   if (rc == 0) {
		int len = tokens_get_length(tokens);
		
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);

		char * cmd[len + 1];
                for (int i = 0; i < len; i += 1) {
                        cmd[i] = tokens_get_token(tokens, i);
                }
                cmd[len] = NULL;
               
	   	return 0;
	   } else {
		close(fd[0]);
		close(fd[1]);
		waitpid(rc, NULL, 0);	
	   }
    }*/

    /* for (size_t i = 0; i < cur_size; i += 1) {
	    run_cmd(tokens_arr[i]);
    } */   

    /* Find which built-in function to run. */
//    int fundex = lookup(tokens_get_token(tokens, 0));

   // if (fundex >= 0) {
     // cmd_table[fundex].fun(tokens);
    //} else {
      /* REPLACE this to run commands as programs. */
      //fprintf(stdout, "This shell doesn't know how to run programs.\n");
      //int cpid = fork();
      /*if (cpid == 0) {
	      

	      //SETUP REDIRECTION
	      size_t len = tokens_get_length(tokens);
	      bool in = false;
	      bool out = false;
	      if (len > 2) {
	      	if (strcmp(tokens_get_token(tokens, len - 2), ">") == 0) {
			out = true;
		} else if (strcmp(tokens_get_token(tokens, len - 2), "<") == 0) {
			in = true;
		}
	      }

	  
	      if (out) {
	      	int fd = open(tokens_get_token(tokens, tokens_get_length(tokens) - 1),
				O_WRONLY | O_APPEND | O_CREAT, 0777);
		if (fd < 0)
			perror("ERROR FINDING FILE");
		dup2(fd, STDOUT_FILENO);
		close(fd);
	      }

	      if (in) {
		int fd = open(tokens_get_token(tokens, tokens_get_length(tokens) - 1),
				O_RDONLY);
		if (fd < 0)
			perror("ERROR FINDING FILE");
		dup2(fd, STDIN_FILENO);
		close(fd);
	      }

	      //SETUP AND RUN IF REDIRECT
	      if (in || out) {
	      	char * cmd[len];
              	for (int i = 0; i < len - 2; i += 1) {
                  cmd[i] = tokens_get_token(tokens, i);
		}
		cmd[len - 2] = NULL;
		run_program(cmd[0], cmd);
		return 0;
	      } 

 	      char * cmd[len];
              for (int i = 0; i < len; i += 1) {
                 cmd[i] = tokens_get_token(tokens, i);
              }
              cmd[len] = NULL;

	      run_program(cmd[0], cmd);
	      
	      return 0;
      } else if (cpid > 0){
	      wait(NULL);
      } else {
      	perror("can't run the executable");
      }
    }*/

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    for (size_t j = 0; j < cur_size; j += 1) {
    	tokens_destroy(tokens_arr[j]);
    }
    free(tokens_arr);
  }

  return 0;
}
