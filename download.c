#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <bsd/string.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define URL_MAX_LEN 2048  // Maximum length of an URL
#define BUFFER_LEN 2048   // Maximum length of a buffer
#define FTP_PORT 21       // FTP default port number
#define BAR_WIDTH 24      // Width of the progress bar

// Structure to store the FTP URL components
typedef struct {
    char username[BUFFER_LEN + 1];
    char password[BUFFER_LEN + 1];
    char domain[BUFFER_LEN + 1];
    char path[BUFFER_LEN + 1];
} FtpUrl;

// Structure to store a line from a response received from the server
typedef struct {
    int code;
    char content[BUFFER_LEN + 1];
    bool final;
} Message;


// Print an error message
void print_error(const char *function_name, const char *message_format, ...) {
    va_list args;
    va_start(args, message_format);

    fprintf(stderr, "Error in %s: ", function_name);
    vfprintf(stderr, message_format, args);
    fprintf(stderr, "\n");

    va_end(args);
}

// Parse an URL, storing its components in a FtpUrl structure
// Return 0 on success and non-zero otherwise
int parse_url(char *url, FtpUrl *ftp_url) {
    if (strlen(url) > URL_MAX_LEN) {
        print_error(__func__, "URL max length exceeded");
        return 1;
    }

    char *scheme = strtok(url, ":"); // Remove scheme
    char *schemeless_url = strtok(NULL, "");

    if (scheme == NULL || strncmp(schemeless_url, "//", 2) != 0) {
        print_error(__func__, "Bad URL");
        return 1;
    }

    char *username_password, *domain;
    if (strchr(schemeless_url + 2, '@') != NULL) {
        username_password = strtok(schemeless_url + 2, "@");
        domain = strtok(NULL, "/");
    } else {
        username_password = NULL;
        domain = strtok(schemeless_url + 2, "/");
    }
    char *path = strtok(NULL, "");

    if (domain == NULL || path == NULL) {
        print_error(__func__, "Bad URL");
        return 1;
    }

    if (strcmp(scheme, "ftp") != 0) {
        print_error(__func__, "Invalid schema %s", scheme);
        return 1;
    }

    char *username, *password;
    if (username_password == NULL) {
        username = "anonymous";
        password = "anonymous";
    } else if (strchr(username_password, ':') == NULL) {
        username = username_password;
        password = "anonymous";
    } else {
        username = strtok(username_password, ":");
        password = strtok(NULL, "");
    }

    strlcpy(ftp_url->username, username, BUFFER_LEN);
    strlcpy(ftp_url->password, password, BUFFER_LEN);
    strlcpy(ftp_url->domain, domain, BUFFER_LEN);
    strlcpy(ftp_url->path, path, BUFFER_LEN);

    return 0;
}
// Get a socket file descriptor connected to a given address and port
// Return the socket file descriptor on success and -1 otherwise
int get_socket_fd_addr(const char *addr, uint16_t port) {
    struct sockaddr_in server_addr;
    int sockfd;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(addr);
    server_addr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_error(__func__, "socket() failed");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        print_error(__func__, "connect() failed");
        return -1;
    }

    printf("Connected to %s:%d\n", addr, port);
    return sockfd;
}

// Returns a socket file descriptor connected to a specific host and port
// Return the socket file descriptor on success and -1 otherwise
int get_socket_fd_host(const struct hostent *host, uint16_t port) {
    const char *addr = inet_ntoa(*((struct in_addr *) host->h_addr));
    return get_socket_fd_addr(addr, port);
}

// Read a line from a socket, storing the corresponding info in a Message structure
// Return 0 on success and non-zero otherwise
int read_message(int sockfd, Message *message) {
    char buffer[BUFFER_LEN + 1], separator;

    while (true) {
        int i = 0;
        char c;
        while (i < BUFFER_LEN && read(sockfd, &c, 1) > 0 && c != '\r') {
            buffer[i++] = c;
        }
        buffer[i] = '\0';
        read(sockfd, &c, 1);  // Consume '\n'

        printf("    \x1B[2;37m%s\x1B[0m\n", buffer);

        int res, n;
        if ((res = sscanf(buffer, "%d%c%n", &message->code, &separator, &n)) == 2) {  // %n does not count for the argument count
            strcpy(message->content, buffer + n);
        } else {
            continue;
        }
        break;
    }

    switch (separator) {
        case '-':
            message->final = false;
            break;
        case ' ':
            message->final = true;
            break;
        default:
            print_error(__func__, "Invalid response (Unknown separator)\n");
            return 1;
    }

    return 0;
}

// Read the final line of a socket response, ignoring all the previous ones
// Return 0 on success and non-zero otherwise
int read_end(int sockfd, Message *message) {
    do {
        if (read_message(sockfd, message))
            return 1;
    } while (!message->final);

    return 0;
}

// Check if a message has a specific code
// Return 0 if the code is the expected one and non-zero otherwise
int check_code(Message *message, ...) {
    va_list args;
    va_start(args, message);

    for (int expected_code = va_arg(args, int); expected_code != 0; expected_code = va_arg(args, int)) {
        if (message->code == expected_code) {
            return 0;
        }
    }

    va_end(args);

    fprintf(stderr, "Error in %s: Invalid response (code %d, expected ", __func__, message->code);

    va_start(args, message);
    for (int i = 0, expected_code = va_arg(args, int); expected_code != 0; i++, expected_code = va_arg(args, int)) {
        if (i > 0) {
            fprintf(stderr, "/");
        }
        fprintf(stderr, "%d", expected_code);
    }
    va_end(args);

    fprintf(stderr, ")\n");

    return 1;
}

// Send a command to a socket
// Return 0 on success and non-zero otherwise
int send_command(int sockfd, const char *command_format, ...) {
    char command[BUFFER_LEN + 1];

    va_list args;
    va_start(args, command_format);

    int len = vsnprintf(command, BUFFER_LEN - 2, command_format, args);
    sprintf(command + len, "\r\n");

    va_end(args);

    printf("  > \x1B[1;2;37m%.*s\x1B[0m\n", len, command);
    if (write(sockfd, command, len + 2) < 0) {
        print_error(__func__, "write() failed");
        return 1;
    }

    return 0;
}

// Print a progress bar
void print_progress(size_t current, size_t total) {
    int width = current * BAR_WIDTH / total;

    if (width < 24) {
        double percentage = current * 100.0 / total;
        printf("\x1B[2KDownloading...      [%.*s>%*s] %.1f%%\r", width, "=======================", 23 - width, "", percentage);
    } else {
        printf("\x1B[2KDownload complete!  [%s] 100.0%%\n", "=======================");
    }

    fflush(stdout);
}

// Parse the address and port from a "Passive Mode" response
// Return 0 on success and non-zero otherwise
int parse_pasv_response(const char *response, char *addr, uint16_t *port) {
    uint8_t addr_bytes[4], port_bytes[2];

    int res = sscanf(
        response,
        "Entering Passive Mode (%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)",
        addr_bytes, addr_bytes + 1, addr_bytes + 2, addr_bytes + 3, port_bytes, port_bytes + 1
    );
    if (res != 6) {
        print_error(__func__, "Invalid \"Passive Mode\" response");
        return 1;
    }

    sprintf(addr, "%hhu.%hhu.%hhu.%hhu", addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3]);
    *port = port_bytes[0] * 256 + port_bytes[1];

    return 0;
}

// Reduce a size to the most appropriate unit
// Gives the most appropriate unit and the corresponding size modifier
void reduce_unit(size_t size, char **unit, size_t *size_modifier) {
    if (size > 1024 * 1024) {
        *size_modifier = 1024 * 1024;
        *unit = "MiB";
    }
    else if (size > 1024) {
        *size_modifier = 1024;
        *unit = "KiB";
    } else {
        *size_modifier = 1;
        *unit = "B";
    }
}

// Print transfer statistics
void print_transfer_stats(size_t bytes, struct timespec start, struct timespec end) {
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double speed = bytes / total_time;
    char* speed_unit, *size_unit;
    size_t speed_modifier, size_modifier;

    reduce_unit(speed, &speed_unit, &speed_modifier);
    reduce_unit(bytes, &size_unit, &size_modifier);

    printf("\n============ STATISTICS ============\n");
    printf("Total transfer time: %.3f s\n", total_time);
    printf("Total transferred bytes: %.2f %s\n", (double)bytes / size_modifier, size_unit);
    printf("Transfer speed: %.2f %s/s\n", speed / speed_modifier, speed_unit);
    printf("====================================\n\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <FTP URL>\n", argv[0]);
        return 1;
    }

    char *url = argv[1];
    FtpUrl ftp_url;
    size_t filesize;

    if (parse_url(url, &ftp_url)) {
        return 1;
    }

    struct hostent *host = gethostbyname(ftp_url.domain);
    if (host == NULL) {
        print_error(__func__, "Could not resolve host");
        return 1;
    }

    Message message;
    int control_sockfd = get_socket_fd_host(host, FTP_PORT);
    if (control_sockfd < 0) {
        print_error(__func__, "get_socket_fd_addr() failed");
        return 1;
    }
    if (read_end(control_sockfd, &message)
        || check_code(&message, 220, NULL)) {
        return 1;
    }

    if (send_command(control_sockfd, "USER %s", ftp_url.username)
        || read_end(control_sockfd, &message)
        || check_code(&message, 331, NULL)) {
        return 1;
    }

    if (send_command(control_sockfd, "PASS %s", ftp_url.password)
        || read_end(control_sockfd, &message)
        || check_code(&message, 230, NULL)) {
        return 1;
    }

    if (send_command(control_sockfd, "TYPE I")
        || read_end(control_sockfd, &message)
        || check_code(&message, 200, NULL)) {
        return 1;
    }

    if (send_command(control_sockfd, "SIZE %s", ftp_url.path)
        || read_end(control_sockfd, &message)
        || check_code(&message, 213, NULL)) {
        return 1;
    }
    if (sscanf(message.content, "%lu", &filesize) != 1) {
        print_error(__func__, "Invalid \"SIZE\" response");
        return 1;
    }

    if (send_command(control_sockfd, "PASV")
        || read_end(control_sockfd, &message)
        || check_code(&message, 227, NULL)) {
        return 1;
    }

    char addr[15 + 1];
    uint16_t port;
    if (parse_pasv_response(message.content, addr, &port)) {
        return 1;
    }
    int data_sockfd = get_socket_fd_addr(addr, port);
    if (data_sockfd < 0) {
        print_error(__func__, "get_socket_fd_addr() failed");
        return 1;
    }

    if (send_command(control_sockfd, "RETR %s", ftp_url.path)
        || read_end(control_sockfd, &message)
        || check_code(&message, 150, 125, NULL)) {
        return 1;
    }

    int file_fd = open(basename(ftp_url.path), O_WRONLY | O_CREAT, 0640);
    uint8_t buffer[BUFFER_LEN];
    size_t response_len, total_bytes = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while ((response_len = read(data_sockfd, buffer, BUFFER_LEN)) > 0) {
        write(file_fd, buffer, response_len);

        total_bytes += response_len;
        print_progress(total_bytes, filesize);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    if (close(file_fd) || close(data_sockfd)) {
        print_error(__func__, "close() failed");
        return 1;
    }

    if (read_message(control_sockfd, &message) || check_code(&message, 226)) {
        return 1;
    }

    if (send_command(control_sockfd, "QUIT")
        || read_message(control_sockfd, &message)
        || check_code(&message, 221, NULL)) {
        return 1;
    }

    if (close(control_sockfd)) {
        print_error(__func__, "close() failed");
        return 1;
    }

    print_transfer_stats(total_bytes, start, end);

    return 0;
}