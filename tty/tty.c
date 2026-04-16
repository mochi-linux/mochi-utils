#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

#define PROGRAM_NAME "tty"
#define PROGRAM_VERSION "2.0 (Advanced PTY)"

/* Globals used by the SIGWINCH handler */
static volatile int g_master_fd  = -1;

static void sigwinch_handler(int sig) {
    (void)sig;
    if (g_master_fd >= 0) {
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0)
            ioctl(g_master_fd, TIOCSWINSZ, &ws);
    }
}

static struct option long_options[] = {
    {"silent", no_argument, 0, 's'},
    {"quiet", no_argument, 0, 's'},
    {"info", no_argument, 0, 'i'},
    {"exec", required_argument, 0, 'e'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
};

static struct termios orig_termios;
static int termios_saved = 0;

static void reset_terminal_mode(void) {
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }
}

static void set_raw_mode(void) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return;
    }
    termios_saved = 1;
    atexit(reset_terminal_mode);
    
    raw = orig_termios;
    
    /* Input modes - clear indicated ones */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    /* Output modes - clear giving: no post-processing */
    raw.c_oflag &= ~(OPOST);
    
    /* Control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    
    /* Local modes - clear giving: echoing off, canonical off, no extended functions, no signal chars */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    /* Control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void print_help(void) {
    printf("Usage: %s [OPTION]...\n", PROGRAM_NAME);
    printf("Print the file name of the terminal connected to standard input, or run a PTY.\n\n");
    printf("  -s, --silent, --quiet   print nothing, only return an exit status\n");
    printf("  -i, --info              print extended information about the terminal\n");
    printf("  -e, --exec COMMAND      execute COMMAND in a new pseudo-terminal (PTY)\n");
    printf("  -h, --help              display this help and exit\n");
    printf("  -v, --version           output version information and exit\n");
    printf("\nExit status:\n");
    printf("  0 if standard input is a terminal (when not using -e)\n");
    printf("  1 if standard input is not a terminal (when not using -e)\n");
    printf("  2 if given an unrecognized option\n");
    printf("  3 if an error occurs\n");
}

static speed_t baud_to_int(speed_t speed) {
    switch (speed) {
        case B0:      return 0;
        case B50:     return 50;
        case B75:     return 75;
        case B110:    return 110;
        case B134:    return 134;
        case B150:    return 150;
        case B200:    return 200;
        case B300:    return 300;
        case B600:    return 600;
        case B1200:   return 1200;
        case B1800:   return 1800;
        case B2400:   return 2400;
        case B4800:   return 4800;
        case B9600:   return 9600;
        case B19200:  return 19200;
        case B38400:  return 38400;
        case B57600:  return 57600;
        case B115200: return 115200;
        case B230400: return 230400;
#ifdef B460800
        case B460800: return 460800;
#endif
#ifdef B921600
        case B921600: return 921600;
#endif
#ifdef B1000000
        case B1000000: return 1000000;
#endif
#ifdef B1500000
        case B1500000: return 1500000;
#endif
#ifdef B2000000
        case B2000000: return 2000000;
#endif
        default: return speed;
    }
}

static void print_extended_info(int fd) {
    struct termios term;
    struct winsize ws;
    pid_t pgrp;
    
    printf("\n--- Extended Terminal Information ---\n");
    
    if (tcgetattr(fd, &term) == 0) {
        printf("Input Baud Rate  : %u\n", (unsigned int)baud_to_int(cfgetispeed(&term)));
        printf("Output Baud Rate : %u\n", (unsigned int)baud_to_int(cfgetospeed(&term)));
        
        printf("Local Modes      : ");
        if (term.c_lflag & ICANON) printf("ICANON ");
        if (term.c_lflag & ECHO) printf("ECHO ");
        if (term.c_lflag & ECHOE) printf("ECHOE ");
        if (term.c_lflag & ISIG) printf("ISIG ");
        if (term.c_lflag & IEXTEN) printf("IEXTEN ");
        printf("\n");

        printf("Input Modes      : ");
        if (term.c_iflag & ICRNL) printf("ICRNL ");
        if (term.c_iflag & INLCR) printf("INLCR ");
        if (term.c_iflag & IGNCR) printf("IGNCR ");
        if (term.c_iflag & IXON) printf("IXON ");
        if (term.c_iflag & IXOFF) printf("IXOFF ");
        printf("\n");
        
        printf("Output Modes     : ");
        if (term.c_oflag & OPOST) printf("OPOST ");
        if (term.c_oflag & ONLCR) printf("ONLCR ");
        printf("\n");

        printf("Control Modes    : ");
        if (term.c_cflag & PARENB)  printf("PARENB ");
        if (term.c_cflag & PARODD)  printf("PARODD ");
        if (term.c_cflag & CSTOPB)  printf("CSTOPB ");
        if (term.c_cflag & CREAD)   printf("CREAD ");
        if (term.c_cflag & HUPCL)   printf("HUPCL ");
        if (term.c_cflag & CLOCAL)  printf("CLOCAL ");
#ifdef CRTSCTS
        if (term.c_cflag & CRTSCTS) printf("CRTSCTS ");
#endif
        printf("\n");

        printf("Special Chars    :");
        if (term.c_cc[VINTR]  < 32) printf(" INTR=^%c",  'A' + term.c_cc[VINTR]  - 1);
        if (term.c_cc[VQUIT]  < 32) printf(" QUIT=^%c",  'A' + term.c_cc[VQUIT]  - 1);
        if (term.c_cc[VERASE] < 32) printf(" ERASE=^%c", 'A' + term.c_cc[VERASE] - 1);
        if (term.c_cc[VKILL]  < 32) printf(" KILL=^%c",  'A' + term.c_cc[VKILL]  - 1);
        if (term.c_cc[VEOF]   < 32) printf(" EOF=^%c",   'A' + term.c_cc[VEOF]   - 1);
        if (term.c_cc[VSUSP]  < 32) printf(" SUSP=^%c",  'A' + term.c_cc[VSUSP]  - 1);
        printf("\n");
    }

    if (ioctl(fd, TIOCGWINSZ, &ws) == 0) {
        printf("Window Size      : %d columns, %d rows\n", ws.ws_col, ws.ws_row);
        printf("Pixel Size       : %d x %d\n", ws.ws_xpixel, ws.ws_ypixel);
    }

    pgrp = tcgetpgrp(fd);
    if (pgrp != -1) {
        printf("Foreground PGRP  : %d\n", pgrp);
    }
    printf("-------------------------------------\n");
}

static int run_pty(const char *cmd) {
    int master_fd, slave_fd;
    char *slavename;
    pid_t pid;
    struct winsize ws;
    int has_ws = 0;

    if (isatty(STDIN_FILENO) && ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        has_ws = 1;
    }

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd == -1) {
        perror("posix_openpt");
        return 3;
    }

    if (grantpt(master_fd) == -1 || unlockpt(master_fd) == -1) {
        perror("grantpt/unlockpt");
        close(master_fd);
        return 3;
    }

    slavename = ptsname(master_fd);
    if (!slavename) {
        perror("ptsname");
        close(master_fd);
        return 3;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(master_fd);
        return 3;
    }

    if (pid == 0) {
        /* Child process */
        close(master_fd);
        
        /* Create new session */
        setsid();
        
        slave_fd = open(slavename, O_RDWR);
        if (slave_fd == -1) {
            perror("open slave");
            exit(1);
        }

#ifdef TIOCSCTTY
        /* Acquire controlling tty */
        if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
            /* ignore error, could already be controlling tty */
        }
#endif

        if (has_ws) {
            ioctl(slave_fd, TIOCSWINSZ, &ws);
        }

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO) {
            close(slave_fd);
        }

        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        perror("execlp");
        exit(1);
    }

    /* Parent process: register resize handler and enter raw mode */
    g_master_fd = master_fd;
    signal(SIGWINCH, sigwinch_handler);
    /* Propagate current window size immediately */
    sigwinch_handler(0);
    if (isatty(STDIN_FILENO)) {
        set_raw_mode();
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = master_fd;
    fds[1].events = POLLIN;

    char buf[8192];
    int status;
    
    while (1) {
        if (poll(fds, 2, -1) == -1) {
            if (errno == EINTR) continue;
            break;
        }

        if (fds[0].revents & POLLIN) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(master_fd, buf + written, n - written);
                if (w <= 0) break;
                written += w;
            }
        }

        if (fds[1].revents & POLLIN) {
            ssize_t n = read(master_fd, buf, sizeof(buf));
            if (n <= 0) break;
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
                if (w <= 0) break;
                written += w;
            }
        }
        
        /* If slave was closed/hung up, exit loop */
        if ((fds[1].revents & (POLLERR | POLLHUP)) && !(fds[1].revents & POLLIN)) {
            break;
        }

        /* Check if child has exited to avoid hanging if the PTY doesn't emit HUP */
        if (waitpid(pid, &status, WNOHANG) > 0) {
            break;
        }
    }
    
    close(master_fd);
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 3;
}

int main(int argc, char **argv) {
    int silent = 0;
    int info = 0;
    char *exec_cmd = NULL;
    int opt;
    int status = 0;

    while ((opt = getopt_long(argc, argv, "se:ihv", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': silent = 1; break;
            case 'i': info = 1; break;
            case 'e': exec_cmd = optarg; break;
            case 'h': print_help(); return 0;
            case 'v': printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION); return 0;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
                return 2;
        }
    }

    if (exec_cmd) {
        return run_pty(exec_cmd);
    }

    if (optind < argc) {
        fprintf(stderr, "%s: extra operand '%s'\n", PROGRAM_NAME, argv[optind]);
        fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
        return 2;
    }

    int fd = STDIN_FILENO;
    if (isatty(fd)) {
        char *tty_name = ttyname(fd);
        if (tty_name) {
            if (!silent) {
                printf("%s\n", tty_name);
                if (info) {
                    print_extended_info(fd);
                }
            }
        } else {
            if (!silent) perror("ttyname");
            status = 3;
        }
    } else {
        if (!silent) printf("not a tty\n");
        status = 1;
    }

    return status;
}