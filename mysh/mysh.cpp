// Importing Libraries

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

// Constant Strings for prompts
const string PROMPT = "mysh> ";
const string WELCOME_MESSAGE = "Welcome to my shell!";
const string ERROR_PROMPT = "!mysh> ";
const string EXIT_CMD = "exit";
const string CD_CMD = "cd";
const string PWD_CMD = "pwd";
int last_exit_status = 0;

// utility function for printing prompts
void print_prompt(bool error) {
  cout << (error ? ERROR_PROMPT : PROMPT);
  cout.flush();
}

/*
Extension 3.1: Escape sequences
This function is implementation of Extension 3.1
*/
string expand_escape_sequences(const string & str) {
  string result;
  bool escape = false;
  bool newline_escape = false;
  for (size_t i = 0; i < str.size(); i++) {
    if (escape) {
      escape = false;
      if (newline_escape && str[i] == '\n') {
        newline_escape = false;
        continue;
      }
      switch (str[i]) {
        case 'n':
          result += '\n';
          break;
        case '<':
        case '>':
        case '|':
        case '*':
          result += str[i];
          break;
        default:
          result += '\\';
          result += str[i];
          break;
      }
    } else {
      if (str[i] == '\\') {
        escape = true;
        if (i + 1 < str.size() && str[i + 1] == ' ') {
          result += '\\';
          i++;
        } else if (i + 1 < str.size() && str[i + 1] == '\n') {
          newline_escape = true;
          i++;
        }
      } else {
        result += str[i];
      }
    }
  }
  return result;
}



/*
Extension 3.2: Home directory
This function is implementing extension 3.2 and
used as helper function by change directory function
*/
string get_home_directory() {
  char * home_dir = getenv("HOME");
  if (home_dir == nullptr) {
    cerr << "Error: HOME environment variable not set" << endl;
    last_exit_status = 1;
    return "";
  }
  return string(home_dir);
}


/*
This is utility function to read command line
*/
bool read_command(string & command) {
  if (cin.eof()) {
    cout << endl;
    return false;
  }
  if (cin.peek() == '\n') {
    cin.ignore();
  }
  getline(cin, command);
  return true;
}

/*
This function parses the command provided to it as arg
extracts tokens from it
*/
void parse_command(const string & command, vector < string > & tokens) {
  tokens.clear();
  istringstream iss(command);
  string token;
  while (iss >> token) {
    token = expand_escape_sequences(token);
    if (token.find('*') != string::npos) {
      // Wildcard expansion
      string prefix = token.substr(0, token.find('*'));
      string suffix = token.substr(token.find('*') + 1);
      DIR * dir = opendir(".");
      if (dir) {
        struct dirent * entry;
        while ((entry = readdir(dir))) {
          string filename = entry -> d_name;
          if (filename.size() >= prefix.size() + suffix.size() &&
              filename.substr(0, prefix.size()) == prefix &&
              filename.substr(filename.size() - suffix.size()) == suffix) {
            tokens.push_back(filename);
          }
        }
        closedir(dir);
      }
    } else {
      tokens.push_back(token);
    }
  }
}


/*
This helper function checks if command provided as arg is
builtin or not
*/
bool is_builtin(const string & cmd) {
  return cmd == CD_CMD || cmd == PWD_CMD;
}

/*
This function change directory
based on args, it decides if args are passed
it will change directory to that path else to home

also it implements EXTENSION 3.2
*/
void change_directory(const vector<string>& args) {
  if (args.size() > 2) {
    cerr << "cd: too many arguments" << endl;
    last_exit_status = 1;
    return;
  }
  string dir;
  if (args.size() == 1) {
      // cd with no arguments: navigate to home directory
      dir = get_home_directory();
      if (dir.empty()) {
          return;
      }
  } else {
      // cd with one argument: navigate to specified directory
      dir = args[1];
  }
  if(dir == "~"){
     dir = get_home_directory();
      if (dir.empty()) {
          return;
      }
  }
  if (chdir(dir.c_str()) == -1) {
    cerr << "cd: " << strerror(errno) << endl;
    last_exit_status = 1;
  }else{
    last_exit_status = 0;
  }
}

/*
This function prints the current directory path
*/
void print_working_directory() {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    cout << cwd << endl;
  } else {
    cerr << "Error getting working directory: " << strerror(errno) << endl;
    last_exit_status = 1;
  }
}

/*
This function is for executing built in commands
*/
bool execute_builtin(const string & cmd,
  const vector < string > & args) {
  if (cmd == CD_CMD) {
    change_directory(args);
    return true;
  }
  if (cmd == PWD_CMD) {
    print_working_directory();
    return true;
  }
  return false;
}


/*
This function executes the external commands passed to it as args
*/
void execute_command(const vector < string > & args) {
  const char ** args_cstr = new
  const char * [args.size() + 1];
  for (size_t i = 0; i < args.size(); i++) {
    args_cstr[i] = args[i].c_str();
  }
  args_cstr[args.size()] = nullptr;

  vector < int > pipes;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "|") {
      int pipefd[2];
      if (pipe(pipefd) < 0) {
        cerr << "Error: pipe failed" << endl;
        last_exit_status = 1;
        delete[] args_cstr;
        return;
      }
      pipes.push_back(pipefd[0]);
      pipes.push_back(pipefd[1]);
      args_cstr[i] = nullptr;
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    cerr << "Error: fork failed" << endl;
    last_exit_status = 1;
  } else if (pid == 0) {
    // Child process
    int in_fd = dup(STDIN_FILENO);
    int out_fd = dup(STDOUT_FILENO);

    bool redirect_input = false;
    bool redirect_output = false;
    bool append_output = false;
    int input_fd, output_fd;

    for (size_t i = 1; i < args.size(); i++) {
      if (args[i] == "<") {
        redirect_input = true;
        input_fd = open(args[i + 1].c_str(), O_RDONLY);
        if (input_fd < 0) {
          cerr << "Error: cannot open file " << args[i + 1] << endl;
          last_exit_status = 1;
          exit(1);
        }
        args_cstr[i] = nullptr;
        args_cstr[i + 1] = nullptr;
        i++;
      } else if (args[i] == ">") {
        redirect_output = true;
        output_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output_fd < 0) {
          cerr << "Error: cannot open file " << args[i + 1] << endl;
          last_exit_status = 1;
          exit(1);
        }
        args_cstr[i] = nullptr;
        args_cstr[i + 1] = nullptr;
        i++;
      }

      else if (args[i] == ">>") {
        redirect_output = true;
        append_output = true;
        output_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output_fd < 0) {
            cerr << "Error: cannot open file " << args[i + 1] << endl;
            last_exit_status = 1;
            exit(1);
        }
        args_cstr[i] = nullptr;
        args_cstr[i + 1] = nullptr;
        i++;
    }

    }

    if (redirect_input) {
      if (dup2(input_fd, STDIN_FILENO) < 0) {
        cerr << "Error: dup2 failed" << endl;
        last_exit_status = 1;
        exit(1);
      }
      close(input_fd);
    }

    if (!pipes.empty()) {
      if (dup2(pipes[1], STDOUT_FILENO) < 0) {
        cerr << "Error: dup2 failed" << endl;
        last_exit_status = 1;
        exit(1);
      }
      close(pipes[0]);
      close(pipes[1]);
    } else if (redirect_output) {
    if (append_output) {
        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            cerr << "Error: dup2 failed" << endl;
            last_exit_status = 1;
            exit(1);
        }
    } else {
        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            cerr << "Error: dup2 failed" << endl;
            last_exit_status = 1;
            exit(1);
        }
        ftruncate(output_fd, 0); // truncate the file if it exists
    }
    close(output_fd);
}

    execvp(args_cstr[0], const_cast < char *
      const * > (args_cstr));
    cerr << "Error: execvp failed" << endl;
    last_exit_status = 1;
    exit(1);
  } else {
    // Parent process
    int status;

    if (!pipes.empty()) {
      pid_t pid2 = fork();
      if (pid2 < 0) {
        cerr << "Error: fork failed" << endl;
        last_exit_status = 1;
      } else if (pid2 == 0) {
        // Child process
        if (dup2(pipes[0], STDIN_FILENO) < 0) {
          cerr << "Error: dup2 failed" << endl;
          last_exit_status = 1;
          exit(1);
        }
        close(pipes[0]);
        close(pipes[1]);
        execvp(args_cstr[pipes.size() + 1], const_cast < char *
          const * > ( & args_cstr[pipes.size() + 1]));
        cerr << "Error: execvp failed" << endl;
        last_exit_status = 1;
        exit(1);
      }
      close(pipes[0]);
      close(pipes[1]);
      waitpid(pid2, & status, 0);
    }

    waitpid(pid, & status, 0);
    last_exit_status = WEXITSTATUS(status);
  }

  delete[] args_cstr;
}



/*

This is our main function that runs our shell in interactive or batch mode
on the basis of argument provided on command line, also it have one input
loop and command parsing algorithm that works for both modes.

*/
int main(int argc, char* argv[]) {
    bool interactive = true;
    ifstream input_file;

    if (argc > 1) {
        input_file.open(argv[1]);
        if (!input_file) {
            cerr << "Error: could not open input file '" << argv[1] << "'" << endl;
            return 1;
        }
        interactive = false;
    }

    string command;
    vector<string> tokens;

    // Print Welcome Message
    if(interactive){
      cout << WELCOME_MESSAGE << endl;
    }

    // Main Loop
    while (true) {
        if (interactive) {
          print_prompt(last_exit_status);
            if (!read_command(command)) {
                break;
            }
        } else {
            if (!getline(input_file, command)) {
                break;
            }

        }
        parse_command(command, tokens);
        if (tokens.empty()) {
            continue;
        }
        string cmd = tokens[0];
        if (cmd == EXIT_CMD) {
            print_prompt(0);
            cout << "exiting" << endl;
            break;
        }
        if (is_builtin(cmd)) {
            execute_builtin(cmd, tokens);
        } else {
            execute_command(tokens);
        }

    }

    if (input_file.is_open()) {
        input_file.close();
    }
    return 0;
}
