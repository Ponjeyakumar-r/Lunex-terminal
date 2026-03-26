#include "shell.h"
#include "shell_web.h"

int create_server_socket(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    listen(sockfd, 5);
    return sockfd;
}

void url_decode(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int hex;
            sscanf(src + 1, "%2x", &hex);
            *dst++ = hex;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

char *parse_http_request(char *buffer, char *path, char *query, char *method_out) {
    char method[16] = {0};
    char uri[256] = {0};
    
    sscanf(buffer, "%s %s", method, uri);
    
    char *query_start = strchr(uri, '?');
    if (query_start) {
        *query_start = '\0';
        strcpy(query, query_start + 1);
    } else {
        query[0] = '\0';
    }
    
    strcpy(path, uri);
    strcpy(method_out, method);
    return method_out;
}

void send_response(int client_fd, const char *status, const char *content_type, const char *body, int body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n", status, content_type, body_len);
    
    send(client_fd, header, header_len, 0);
    send(client_fd, body, body_len, 0);
}

void send_terminal_html(int client_fd) {
    const char *html = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>LUNEX Terminal</title>\n"
"    <style>\n"
"        @import url('https://fonts.googleapis.com/css2?family=Fira+Code:wght@400;500&display=swap');\n"
"        * { box-sizing: border-box; margin: 0; padding: 0; }\n"
"        body { \n"
"            background: linear-gradient(135deg, #1a1f2e 0%, #0d1117 100%); \n"
"            min-height: 100vh; \n"
"            display: flex; \n"
"            justify-content: center; \n"
"            align-items: center; \n"
"            padding: 20px;\n"
"            font-family: 'Fira Code', 'Consolas', monospace;\n"
"        }\n"
"        .terminal-container { \n"
"            width: 100%; \n"
"            max-width: 900px; \n"
"            background: #1e1e2e; \n"
"            border-radius: 12px; \n"
"            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(255,255,255,0.1);\n"
"            overflow: hidden;\n"
"        }\n"
"        .terminal-header {\n"
"            background: linear-gradient(180deg, #2d2d3f 0%, #1e1e2e 100%);\n"
"            padding: 12px 16px;\n"
"            display: flex;\n"
"            align-items: center;\n"
"            border-bottom: 1px solid rgba(255,255,255,0.05);\n"
"        }\n"
"        .terminal-buttons { display: flex; gap: 8px; }\n"
"        .terminal-btn { width: 12px; height: 12px; border-radius: 50%; }\n"
"        .btn-red { background: #ff5f56; }\n"
"        .btn-yellow { background: #ffbd2e; }\n"
"        .btn-green { background: #27c93f; }\n"
"        .terminal-title { flex: 1; text-align: center; color: #6c7086; font-size: 13px; }\n"
"        .terminal-body { padding: 16px; min-height: 450px; max-height: 600px; display: flex; flex-direction: column; }\n"
"        .logo { text-align: center; margin-bottom: 20px; }\n"
"        .logo h1 { \n"
"            background: linear-gradient(90deg, #89b4fa, #cba6f7, #f5c2e7); \n"
"            -webkit-background-clip: text; \n"
"            -webkit-text-fill-color: transparent;\n"
"            font-size: 28px; \n"
"            font-weight: 500;\n"
"            letter-spacing: 4px;\n"
"        }\n"
"        .logo p { color: #6c7086; font-size: 12px; margin-top: 4px; }\n"
"        .help-panel { \n"
"            display: none;\n"
"            background: #181825; \n"
"            border-radius: 8px; \n"
"            padding: 12px 16px; \n"
"            margin-bottom: 16px;\n"
"            border: 1px solid #313244;\n"
"        }\n"
"        .help-panel.show { display: block; }\n"
"        .help-toggle {\n"
"            background: #313244;\n"
"            color: #6c7086;\n"
"            border: none;\n"
"            padding: 6px 12px;\n"
"            border-radius: 4px;\n"
"            font-size: 11px;\n"
"            cursor: pointer;\n"
"            margin-bottom: 12px;\n"
"            font-family: inherit;\n"
"        }\n"
"        .help-toggle:hover { background: #45475a; color: #cdd6f4; }\n"
"        .help-title { \n"
"            color: #89b4fa; \n"
"            font-size: 12px; \n"
"            margin-bottom: 12px; \n"
"            font-weight: 500;\n"
"            text-transform: uppercase;\n"
"            letter-spacing: 1px;\n"
"        }\n"
"        .help-grid { \n"
"            display: grid; \n"
"            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); \n"
"            gap: 8px;\n"
"        }\n"
"        .help-cmd { \n"
"            display: flex;\n"
"            align-items: center;\n"
"            gap: 8px;\n"
"            font-size: 12px;\n"
"            padding: 4px 8px;\n"
"            background: #1e1e2e;\n"
"            border-radius: 4px;\n"
"        }\n"
"        .help-cmd span.cmd { \n"
"            color: #f5c2e7; \n"
"            font-weight: 600;\n"
"            background: #313244;\n"
"            padding: 2px 8px;\n"
"            border-radius: 4px;\n"
"            min-width: 70px;\n"
"            text-align: center;\n"
"        }\n"
"        .help-cmd span.desc { \n"
"            color: #cdd6f4;\n"
"        }\n"
"        .output { \n"
"            flex: 1; \n"
"            overflow-y: auto; \n"
"            margin-bottom: 16px; \n"
"            padding-right: 8px;\n"
"        }\n"
"        .output::-webkit-scrollbar { width: 6px; }\n"
"        .output::-webkit-scrollbar-track { background: #1e1e2e; }\n"
"        .output::-webkit-scrollbar-thumb { background: #45475a; border-radius: 3px; }\n"
"        .output-line { margin-bottom: 8px; line-height: 1.6; }\n"
"        .cmd-echo { color: #a6adc8; }\n"
"        .cmd-prompt { color: #a6e3a1; margin-right: 8px; }\n"
"        .cmd-output { color: #cdd6f4; white-space: pre-wrap; }\n"
"        .input-line { \n"
"            display: flex; \n"
"            align-items: center; \n"
"            background: #181825; \n"
"            border-radius: 8px; \n"
"            padding: 12px 16px;\n"
"            border: 1px solid #313244;\n"
"        }\n"
"        .input-line:focus-within { border-color: #89b4fa; box-shadow: 0 0 0 2px rgba(137,180,250,0.2); }\n"
"        .prompt { color: #a6e3a1; margin-right: 8px; font-weight: 500; }\n"
"        input { \n"
"            flex: 1; \n"
"            background: transparent; \n"
"            border: none; \n"
"            color: #cdd6f4; \n"
"            font-family: inherit; \n"
"            font-size: 14px; \n"
"            outline: none;\n"
"        }\n"
"        input::placeholder { color: #6c7086; }\n"
"        button { \n"
"            background: linear-gradient(135deg, #89b4fa 0%, #74c7ec 100%); \n"
"            color: #1e1e2e; \n"
"            border: none; \n"
"            padding: 8px 20px; \n"
"            border-radius: 6px; \n"
"            cursor: pointer; \n"
"            font-family: inherit;\n"
"            font-weight: 500;\n"
"            margin-left: 12px;\n"
"            transition: all 0.2s;\n"
"        }\n"
"        button:hover { transform: translateY(-1px); box-shadow: 0 4px 12px rgba(137,180,250,0.3); }\n"
"        .error { color: #f38ba8; }\n"
"        .success { color: #a6e3a1; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"terminal-container\">\n"
"        <div class=\"terminal-header\">\n"
"            <div class=\"terminal-buttons\">\n"
"                <div class=\"terminal-btn btn-red\"></div>\n"
"                <div class=\"terminal-btn btn-yellow\"></div>\n"
"                <div class=\"terminal-btn btn-green\"></div>\n"
"            </div>\n"
"            <div class=\"terminal-title\">LUNEX Terminal — bash</div>\n"
"        </div>\n"
"        <div class=\"terminal-body\">\n"
"            <div class=\"logo\">\n"
"                <h1>▓▓▓▓▓ LUNEX ▓▓▓▓▓</h1>\n"
"                <p>Web Terminal Interface</p>\n"
"            </div>\n"
"            <button class=\"help-toggle\" onclick=\"toggleHelp()\">Show Commands</button>\n"
"            <div class=\"help-panel\" id=\"helpPanel\">\n"
"                <div class=\"help-title\">Available Commands</div>\n"
"                <div class=\"help-grid\">\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">where</span> <span class=\"desc\">Print working directory</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">pwd</span> <span class=\"desc\">Print working directory</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">say</span> <span class=\"desc\">Print text to terminal</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">echo</span> <span class=\"desc\">Print text to terminal</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">ls</span> <span class=\"desc\">List files and directories</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">create</span> <span class=\"desc\">Create a directory</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">goto</span> <span class=\"desc\">Change directory</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">tasks</span> <span class=\"desc\">List background jobs</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">fg</span> <span class=\"desc\">Bring job to foreground</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">bg</span> <span class=\"desc\">Run job in background</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">stop</span> <span class=\"desc\">Stop a background job</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">let</span> <span class=\"desc\">Set environment variable</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">env</span> <span class=\"desc\">Show environment variables</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">set</span> <span class=\"desc\">Set shell options</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">alias</span> <span class=\"desc\">List command aliases</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">history</span> <span class=\"desc\">Show command history</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">quit</span> <span class=\"desc\">Exit the shell</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">whoami</span> <span class=\"desc\">Print current user</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">date</span> <span class=\"desc\">Print current date</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">uname</span> <span class=\"desc\">Print system information</span></div>\n"
"                    <div class=\"help-cmd\"><span class=\"cmd\">cat</span> <span class=\"desc\">Print file contents</span></div>\n"
"                </div>\n"
"            </div>\n"
"            <div class=\"output\" id=\"output\"></div>\n"
"            <div class=\"input-line\">\n"
"                <span class=\"prompt\">❯</span>\n"
"                <input type=\"text\" id=\"cmd\" placeholder=\"Type a command...\" autocomplete=\"off\">\n"
"                <button onclick=\"execute()\">Run</button>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    <script>\n"
"        let history = [];\n"
"        let historyIndex = -1;\n"
"\n"
"        function execute() {\n"
"            const input = document.getElementById('cmd');\n"
"            const cmd = input.value.trim();\n"
"            if (!cmd) return;\n"
"\n"
"            history.push(cmd);\n"
"            historyIndex = history.length;\n"
"\n"
"            fetch('/exec?cmd=' + encodeURIComponent(cmd))\n"
"                .then(r => r.text())\n"
"                .then(data => {\n"
"                    const output = document.getElementById('output');\n"
"                    output.innerHTML += '<div class=\"output-line\"><span class=\"cmd-prompt\">❯</span><span class=\"cmd-echo\">' + cmd + '</span></div>';\n"
"                    output.innerHTML += '<div class=\"output-line cmd-output\">' + (data ? data.replace(/</g, '&lt;').replace(/>/g, '&gt;') : ' ') + '</div>';\n"
"                    output.scrollTop = output.scrollHeight;\n"
"                    input.value = '';\n"
"                })\n"
"                .catch(err => {\n"
"                    document.getElementById('output').innerHTML += '<div class=\"output-line error\">Error: ' + err + '</div>';\n"
"                });\n"
"        }\n"
"\n"
"        document.getElementById('cmd').addEventListener('keypress', function(e) {\n"
"            if (e.key === 'Enter') execute();\n"
"        });\n"
"\n"
"        document.getElementById('cmd').focus();\n"
"\n"
"        function toggleHelp() {\n"
"            var panel = document.getElementById('helpPanel');\n"
"            var btn = document.querySelector('.help-toggle');\n"
"            if (panel.classList.contains('show')) {\n"
"                panel.classList.remove('show');\n"
"                btn.textContent = 'Show Commands';\n"
"            } else {\n"
"                panel.classList.add('show');\n"
"                btn.textContent = 'Hide Commands';\n"
"            }\n"
"        }\n"
"\n"
"        document.getElementById('cmd').addEventListener('keydown', function(e) {\n"
"            if (e.key === 'ArrowUp') {\n"
"                if (historyIndex > 0) {\n"
"                    historyIndex--;\n"
"                    this.value = history[historyIndex];\n"
"                }\n"
"                e.preventDefault();\n"
"            } else if (e.key === 'ArrowDown') {\n"
"                if (historyIndex < history.length - 1) {\n"
"                    historyIndex++;\n"
"                    this.value = history[historyIndex];\n"
"                } else {\n"
"                    historyIndex = history.length;\n"
"                    this.value = '';\n"
"                }\n"
"                e.preventDefault();\n"
"            }\n"
"        });\n"
"    </script>\n"
"</body>\n"
"</html>";
    
    send_response(client_fd, "200 OK", "text/html", html, strlen(html));
}

int is_ansi_line(const char *line) {
    while (*line) {
        if (*line == '\033' && line[1] == '[') {
            return 1;
        }
        if (*line == '[' && (line[1] >= '0' && line[1] <= '9')) {
            return 1;
        }
        line++;
    }
    return 0;
}

static int starts_with_ansi(const char *line) {
    return line[0] == '\033';
}

void handle_api_request(int client_fd, char *query) {
    char cmd[MAX_LINE] = {0};
    char *cmd_start = strstr(query, "cmd=");
    if (!cmd_start) {
        send_response(client_fd, "400 Bad Request", "text/plain", "Missing cmd parameter", 21);
        return;
    }
    
    strcpy(cmd, cmd_start + 4);
    url_decode(cmd);
    
    char shell_cmd[MAX_LINE + 64];
    snprintf(shell_cmd, sizeof(shell_cmd), "echo '%s' | ./shell 2>&1", cmd);
    
    FILE *p = popen(shell_cmd, "r");
    if (!p) {
        send_response(client_fd, "500 Internal Error", "text/plain", "Command execution failed", 27);
        return;
    }
    
    char output[4096] = {0};
    char line[256];
    int found_output = 0;
    while (fgets(line, sizeof(line), p)) {
        if (strstr(line, "exit") || strstr(line, "quit")) continue;
        if (!found_output) {
            if (strstr(line, "────") || strstr(line, "════")) {
                found_output = 1;
            }
            continue;
        }
        if (strstr(line, "Commands:") || strstr(line, "Features:")) {
            continue;
        }
        if (strstr(line, "│") || strstr(line, "Print ")) {
            continue;
        }
        if (line[0] == '\033') {
            continue;
        }
        if (strlen(line) > 1) {
            strcat(output, line);
        }
    }
    pclose(p);
    
    send_response(client_fd, "200 OK", "text/plain", output, strlen(output));
}

void handle_web_client(int client_fd) {
    char buffer[4096] = {0};
    char ip[32] = {0};
    
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr *)&addr, &len);
    snprintf(ip, sizeof(ip), "%s", inet_ntoa(addr.sin_addr));
    
    recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    char path[256] = {0};
    char query[1024] = {0};
    char method[16] = {0};
    parse_http_request(buffer, path, query, method);
    
    if (strcmp(path, "/exec") == 0 && strcmp(method, "GET") == 0) {
        handle_api_request(client_fd, query);
    } else if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_terminal_html(client_fd);
    } else {
        send_response(client_fd, "404 Not Found", "text/plain", "Not Found", 9);
    }
    
    close(client_fd);
}

void start_web_server(void) {
    int server_fd = create_server_socket(WEB_PORT);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to start web server on port %d\n", WEB_PORT);
        return;
    }
    
    printf("\n");
    printf("\033[38;5;45m═══════════════════════════════════════════════════════════\033[0m\n");
    printf("                    🌐 LUNEX WEB TERMINAL\n");
    printf("\033[38;5;45m═══════════════════════════════════════════════════════════\033[0m\n");
    printf("\n");
    printf("  \033[38;5;82mAccess from any device:\033[0m\n");
    printf("    \033[38;5;45mhttp://localhost:%d\033[0m\n", WEB_PORT);
    printf("\n");
    printf("  \033[38;5;82mFrom another computer:\033[0m\n");
    printf("    \033[38;5;45mhttp://<YOUR-IP>:%d\033[0m\n", WEB_PORT);
    printf("\n");
    printf("  \033[38;5;82mPress Ctrl+C to stop the web server\033[0m\n");
    printf("\n");
    printf("\033[38;5;45m═══════════════════════════════════════════════════════════\033[0m\n");
    printf("\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd >= 0) {
            handle_web_client(client_fd);
        }
    }
}