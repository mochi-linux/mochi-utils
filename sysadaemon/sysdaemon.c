#define _GNU_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

/* Path Configurations */
#define HOSTNAME_FILE "/etc/hostname"
#define MODULES_FILE "/etc/modules"
#define STARTUP_DIR "/System/Library/CoreServices/Startup"
#define SERVICES_DIR "/System/Library/CoreServices/Services"

/* 16-Color ANSI Palette mapped from the ref Script */
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"

#define COLOR_PINK "\033[95m"       /* Bright Magenta */
#define COLOR_TEAL "\033[36m"       /* Cyan */
#define COLOR_LAVENDER "\033[94m"   /* Bright Blue */
#define COLOR_YELLOW "\033[93m"     /* Bright Yellow */
#define COLOR_MINT "\033[92m"       /* Bright Green */
#define COLOR_CORAL "\033[91m"      /* Bright Red */
#define COLOR_BLUE "\033[34m"       /* Blue */
#define COLOR_WHITE "\033[97m"      /* Bright White */
#define COLOR_DIM "\033[90m"        /* Bright Black / Gray */
#define COLOR_AMBER "\033[33m"      /* Yellow */

void print_ts() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    long s = ms / 1000;
    long frac = (ms % 1000) / 10;
    long m = s / 60;
    long h = m / 60;
    s = s % 60;
    m = m % 60;

    printf("%s[%s %s%02ld:%02ld:%02ld.%02ld%s %s]%s",
        COLOR_DIM, COLOR_RESET, COLOR_TEAL, h, m, s, frac, COLOR_RESET, COLOR_DIM, COLOR_RESET);
}

void print_badge(const char* color, const char* text) {
    /* Format as [ STATUS ] instead of solid background badges */
    printf("%s[%s %s%s%s %s]%s", COLOR_DIM, COLOR_RESET, color, text, COLOR_RESET, COLOR_DIM, COLOR_RESET);
}

void line_ok(const char* msg) {
    printf("  "); print_ts(); printf(" "); print_badge(COLOR_MINT, "  OK  "); printf("   %s\n", msg);
}

void line_fail(const char* msg) {
    printf("  "); print_ts(); printf(" "); print_badge(COLOR_CORAL, " FAIL "); printf(" %s\n", msg);
}

void line_info(const char* msg) {
    printf("  "); print_ts(); printf(" "); print_badge(COLOR_BLUE, " INFO "); printf(" %s\n", msg);
}

void line_warn(const char* msg) {
    printf("  "); print_ts(); printf(" "); print_badge(COLOR_YELLOW, " WARN "); printf(" %s\n", msg);
}

void line_skip(const char* msg) {
    printf("  "); print_ts(); printf(" "); print_badge(COLOR_WHITE, " SKIP "); printf(" %s%s%s\n", COLOR_DIM, msg, COLOR_RESET);
}

void section(const char* title) {
    printf("\n  %s── %s%s%s%s ──%s%s\n\n", COLOR_DIM, COLOR_LAVENDER, COLOR_BOLD, title, COLOR_RESET, COLOR_DIM, COLOR_RESET);
}

void print_banner() {
    printf("\033[2J\033[H\n"); /* Clear screen and home cursor */
    printf("  %s%s╔══════════════════════════════════════════════╗%s\n", COLOR_PINK, COLOR_BOLD, COLOR_RESET);
    printf("  %s%s║%s  %s%s🍡 mochi-os%s %sv1.0 — systemd 252 (mochi)%s      %s%s║%s\n",
        COLOR_PINK, COLOR_BOLD, COLOR_RESET, COLOR_LAVENDER, COLOR_BOLD, COLOR_RESET, COLOR_DIM, COLOR_RESET, COLOR_PINK, COLOR_BOLD, COLOR_RESET);
    printf("  %s%s║%s  %sBooting into multi-user.target …%s              %s%s║%s\n",
        COLOR_PINK, COLOR_BOLD, COLOR_RESET, COLOR_DIM, COLOR_RESET, COLOR_PINK, COLOR_BOLD, COLOR_RESET);
    printf("  %s%s╚══════════════════════════════════════════════╝%s\n\n", COLOR_PINK, COLOR_BOLD, COLOR_RESET);
}

void animate_start(const char *service_name) {
    const char *spinners[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int num_spinners = 10;

    /* Simulate visual delay & processing */
    for (int i = 0; i < 5; i++) {
        printf("\r  ");
        print_ts();
        printf(" ");
        print_badge(COLOR_LAVENDER, "START ");
        printf(" %sStarting %s%s%s… %s%s%s  ",
            COLOR_DIM, COLOR_LAVENDER, COLOR_BOLD, service_name, COLOR_RESET,
            COLOR_LAVENDER, spinners[i % num_spinners]);
        fflush(stdout);
        usleep(80000); /* 80ms per tick */
    }
    printf("\r\033[2K"); /* Clear the spinner line */
}

void mount_essential_fs() {
    section("mounting filesystems");

    mkdir("/proc", 0555);
    animate_start("proc");
    if (mount("proc", "/proc", "proc", 0, NULL) == 0 || errno == EBUSY) {
        line_ok("proc mounted (procfs)");
    } else {
        line_fail("failed to mount /proc");
    }

    mkdir("/sys", 0555);
    animate_start("sysfs");
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) == 0 || errno == EBUSY) {
        line_ok("sys mounted (sysfs)");
    } else {
        line_fail("failed to mount /sys");
    }

    mkdir("/dev", 0755);
    animate_start("devtmpfs");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) == 0 || errno == EBUSY) {
        line_ok("dev mounted (devtmpfs)");
    } else {
        line_fail("failed to mount /dev");
    }

    mkdir("/dev/pts", 0755);
    animate_start("devpts");
    if (mount("devpts", "/dev/pts", "devpts", 0, NULL) == 0 || errno == EBUSY) {
        line_ok("devpts mounted (devpts)");
    } else {
        line_fail("failed to mount /dev/pts");
    }
}

void set_system_hostname() {
    section("system config");
    FILE *f = fopen(HOSTNAME_FILE, "r");
    if (f) {
        char hostname[256];
        if (fgets(hostname, sizeof(hostname), f)) {
            hostname[strcspn(hostname, "\r\n")] = 0;
            if (strlen(hostname) > 0) {
                animate_start("hostname");
                if (sethostname(hostname, strlen(hostname)) == 0) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "hostname set to %s%s%s%s", COLOR_PINK, COLOR_BOLD, hostname, COLOR_RESET);
                    line_ok(msg);
                } else {
                    line_fail("failed to set hostname");
                }
            }
        }
        fclose(f);
    } else {
        line_skip("/etc/hostname not found");
    }
}

void load_modules() {
    section("kernel modules");
    FILE *f = fopen(MODULES_FILE, "r");
    if (!f) {
        line_skip("/etc/modules not found");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;

        animate_start(line);
        pid_t pid = fork();
        if (pid == 0) {
            execlp("modprobe", "modprobe", line, NULL);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            char msg[512];
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                snprintf(msg, sizeof(msg), "module %s%s%s loaded", COLOR_YELLOW, line, COLOR_RESET);
                line_ok(msg);
            } else {
                snprintf(msg, sizeof(msg), "failed to load %s%s%s", COLOR_CORAL, line, COLOR_RESET);
                line_fail(msg);
            }
        }
    }
    fclose(f);
}

void start_service(const char *service_name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", SERVICES_DIR, service_name);

    FILE *f = fopen(path, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "service definition %s%s%s not found", COLOR_CORAL, service_name, COLOR_RESET);
        line_warn(msg);
        return;
    }

    char line[1024];
    char *exec_cmd = NULL;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ExecStart=", 10) == 0) {
            exec_cmd = strdup(line + 10);
            exec_cmd[strcspn(exec_cmd, "\r\n")] = 0;
            break;
        }
    }
    fclose(f);

    if (!exec_cmd) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s%s%s missing ExecStart=", COLOR_CORAL, service_name, COLOR_RESET);
        line_fail(msg);
        return;
    }

    animate_start(service_name);

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", exec_cmd, NULL);
        exit(1);
    } else if (pid < 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "failed to fork %s%s%s", COLOR_CORAL, service_name, COLOR_RESET);
        line_fail(msg);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s%s%s service started", COLOR_MINT, service_name, COLOR_RESET);
        line_ok(msg);
    }

    free(exec_cmd);
}

void load_startup_services() {
    section("core services");

    DIR *d = opendir(STARTUP_DIR);
    if (!d) {
        line_skip("Startup directory not found or empty");
        return;
    }

    struct dirent *dir;
    int started = 0;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.') continue;
        start_service(dir->d_name);
        started++;
    }
    closedir(d);

    if (started == 0) {
        line_skip("no startup services defined");
    }
}

pid_t shell_pid = -1;

void spawn_shell() {
    shell_pid = fork();
    if (shell_pid == 0) {
        /* Set up the terminal for the interactive shell */
        setsid();
        
        int fd = open("/dev/console", O_RDWR);
        if (fd < 0) fd = open("/dev/tty1", O_RDWR);
        
        if (fd >= 0) {
            ioctl(fd, TIOCSCTTY, 1);
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        
        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
        setenv("TERM", "xterm-256color", 1);
        setenv("USER", "root", 1);
        setenv("HOME", "/root", 1);
        
        /* Launch bash as a login shell so it processes /etc/profile (and prints MOTD) */
        execl("/bin/bash", "-bash", NULL);
        /* Fallback to generic sh if bash is unavailable */
        execl("/bin/sh", "-sh", NULL);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (getpid() == 1) {
        print_banner();

        mount_essential_fs();
        set_system_hostname();
        load_modules();
        load_startup_services();

        printf("\n\n");
        printf("  %s%s❯%s %s%smochi-os%s reached %s%smulti-user.target%s\n\n",
            COLOR_PINK, COLOR_BOLD, COLOR_RESET,
            COLOR_WHITE, COLOR_BOLD, COLOR_RESET,
            COLOR_LAVENDER, COLOR_BOLD, COLOR_RESET);

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        long total_s = ts.tv_sec;
        printf("  %sUptime:   %lds — entering supervisor loop%s\n", COLOR_DIM, total_s, COLOR_RESET);
        printf("  %sHint:     Ready for user interaction.%s\n\n", COLOR_DIM, COLOR_RESET);

        /* Launch the system shell */
        spawn_shell();

        /* PID 1 (init) must act as the ultimate zombie reaper forever */
        while (1) {
            int status;
            pid_t pid = wait(&status);
            if (pid > 0) {
                if (pid == shell_pid) {
                    /* If the shell exits, respawn it */
                    spawn_shell();
                }
                /* System supervisor reaps cleanly otherwise */
            }
        }
    } else {
        fprintf(stderr, "SysDaemon Error: This program must be run as PID 1 to initialize the system.\n");
        return 1;
    }

    return 0;
}