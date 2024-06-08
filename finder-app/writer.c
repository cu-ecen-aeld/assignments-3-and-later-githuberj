#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    // Check if the correct number of arguments is provided
    if (argc != 3) {
        syslog(LOG_ERR, "Usage: %s <file_path> <text_to_write>", argv[0]);
        closelog();
        return 1; // Return 1 if parameters are incorrect
    }

    // Check if the write string is not empty
    if (strlen(argv[2]) == 0) {
        syslog(LOG_ERR, "Write string is not specified");
        closelog();
        return 1; // Return 1 if write string is not specified
    }

    int fd;
    fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd == -1) {
        syslog(LOG_ERR, "Cannot create file %s: %s", argv[1], strerror(errno));
        closelog();
        return 1; // Return 1 if file cannot be created
    }

    ssize_t ret;
    size_t len = strlen(argv[2]);
    const char *text = argv[2];

    while (len != 0 && (ret = write(fd, text, len)) != 0) {
        if (ret == -1) {
            if (errno == EINTR)
                continue;

            syslog(LOG_ERR, "Cannot write to file %s: %s", argv[1], strerror(errno));
            close(fd);
            closelog();
            return 1; // Return 1 if write fails
        }
        len -= ret;
        text += ret;
    }

    if (close(fd) == -1) {
        syslog(LOG_ERR, "Cannot close file %s: %s", argv[1], strerror(errno));
        closelog();
        return 1; // Return 1 if file cannot be closed
    }

    syslog(LOG_DEBUG, "Writing to %s done", argv[1]);
    closelog();
    return 0; // Return 0 for success
}
