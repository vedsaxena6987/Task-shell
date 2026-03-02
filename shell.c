#include "shell.h"

void shell_interactive(void)
{
    char buffer[1024];
    
    while (1)
    {
        printf("shell> ");
        fflush(stdout);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            break;
        
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) == 0)
            continue;
        
        if (strcmp(buffer, "exit") == 0)
            break;
        
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
        }
        else if (pid == 0)
        {
            char *args[] = {buffer, NULL};
            execvp(buffer, args);
            perror("execvp");
            exit(127);
        }
        else
        {
            wait(NULL);
        }
    }
}

void shell_no_interactive(void)
{
    char buffer[1024];
    
    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) == 0)
            continue;
        
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
        }
        else if (pid == 0)
        {
            char *args[] = {buffer, NULL};
            execvp(buffer, args);
            perror("execvp");
            exit(127);
        }
        else
        {
            wait(NULL);
        }
    }
}

int main(void)
{

 if (isatty(STDIN_FILENO) == 1)
 {
  shell_interactive();
 }
 else
 {
  shell_no_interactive();
 }
 return (0);
}