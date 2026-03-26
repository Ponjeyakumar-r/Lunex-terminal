# LUNEX Terminal

A custom shell implementation with a built-in web terminal interface.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%2FWSL-green.svg)

## Features

### Shell Commands
- `where` / `pwd` - Print working directory
- `say` / `echo` - Print text to terminal
- `goto` / `cd` - Change directory
- `create` / `mkdir` - Create directories
- `tasks` - List background jobs
- `fg` - Bring job to foreground
- `bg` - Run job in background
- `stop` - Stop a background job
- `quit` / `exit` - Exit the shell
- `let` / `set` - Set environment variables
- `env` - Show environment variables
- `alias` - List command aliases
- `history` - Command history

### Advanced Features
- **Pipes** - `cmd1 | cmd2 | cmd3`
- **Redirections** - `>`, `>>`, `<`, `2>`, `2>>`
- **Compound Commands** - `&&`, `||`, `;`
- **Background Jobs** - Run commands in background with `&`
- **Tilde Expansion** - `~` and `~/path`
- **Variable Expansion** - `$VAR` and `${VAR}`
- **Quote Handling** - Single and double quotes
- **Command History** - Persistent history across sessions

### Web Terminal
- Browser-based terminal interface
- Execute shell commands via web UI
- Responsive design
- Command history in web interface

## Building

```bash
# Clone and build
make

# Or clean build
make clean && make
```

## Running

### Local Shell
```bash
./shell
```

### Web Terminal
```bash
./shell --web
```

Or use the Makefile:
```bash
make run    # Run local shell
make web    # Run web server
```

Access at: `http://localhost:8080`

## Docker Deployment

```bash
# Build Docker image
docker build -t lunex .

# Run container
docker run -d -p 8080:8080 lunex
```

## Deployment to Render

1. Fork or push to GitHub
2. Go to [render.com](https://render.com)
3. Create new "Web Service"
4. Select "Docker" as environment
5. Connect your GitHub repo
6. Set PORT = 8080
7. Deploy

## Usage Examples

```bash
# Basic commands
where
echo Hello World
create myproject
goto myproject

# Pipes and redirections
ls | grep .c > files.txt
echo "hello" >> log.txt

# Compound commands
make && ./shell
cd /tmp || cd /home

# Environment variables
let MYVAR=value
echo $MYVAR

# History
history
history -10
history -c
```

## Project Structure

```
custom_terminal/
├── shell.c          # Main shell implementation
├── shell.h          # Header file
├── shell_web.c      # Web server implementation
├── shell_web.h      # Web header file
├── Makefile         # Build configuration
└── Dockerfile       # Docker configuration
```

## Requirements

- GCC compiler
- Linux/Unix system
- POSIX-compliant shell

## License

MIT License - See LICENSE file for details

## Author

Created by Ponjeyakumar R
