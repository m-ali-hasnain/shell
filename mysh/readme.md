# Shell
This program is a basic shell that can execute commands and navigate directories.

# Requirements
C++11 or higher
Linux/Unix Environment

# How to use
1. Open terminal and navigate to the directory where the code is located.
2. Compile the program using the following command:

> g++ -std=c++11 mysh.cpp -o mysh
> ./mysh

Or, you can use the following commands

> make all
for creating mysh file
and then
> ./mysh
and for removing mysh
> make clean

3. You can then enter a command or navigate the directory using the built-in commands.
4. Enter "exit" to exit the shell.

# Built-in Commands
1. cd
The cd command is used to navigate the directory.
> cd [directory]
If a directory is not specified, the shell will navigate to the home directory.

2. pwd
The pwd command is used to print the current directory path.
> pwd

3. File Redirections
> cat file1.txt > file2.txt
> cat file1.txt >> file2.txt

4. Piping
> ls -la | grep .txt

5. wildcard searching
> ls foo*bar.txt

# Extensions
1. Escape Sequences
This shell supports escape sequences, such as \n, <, >, |, and *.

2. Home Directory
This shell supports the use of the ~ symbol to represent the home directory.

Limitations
The shell does not support the use of environment variables.
The shell does not support history commands
