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

    if(_numberOfSimpleCommands == 0)
    {
        return;
    }

    int pid = fork();
    if(pid < 0)
    {
        printf("Fork failed\n");
        return;
    }
    else if(pid == 0)
    {
        execvp(_simpleCommands[0]->_arguments[0], _simpleCommands[0]->_arguments);
        perror("Execution failed");
        exit(1);
    }
    else if(pid > 0)
    {
        waitpid(pid,0,0);
    }
    
    clear();
}

void Command::prompt()
{
    fflush(stdout);
    std::string input;
    while (true)
    {
        printf("myshell>");
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

int main()
{
    Command::_currentCommand.prompt();
    return 0;
}