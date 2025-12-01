/*
 * C X11 GUI for Terminal App - Simple Version
 * Spawns ./bin/terminal_app and displays output in an X11 window.
 * 
 * Compile:
 *   gcc -o gui_terminal gui_terminal.c -lX11 -lpthread
 * 
 * Run:
 *   ./gui_terminal
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <pwd.h>
#include <limits.h>

typedef struct {
    Display *display;
    int screen;
    Window window;
    GC gc;
    Atom wm_delete;
    
    char output[65536];
    int output_len;
    char input_line[256];
    int input_len;
    
    pid_t child_pid;
    int stdin_fd;
    int stdout_fd;
    pthread_t reader_thread;
    int running;
    
    int width;
    int height;
} AppState;

/* Read from child process in background thread */
static void *reader_thread_func(void *arg) {
    AppState *state = (AppState *)arg;
    char buf[512];
    ssize_t n;
    
    while (state->running) {
        /* Use non-blocking read with proper error handling */
        n = read(state->stdout_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            
            /* Check for clear screen ANSI escape code */
            if (strstr(buf, "\033[2J\033[H") != NULL) {
                /* Clear the output buffer */
                state->output[0] = '\0';
                state->output_len = 0;
                /* Continue to avoid adding the escape code itself */
                usleep(50000);
                continue;
            }
            
            if (state->output_len + n < (int)sizeof(state->output) - 1) {
                /* Handle ANSI clear code specially (also handle from subprocess) */
                if (strstr(buf, "\033[2J") != NULL || strstr(buf, "\033[H") != NULL) {
                    state->output[0] = '\0';
                    state->output_len = 0;
                }
                strcat(state->output, buf);
                state->output_len += n;
            }
        } else if (n == 0) {
            /* EOF — process closed stdout */
            break;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            /* Real error */
            break;
        }
        usleep(50000); /* 50ms */
    }
    return NULL;
}

/* Spawn terminal app subprocess */
static int spawn_app(AppState *state) {
    int stdin_pipe[2], stdout_pipe[2];
    pid_t pid;
    
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
        perror("pipe");
        return 0;
    }
    
    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 0;
    }
    
    if (pid == 0) {
        /* Child process */
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        execl("./bin/terminal_app", "terminal_app", NULL);
        perror("execl");
        exit(1);
    }
    
    /* Parent process */
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    
    state->child_pid = pid;
    state->stdin_fd = stdin_pipe[1];
    state->stdout_fd = stdout_pipe[0];
    
    fcntl(state->stdout_fd, F_SETFL, O_NONBLOCK);
    
    state->running = 1;
    pthread_create(&state->reader_thread, NULL, reader_thread_func, state);
    
    return 1;
}

/* Helper: map ANSI color codes to X11 colors */
static unsigned long ansi_to_xcolor(int ansi_code) {
    switch (ansi_code) {
        case 30: case 90: return 0x555555;  /* black/bright black */
        case 31: case 91: return 0xff5555;  /* red/bright red */
        case 32: case 92: return 0x55ff55;  /* green/bright green */
        case 33: case 93: return 0xffff55;  /* yellow/bright yellow */
        case 34: case 94: return 0x5555ff;  /* blue/bright blue */
        case 35: case 95: return 0xff55ff;  /* magenta/bright magenta */
        case 36: case 96: return 0x55ffff;  /* cyan/bright cyan */
        case 37: case 97: return 0xffffff;  /* white/bright white */
        default: return 0xd4d4d4;  /* default light text */
    }
}

/* Draw a line with possible ANSI color codes embedded */
static void draw_line_with_ansi(Display *display, Drawable window, GC gc, 
                                 int x, int y, const char *line, int line_len) {
    unsigned long current_color = 0xd4d4d4;
    int i = 0;
    char buf[512];
    int buf_len = 0;

    while (i < line_len) {
        if (line[i] == '\x1b' && i + 1 < line_len && line[i+1] == '[') {
            /* Found ANSI escape sequence */
            /* Flush current buffer */
            if (buf_len > 0) {
                XSetForeground(display, gc, current_color);
                XDrawString(display, window, gc, x, y, buf, buf_len);
                x += (buf_len * 8);
                buf_len = 0;
            }
            
            /* Parse escape sequence */
            i += 2;
            int code = 0;
            while (i < line_len && line[i] >= '0' && line[i] <= '9') {
                code = code * 10 + (line[i] - '0');
                i++;
            }
            
            /* Handle 'm' (SGR - Select Graphic Rendition) */
            if (i < line_len && line[i] == 'm') {
                if (code == 0) {
                    current_color = 0xd4d4d4;  /* reset */
                } else if (code >= 30 && code <= 97) {
                    current_color = ansi_to_xcolor(code);
                }
                i++;
            }
        } else if (line[i] == '\n') {
            break;  /* stop at newline */
        } else {
            /* Regular character */
            if (buf_len < (int)sizeof(buf) - 1) {
                buf[buf_len++] = line[i];
            }
            i++;
        }
    }

    /* Flush remaining buffer */
    if (buf_len > 0) {
        XSetForeground(display, gc, current_color);
        XDrawString(display, window, gc, x, y, buf, buf_len);
    }
}

/* Draw the window contents */
static void draw_window(AppState *state) {
    /* Dark background (dark gray) */
    unsigned long dark_bg = 0x1e1e1e;
    unsigned long light_text = 0xd4d4d4;
    unsigned long title_color = 0x0099cc;
    
    XSetForeground(state->display, state->gc, dark_bg);
    XFillRectangle(state->display, state->window, state->gc, 0, 0, state->width, state->height);
    
    /* Draw title */
    XSetForeground(state->display, state->gc, title_color);
    XDrawString(state->display, state->window, state->gc, 10, 25, "C Terminal App - X11 GUI", 24);
    
    /* Draw output box (dark with border) */
    XSetForeground(state->display, state->gc, 0x333333);
    XFillRectangle(state->display, state->window, state->gc, 10, 50, state->width - 20, state->height - 150);
    XSetForeground(state->display, state->gc, 0x666666);
    XDrawRectangle(state->display, state->window, state->gc, 10, 50, state->width - 20, state->height - 150);
    
    /* Draw output text (light text on dark background, with ANSI color support) */
    const char *text = state->output;
    int y = 70;
    const int max_visible_lines = (state->height - 150) / 15;
    int text_len = strlen(text);
    
    if (text_len > 0) {
        /* Count total lines to determine starting position */
        int total_lines = 0;
        for (int i = 0; i < text_len; i++) {
            if (text[i] == '\n') total_lines++;
        }
        
        /* Calculate which line to start from */
        int skip_lines = (total_lines > max_visible_lines) ? (total_lines - max_visible_lines) : 0;
        int current_line = 0;
        int i = 0;
        
        /* Skip to the starting line */
        while (i < text_len && current_line < skip_lines) {
            if (text[i] == '\n') current_line++;
            i++;
        }
        
        /* Now draw the visible lines */
        while (i < text_len && y < state->height - 90) {
            /* Find the end of this line */
            int line_start = i;
            while (i < text_len && text[i] != '\n') {
                i++;
            }
            
            /* Draw the line with ANSI color support */
            int line_len = i - line_start;
            if (line_len > 0) {
                if (line_len > 255) line_len = 255;
                char buf[512];
                strncpy(buf, &text[line_start], line_len);
                buf[line_len] = '\0';
                draw_line_with_ansi(state->display, state->window, state->gc, 20, y, buf, line_len);
            }
            
            y += 15;
            
            /* Skip the newline */
            if (i < text_len && text[i] == '\n') {
                i++;
            }
        }
    }
    
    /* Draw input prompt and entry box (dark with border) */
    XSetForeground(state->display, state->gc, 0x2d2d2d);
    XFillRectangle(state->display, state->window, state->gc, 10, state->height - 70, state->width - 20, 30);
    XSetForeground(state->display, state->gc, 0x666666);
    XDrawRectangle(state->display, state->window, state->gc, 10, state->height - 70, state->width - 20, 30);
    
    /* Build prompt: user@host:cwd$  - render user@host in green, cwd in blue */
    char host[256];
    char cwd[1024];
    const char *user = "user";
    struct passwd *pw = getpwuid(getuid());
    if (pw) user = pw->pw_name;
    if (gethostname(host, sizeof(host)) != 0) strncpy(host, "host", sizeof(host));
    if (getcwd(cwd, sizeof(cwd)) == NULL) strncpy(cwd, "~", sizeof(cwd));

    char prompt1[256];
    char prompt2[256];
    snprintf(prompt1, sizeof(prompt1), "%s@%s:", user, host);
    snprintf(prompt2, sizeof(prompt2), "%s$ ", cwd);

    /* user@host in green */
    XSetForeground(state->display, state->gc, 0x66ff66);
    XDrawString(state->display, state->window, state->gc, 20, state->height - 48, prompt1, strlen(prompt1));
    /* cwd in blue */
    XSetForeground(state->display, state->gc, 0x66a3ff);
    XDrawString(state->display, state->window, state->gc, 20 + (strlen(prompt1) * 8), state->height - 48, prompt2, strlen(prompt2));
    /* input text in light color */
    XSetForeground(state->display, state->gc, light_text);
    XDrawString(state->display, state->window, state->gc, 20 + ((strlen(prompt1) + strlen(prompt2)) * 8), state->height - 48, state->input_line, state->input_len);
    
    /* Draw cursor (blinking line) */
    if ((time(NULL) % 2) == 0) {  /* Simple blink every 2 seconds */
        XDrawLine(state->display, state->window, state->gc, 
                  40 + (state->input_len * 8), state->height - 55,
                  40 + (state->input_len * 8), state->height - 42);
    }
    
    /* Draw status bar */
    XSetForeground(state->display, state->gc, 0x1a1a1a);
    XFillRectangle(state->display, state->window, state->gc, 0, state->height - 15, state->width, 15);
    XSetForeground(state->display, state->gc, 0x888888);
    XDrawString(state->display, state->window, state->gc, 10, state->height - 3, "Type commands and press Enter. Close window or type 'exit' to quit.", 66);
    
    XFlush(state->display);
}

/* Handle keyboard input */
static void handle_key(AppState *state, KeySym key, char *str) {
    if (key == XK_Return) {
        /* Send command */
        if (state->input_len > 0) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%s\n", state->input_line);
            
            /* Check if child process is still running */
            if (state->child_pid > 0) {
                int status = 0;
                pid_t result = waitpid(state->child_pid, &status, WNOHANG);
                
                if (result == 0) {
                    /* Process still running */
                    ssize_t n = write(state->stdin_fd, buf, strlen(buf));
                    if (n < 0) {
                        perror("write to stdin");
                        char errmsg[256];
                        snprintf(errmsg, sizeof(errmsg), "[Error: failed to send command]\n");
                        if (state->output_len + strlen(errmsg) < (int)sizeof(state->output) - 1) {
                            strcat(state->output, errmsg);
                            state->output_len += strlen(errmsg);
                        }
                    } else {
                        /* Successfully wrote — flush */
                        fsync(state->stdin_fd);
                        
                        /* Add command to output display immediately */
                        if (state->output_len + strlen(state->input_line) + 1 < (int)sizeof(state->output) - 1) {
                            strcat(state->output, state->input_line);
                            strcat(state->output, "\n");
                            state->output_len += strlen(state->input_line) + 1;
                        }
                    }
                } else {
                    /* Process has exited */
                    char msg[256];
                    snprintf(msg, sizeof(msg), "[Process exited with status %d]\n", WEXITSTATUS(status));
                    if (state->output_len + strlen(msg) < (int)sizeof(state->output) - 1) {
                        strcat(state->output, msg);
                        state->output_len += strlen(msg);
                    }
                }
            }
            
            state->input_line[0] = '\0';
            state->input_len = 0;
        }
    } else if (key == XK_BackSpace) {
        if (state->input_len > 0) {
            state->input_line[--state->input_len] = '\0';
        }
    } else if (str && strlen(str) > 0 && state->input_len < (int)sizeof(state->input_line) - 2) {
        state->input_line[state->input_len++] = str[0];
        state->input_line[state->input_len] = '\0';
    }
    
    draw_window(state);
}

int main() {
    /* Ignore SIGPIPE to prevent process death on broken pipe */
    signal(SIGPIPE, SIG_IGN);
    
    AppState *state = malloc(sizeof(AppState));
    memset(state, 0, sizeof(AppState));
    state->width = 800;
    state->height = 500;
    state->running = 1;
    
    /* Open X11 display */
    state->display = XOpenDisplay(NULL);
    if (!state->display) {
        fprintf(stderr, "Cannot open X display\n");
        return 1;
    }
    
    state->screen = DefaultScreen(state->display);
    
    /* Create window */
    state->window = XCreateSimpleWindow(
        state->display,
        RootWindow(state->display, state->screen),
        100, 100, state->width, state->height,
        1,
        BlackPixel(state->display, state->screen),
        0x1e1e1e  /* Dark background color */
    );
    
    /* Set window background to dark gray */
    Colormap cmap = DefaultColormap(state->display, state->screen);
    XColor color;
    color.red = 30 * 256;      /* 30/255 intensity for red */
    color.green = 30 * 256;    /* 30/255 intensity for green */
    color.blue = 30 * 256;     /* 30/255 intensity for blue */
    XAllocColor(state->display, cmap, &color);
    XSetWindowBackground(state->display, state->window, color.pixel);
    
    /* Set window properties */
    XStoreName(state->display, state->window, "C Terminal App - X11 GUI");
    state->wm_delete = XInternAtom(state->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(state->display, state->window, &state->wm_delete, 1);
    
    /* Create graphics context */
    state->gc = XCreateGC(state->display, state->window, 0, NULL);
    XSetForeground(state->display, state->gc, 0);
    XSetBackground(state->display, state->gc, WhitePixel(state->display, state->screen));
    
    /* Select input events */
    XSelectInput(state->display, state->window, ExposureMask | KeyPressMask | StructureNotifyMask | ClientMessage);
    
    /* Map (show) window */
    XMapWindow(state->display, state->window);
    
    /* Spawn terminal app */
    if (!spawn_app(state)) {
        fprintf(stderr, "Failed to spawn terminal app\n");
        return 1;
    }
    
    /* Event loop */
    XEvent event;
    int done = 0;
    
    while (!done) {
        if (XPending(state->display) > 0) {
            XNextEvent(state->display, &event);
            
            switch (event.type) {
                case Expose:
                    draw_window(state);
                    break;
                case KeyPress: {
                    char buf[32];
                    KeySym key;
                    int count = XLookupString(&event.xkey, buf, sizeof(buf), &key, NULL);
                    handle_key(state, key, count > 0 ? buf : NULL);
                    break;
                }
                case ConfigureNotify:
                    state->width = event.xconfigure.width;
                    state->height = event.xconfigure.height;
                    draw_window(state);
                    break;
                case ClientMessage:
                    if ((Atom)event.xclient.data.l[0] == state->wm_delete) {
                        done = 1;
                    }
                    break;
            }
        } else {
            draw_window(state);
            usleep(100000); /* 100ms */
        }
    }
    
    /* Cleanup */
    state->running = 0;
    pthread_join(state->reader_thread, NULL);
    
    if (state->stdin_fd >= 0) close(state->stdin_fd);
    if (state->stdout_fd >= 0) close(state->stdout_fd);
    
    if (state->child_pid > 0) {
        kill(state->child_pid, SIGTERM);
        waitpid(state->child_pid, NULL, WNOHANG);
    }
    
    XFreeGC(state->display, state->gc);
    XDestroyWindow(state->display, state->window);
    XCloseDisplay(state->display);
    
    free(state);
    return 0;
}
