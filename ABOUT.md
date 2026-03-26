# About LUNEX Terminal

## What is LUNEX?

LUNEX is a custom shell implementation written in C that provides both a command-line interface and a web-based terminal. It's designed as a learning project to understand how shells work internally.

## Why LUNEX?

This project demonstrates:
- Process management and job control
- Signal handling
- Pipe and redirection implementation
- Command parsing and execution
- HTTP server implementation for web interface

## Technical Details

### Shell Implementation
- Custom command parser supporting multiple operators
- Built-in commands implemented in C
- External command execution via `execvp()`
- Job control with foreground/background processes
- Signal handling for SIGCHLD, SIGINT, SIGTSTP

### Web Server
- Raw socket HTTP server
- URL decoding for command execution
- HTML/CSS/JavaScript terminal UI
- Real-time command execution via CGI-like pattern

## Capabilities

### Command Parsing
- Tokenization with quote handling
- Variable expansion (`$VAR`, `${VAR}`)
- Tilde expansion (`~`, `~/path`)
- Escape sequence handling

### I/O Redirection
- `>` - Output redirection (truncate)
- `>>` - Output redirection (append)
- `<` - Input redirection
- `2>` - Error redirection (truncate)
- `2>>` - Error redirection (append)

### Piping
- Multi-stage pipes: `cmd1 | cmd2 | cmd3`
- Combines stdout of one command to stdin of next

### Job Control
- Background execution with `&`
- `fg` - Bring to foreground
- `bg` - Run in background
- `stop` - Stop a job
- `jobs` - List all jobs

### History
- Persistent history (saved to `~/.lunex_history`)
- View last N commands
- Delete specific entries
- Clear all history

## Future Enhancements

Potential improvements:
- Tab completion
- Searchable history (Ctrl+R)
- Wildcard globbing
- Shell functions
- Better alias support
- Scripting support

## Acknowledgments

Inspired by:
- Thompson shell
- Bash shell
- Various UNIX shell tutorials

## Contributing

Feel free to fork and contribute! This is an educational project to understand shell internals.

## Contact

For questions or suggestions, open an issue on GitHub.
