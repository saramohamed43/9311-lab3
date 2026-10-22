#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

char *copyString(const std::string &s) // helper fn returns a new heap-allocated C string
{
    char *copy = (char *)malloc(s.length() + 1);
    strcpy(copy, s.c_str());
    return copy;
}

void parse(std::vector<Token> &tokens)
{
    if (tokens[0].type == TOKEN_EOF) //pressed enter by mistake
        {
            return;
        }

    Command::_currentSimpleCommand = new SimpleCommand();

    for(int i=0; i<tokens.size(); i++)
    {
        Token token = tokens[i];
        switch(token.type)
        {
            case TOKEN_COMMAND:
                Command::_currentSimpleCommand->insertArgument(copyString(token.value));
                break;
            case TOKEN_ARGUMENT:
                Command::_currentSimpleCommand->insertArgument(copyString(token.value));
                break;
            case TOKEN_APPEND: //store the next token as the output file
                Command::_currentCommand._append = 1; //append mode so we don't overwrite
                if(tokens[i+1].type == TOKEN_ARGUMENT)
                {
                    Command::_currentCommand._outFile = copyString(tokens[i+1].value);
                    i++;
                }
                else
                {
                    printf("Filename can't be empty\n");
                    Command::_currentCommand.clear();
                    return;
                }
                break;
            case TOKEN_REDIRECT: //store the next token as the output file
                if(tokens[i+1].type == TOKEN_ARGUMENT)
                {
                    Command::_currentCommand._outFile = copyString(tokens[i+1].value);
                    i++;
                }
                else
                {
                    printf("Filename can't be empty\n");
                    Command::_currentCommand.clear();
                    return;
                }               
                break;
            case TOKEN_INPUT: //store the next token as the input file
                if(tokens[i+1].type == TOKEN_ARGUMENT)
                {
                    Command::_currentCommand._inputFile = copyString(tokens[i+1].value);
                    i++;
                }
                else
                {
                    printf("Filename can't be empty\n");
                    Command::_currentCommand.clear();
                    return;
                }
                break;
            case TOKEN_ERROR: //store the next token as the error file
                if(tokens[i+1].type == TOKEN_ARGUMENT)
                {
                    Command::_currentCommand._errFile = copyString(tokens[i+1].value);
                    i++;
                }
                else
                {
                    printf("Filename can't be empty\n");
                    Command::_currentCommand.clear();
                    return;
                }
                break;
            case TOKEN_BACKGROUND: //set the background flag to true
                Command::_currentCommand._background = 1;
                break;
            case TOKEN_REDIRECT_AND_ERROR: //sends both the output and error to the same file
                Command::_currentCommand._out_error = 1;
                if(tokens[i+1].type == TOKEN_ARGUMENT)
                {
                    Command::_currentCommand._append = 1;
                    Command::_currentCommand._outFile = copyString(tokens[i+1].value);
                    Command::_currentCommand._errFile = copyString(tokens[i+1].value);
                    i++;
                }
                else
                {
                    printf("Filename can't be empty\n");
                    Command::_currentCommand.clear();
                    return;
                }
                break;
            case TOKEN_PIPE: //execute the current command and start a new one
                Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
                Command::_currentSimpleCommand = new SimpleCommand();
                break;
            default:
                break;
        }
    }
    Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
    Command::_currentCommand.execute();
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    print();

    if (_numberOfSimpleCommands == 0)
    {
        return;
    }

    // exit shell
    if (strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0)
    {
        printf("Good bye!!");
        exit(0);
    }

    // cd
    if(strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0)
    {
        if(_simpleCommands[0]->_numberOfArguments > 1)
        {
            char * path = _simpleCommands[0]->_arguments[1];
            if(chdir(path) != 0)
            {
                perror("cd didn't work");
            }
        }
        else 
        {
            char * home_path = getenv("HOME");
            if(chdir(home_path) != 0)
            {
                perror("cd didn't work");
            }
        }
        clear();
        return;
    }

    int defaultin = dup(0);
    int defaultout = dup(1);
    int defaulterr = dup(2);

    int failed = 0;
    int pid = -1;
    int fdin = -1;

    if (_inputFile)
    {
        fdin = open(_inputFile, O_RDONLY);
        if (fdin < 0)
        {
            perror("Cannot open input file");
            failed = 1;
        }
    }
    else
    {
        fdin = dup(defaultin);
    }

    if (!failed && _errFile && !_out_error)
    {
        int errfd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (errfd < 0)
        {
            perror("Cannot open error file");
            close(fdin);
            failed = 1;
        }
        else
        {
            dup2(errfd, 2);
            close(errfd);
        }
    }

    for (int i = 0; i < _numberOfSimpleCommands && !failed; i++)
    {
        dup2(fdin, 0);
        close(fdin);

        int fdout;
        if (i == _numberOfSimpleCommands - 1)
        {
            if (_outFile)
            {
                int flags = O_WRONLY | O_CREAT;
                if (_append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                fdout = open(_outFile, flags, 0666);
                if (fdout < 0)
                {
                    perror("Cannot open output file");
                    failed = 1;
                    break;
                }
                if (_out_error)
                    dup2(fdout, 2);  
            }
            else
            {
                fdout = dup(defaultout);
            }
        }
        else
        {
            int fdpipe[2];
            if (pipe(fdpipe) == -1)
            {
                perror("pipe");
                failed = 1;
                break;
            }
            fdout = fdpipe[1];   
            fdin = fdpipe[0];    
        }

        dup2(fdout, 1);
        close(fdout);

        pid = fork();
        if (pid < 0)
        {
            perror("Fork failed");
            failed = 1;
            break;
        }

        if (pid == 0)
        {
            close(defaultin);
            close(defaultout);
            close(defaulterr);
            if (i < _numberOfSimpleCommands - 1)
                close(fdin);    

            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);

            perror("Execution failed");
            exit(1);
        }
    }

    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);

    close(defaultin);
    close(defaultout);
    close(defaulterr);

    if (!failed && !_background)
    {
        waitpid(pid, 0, 0);
    }

    clear();
}

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);
        if(!std::getline(std::cin, input))
        {
            break;
        }
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

//writes in shell.log when a child finishes
void log_file(int signal_number)
{
    int pid;
    while((pid = waitpid(-1, 0, WNOHANG)) > 0)
    // WNOHANG returns 0 immediatly if no child finished 
    {
        FILE * f = fopen("shell.log","a");
        fprintf(f,"Child %d finished \n", pid);
        fclose(f);
    }

}

int main()
{
    signal(SIGINT,SIG_IGN); // ignores ctrl c signal
    signal(SIGCHLD,log_file); // signal when a child is done
    Command::_currentCommand.prompt();
    return 0;
}