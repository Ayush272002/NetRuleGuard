#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h> // For IP address conversion (inet_pton)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define BUFFER_SIZE 1024

typedef struct QueryNode
{
    char query[256];
    struct QueryNode *next;
} QueryNode;

typedef struct RuleNode
{
    char rule[256];
    QueryNode *queries;
    struct RuleNode *next;
} RuleNode;

typedef struct RequestNode
{
    char request[BUFFER_SIZE];
    struct RequestNode *next;
} RequestNode;

RuleNode *rule_head = NULL;
RequestNode *request_head = NULL;

// Mutexes for thread safety
pthread_mutex_t rule_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function Prototypes
void process_command(char *request, int client_fd);
void list_requests(int client_fd);
void run_interactive_mode(void);
void run_network_mode(int port);
int add_rule(const char *rule);
int check_connection(char *ip, int port, int *allowed_rule);
int delete_rule(char *arg);
void list_rules(int client_fd);
int is_valid_rule(const char *rule);
int is_valid_ip(const char *ip);
int is_valid_port(int port);
int is_ip_in_range(const char *ip_start, const char *ip_end, const char *ip);
int is_port_in_range(int port_start, int port_end, int port);
int ip_to_int(const char *ip_str, uint32_t *ip_out);
void *client_handler(void *arg);
void free_requests(void);
void free_rules(void);

void strip_leading_zeros(char *rule)
{
    char *ip = strtok(rule, " ");
    char *port = strtok(NULL, " ");
    char result[512] = ""; // Increased buffer size

    // Process IP address or IP range
    char *token;
    char ip_result[256] = "";
    char *delim = ".";
    char *range_delim = "-";
    char *ip_start = strtok(ip, range_delim);
    char *ip_end = strtok(NULL, range_delim);

    // Process start of IP range or single IP
    token = strtok(ip_start, delim);
    while (token != NULL)
    {
        // Remove leading zeros
        while (*token == '0' && *(token + 1) != '\0')
        {
            token++;
        }
        strcat(ip_result, token);
        token = strtok(NULL, delim);
        if (token != NULL)
        {
            strcat(ip_result, ".");
        }
    }

    if (ip_end != NULL)
    {
        strcat(ip_result, "-");
        // Process end of IP range
        token = strtok(ip_end, delim);
        while (token != NULL)
        {
            // Remove leading zeros
            while (*token == '0' && *(token + 1) != '\0')
            {
                token++;
            }
            strcat(ip_result, token);
            token = strtok(NULL, delim);
            if (token != NULL)
            {
                strcat(ip_result, ".");
            }
        }
    }

    // Process port or port range
    char port_result[256] = "";
    char *port_start = strtok(port, range_delim);
    char *port_end = strtok(NULL, range_delim);

    // Remove leading zeros from start of port range or single port
    while (*port_start == '0' && *(port_start + 1) != '\0')
    {
        port_start++;
    }
    strcat(port_result, port_start);

    if (port_end != NULL)
    {
        strcat(port_result, "-");
        // Remove leading zeros from end of port range
        while (*port_end == '0' && *(port_end + 1) != '\0')
        {
            port_end++;
        }
        strcat(port_result, port_end);
    }

    // Combine IP and port results
    snprintf(result, sizeof(result), "%s %s", ip_result, port_result);
    strcpy(rule, result);
}

void cleanup(int sig)
{
    free_requests();
    free_rules();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, cleanup);
    if (argc != 2 || (strcmp(argv[1], "-i") != 0 && atoi(argv[1]) == 0))
    {
        printf("Usage: %s -i OR %s <port>\n", argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-i") == 0)
    {
        run_interactive_mode();
    }
    else
    {
        int port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            printf("Invalid port number\n");
            return 1;
        }
        run_network_mode(port);
    }
    free_requests();
    free_rules();
    return 0;
}

void run_interactive_mode(void)
{
    char command[BUFFER_SIZE];
    while (1)
    {
        if (fgets(command, BUFFER_SIZE, stdin) == NULL)
        {
            break;
        }
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n')
        {
            command[len - 1] = '\0';
        }

        pthread_mutex_lock(&request_mutex);
        RequestNode *new_request = (RequestNode *)malloc(sizeof(RequestNode));
        if (new_request == NULL)
        {
            perror("Failed to allocate memory for new request");
            pthread_mutex_unlock(&request_mutex);
            continue;
        }
        strcpy(new_request->request, command);
        new_request->next = NULL;

        if (request_head == NULL)
        {
            request_head = new_request;
        }
        else
        {
            RequestNode *current = request_head;
            while (current->next != NULL)
            {
                current = current->next;
            }
            current->next = new_request;
        }
        pthread_mutex_unlock(&request_mutex);

        process_command(command, -1);
    }
}

void run_network_mode(int port)
{
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("ERROR on setsockopt");
        close(sockfd);
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        close(sockfd);
        exit(1);
    }

    listen(sockfd, 5);
    // printf("Server listening on port %d\n", port);
    clilen = sizeof(cli_addr);

    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("ERROR on accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        if (pclient == NULL)
        {
            perror("Failed to allocate memory for client socket");
            close(newsockfd);
            continue;
        }
        *pclient = newsockfd;

        if (pthread_create(&tid, NULL, client_handler, pclient) != 0)
        {
            perror("Failed to create thread");
            free(pclient);
            close(newsockfd);
            continue;
        }

        pthread_detach(tid);
    }

    close(sockfd);
}

void *client_handler(void *arg)
{
    int client_fd = *((int *)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t n;

    while (1)
    {
        bzero(buffer, BUFFER_SIZE);
        n = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (n <= 0)
        {
            if (n < 0)
                perror("ERROR reading from socket");
            break;
        }

        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0';
        }

        pthread_mutex_lock(&request_mutex);
        RequestNode *new_request = (RequestNode *)malloc(sizeof(RequestNode));
        if (new_request == NULL)
        {
            perror("Failed to allocate memory for new request");
            pthread_mutex_unlock(&request_mutex);
            continue;
        }
        strcpy(new_request->request, buffer);
        new_request->next = NULL;

        if (request_head == NULL)
        {
            request_head = new_request;
        }
        else
        {
            RequestNode *current = request_head;
            while (current->next != NULL)
            {
                current = current->next;
            }
            current->next = new_request;
        }
        pthread_mutex_unlock(&request_mutex);

        process_command(buffer, client_fd);
    }

    close(client_fd);
    return NULL;
}

void process_command(char *request, int client_fd)
{
    if (request == NULL)
        return;
    const char command = request[0];
    char *arg = request + 2; // Skip the command and the space

    switch (command)
    {
    case 'R':
        list_requests(client_fd);
        break;
    case 'A':
        if (strlen(arg) == 0)
        {
            if (client_fd == -1)
                printf("Illegal request\n");
            else
                write(client_fd, "Illegal request\n", 16);
            break;
        }
        pthread_mutex_lock(&rule_mutex);
        if (is_valid_rule(arg) && add_rule(arg) == 0)
        {
            pthread_mutex_unlock(&rule_mutex);
            if (client_fd == -1)
                printf("Rule added\n");
            else
                write(client_fd, "Rule added\n", 11);
        }
        else
        {
            pthread_mutex_unlock(&rule_mutex);
            if (client_fd == -1)
                printf("Invalid rule\n");
            else
                write(client_fd, "Invalid rule\n", 13);
        }
        break;
    case 'C':
    {
        char ip[256];
        int port;
        if (sscanf(arg, "%s %d", ip, &port) != 2)
        {
            if (client_fd == -1)
                printf("Illegal request\n");
            else
                write(client_fd, "Illegal request\n", 16);
            break;
        }

        int allowed_rule = -1;
        int result = check_connection(ip, port, &allowed_rule);
        if (result == -1)
        {
            if (client_fd == -1)
                printf("Illegal IP address or port specified\n");
            else
                write(client_fd, "Illegal IP address or port specified\n", 37);
        }
        else if (result == 1)
        {
            if (client_fd == -1)
                printf("Connection accepted\n");
            else
                write(client_fd, "Connection accepted\n", 20);
        }
        else
        {
            if (client_fd == -1)
                printf("Connection rejected\n");
            else
                write(client_fd, "Connection rejected\n", 20);
        }
        break;
    }
    case 'D':
        if (strlen(arg) == 0)
        {
            if (client_fd == -1)
                printf("Illegal request\n");
            else
                write(client_fd, "Illegal request\n", 16);
            break;
        }
        pthread_mutex_lock(&rule_mutex);
        if (is_valid_rule(arg) && delete_rule(arg))
        {
            pthread_mutex_unlock(&rule_mutex);
            if (client_fd == -1)
                printf("Rule deleted\n");
            else
                write(client_fd, "Rule deleted\n", 13);
        }
        else
        {
            pthread_mutex_unlock(&rule_mutex);
            if (is_valid_rule(arg))
            {
                if (client_fd == -1)
                    printf("Rule not found\n");
                else
                    write(client_fd, "Rule not found\n", 15);
            }
            else
            {
                if (client_fd == -1)
                    printf("Rule invalid\n");
                else
                    write(client_fd, "Rule invalid\n", 13);
            }
        }
        break;
    case 'L':
        list_rules(client_fd);
        break;
    default:
        if (client_fd == -1)
            printf("Illegal request\n");
        else
            write(client_fd, "Illegal request\n", 16);
    }
}

void list_requests(int client_fd)
{
    pthread_mutex_lock(&request_mutex);
    RequestNode *current = request_head;
    char response[BUFFER_SIZE];
    bzero(response, BUFFER_SIZE);

    while (current != NULL)
    {
        strncat(response, current->request, BUFFER_SIZE - strlen(response) - 2);
        strncat(response, "\n", BUFFER_SIZE - strlen(response) - 1);
        current = current->next;
    }
    pthread_mutex_unlock(&request_mutex);

    if (client_fd == -1)
    {
        printf("%s", response);
    }
    else
    {
        write(client_fd, response, strlen(response));
    }
}

int add_rule(const char *rule)
{
    RuleNode *new_rule = (RuleNode *)malloc(sizeof(RuleNode));
    if (new_rule == NULL)
        return -1;

    char rule_copy[256];
    strcpy(rule_copy, rule);

    strip_leading_zeros(rule_copy);

    strcpy(new_rule->rule, rule_copy);
    new_rule->queries = NULL;

    // Insert at the beginning of the list
    new_rule->next = rule_head;
    rule_head = new_rule;
    return 0;
}

int delete_rule(char *rule)
{
    RuleNode *current = rule_head;
    RuleNode *prev = NULL;
    while (current != NULL)
    {
        if (strcmp(current->rule, rule) == 0)
        {
            if (prev == NULL)
            {
                rule_head = current->next;
            }
            else
            {
                prev->next = current->next;
            }

            QueryNode *q = current->queries;
            while (q != NULL)
            {
                QueryNode *temp = q;
                q = q->next;
                free(temp);
            }

            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

void list_rules(int client_fd)
{
    pthread_mutex_lock(&rule_mutex);
    RuleNode *current = rule_head;
    char response[BUFFER_SIZE * 10];
    bzero(response, sizeof(response));

    while (current != NULL)
    {
        strcat(response, "Rule: ");
        strcat(response, current->rule);
        strcat(response, "\n");

        QueryNode *q = current->queries;
        while (q != NULL)
        {
            strcat(response, "Query: ");
            strcat(response, q->query);
            strcat(response, "\n");
            q = q->next;
        }

        current = current->next;
    }
    pthread_mutex_unlock(&rule_mutex);

    if (client_fd == -1)
    {
        printf("%s", response);
    }
    else
    {
        write(client_fd, response, strlen(response));
    }
}

void strip_leading_zeros2(char *ip, char *port)
{
    // Process IP address
    char *token;
    char ip_result[16] = "";
    char *delim = ".";
    token = strtok(ip, delim);
    while (token != NULL)
    {
        // Remove leading zeros
        while (*token == '0' && *(token + 1) != '\0')
        {
            token++;
        }
        strcat(ip_result, token);
        token = strtok(NULL, delim);
        if (token != NULL)
        {
            strcat(ip_result, ".");
        }
    }
    strcpy(ip, ip_result);

    // Process port
    while (*port == '0' && *(port + 1) != '\0')
    {
        port++;
    }
}

int check_connection(char *ip, int port, int *allowed_rule)
{
    if (!is_valid_ip(ip) || !is_valid_port(port))
    {
        return -1;
    }

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    strip_leading_zeros2(ip, port_str);

    pthread_mutex_lock(&rule_mutex);
    RuleNode *current = rule_head;
    while (current != NULL)
    {
        char ip_range[256], port_range[256];
        sscanf(current->rule, "%255s %255s", ip_range, port_range);

        int ip_match = 0;
        char ip_start[256], ip_end[256];
        if (strchr(ip_range, '-'))
        {
            sscanf(ip_range, "%255[^-]-%255s", ip_start, ip_end);
            ip_match = is_ip_in_range(ip_start, ip_end, ip);
        }
        else
        {
            ip_match = (strcmp(ip_range, ip) == 0);
        }

        if (!ip_match)
        {
            current = current->next;
            continue;
        }

        int port_match = 0;
        int port_start, port_end;
        if (strchr(port_range, '-'))
        {
            sscanf(port_range, "%d-%d", &port_start, &port_end);
            port_match = is_port_in_range(port_start, port_end, port);
        }
        else
        {
            port_match = (atoi(port_range) == port);
        }

        if (port_match)
        {

            QueryNode *new_query = (QueryNode *)malloc(sizeof(QueryNode));
            if (new_query == NULL)
            {
                perror("Failed to allocate memory for new query");
                pthread_mutex_unlock(&rule_mutex);
                return 0;
            }
            snprintf(new_query->query, sizeof(new_query->query), "%s %d", ip, port);
            new_query->next = current->queries;
            current->queries = new_query;

            if (allowed_rule != NULL)
                *allowed_rule = 1;

            pthread_mutex_unlock(&rule_mutex);
            return 1;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&rule_mutex);
    return 0;
}

int ip_to_int(const char *ip_str, uint32_t *ip_out)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) == 1)
    {
        *ip_out = ntohl(addr.s_addr);
        return 1;
    }
    return 0;
}

int is_ip_in_range(const char *ip_start, const char *ip_end, const char *ip)
{
    uint32_t start, end, target;
    if (!ip_to_int(ip_start, &start) || !ip_to_int(ip_end, &end) || !ip_to_int(ip, &target))
    {
        return 0;
    }
    return (target >= start && target <= end);
}

int is_port_in_range(int port_start, int port_end, int port)
{
    return port >= port_start && port <= port_end;
}

int is_valid_ip(const char *ip)
{
    int segments = 0;
    int curr_num = 0;
    const int len = strlen(ip);

    for (int i = 0; i < len; i++)
    {
        if (ip[i] == '.')
        {
            if (curr_num > 255)
                return 0;
            segments++;
            curr_num = 0;
        }
        else if (isdigit(ip[i]))
        {
            curr_num = curr_num * 10 + (ip[i] - '0');
        }
        else
        {
            return 0;
        }
    }
    return (segments == 3 && curr_num <= 255);
}

int is_valid_port(int port)
{
    return port >= 0 && port <= 65535;
}

int is_valid_rule(const char *rule)
{
    char ip_range[256], port_range[256];
    if (sscanf(rule, "%255s %255s", ip_range, port_range) != 2)
    {
        return 0;
    }

    if (strchr(ip_range, '-'))
    {
        char ip_start[256], ip_end[256];
        sscanf(ip_range, "%255[^-]-%255s", ip_start, ip_end);

        uint32_t start, end;
        if (!is_valid_ip(ip_start) || !is_valid_ip(ip_end) ||
            !ip_to_int(ip_start, &start) || !ip_to_int(ip_end, &end) ||
            start > end)
        {
            return 0;
        }
    }
    else if (!is_valid_ip(ip_range))
    {
        return 0;
    }

    if (strchr(port_range, '-'))
    {
        int port_start, port_end;
        if (sscanf(port_range, "%d-%d", &port_start, &port_end) != 2 ||
            !is_valid_port(port_start) || !is_valid_port(port_end) ||
            port_start > port_end)
        {
            return 0;
        }
    }
    else
    {
        int port = atoi(port_range);
        if (!is_valid_port(port))
            return 0;
    }
    return 1;
}

void free_requests(void)
{
    pthread_mutex_lock(&request_mutex);
    RequestNode *current = request_head;
    while (current != NULL)
    {
        RequestNode *next = current->next;
        free(current);
        current = next;
    }
    request_head = NULL;
    pthread_mutex_unlock(&request_mutex);
}

void free_rules(void)
{
    pthread_mutex_lock(&rule_mutex);
    RuleNode *current_rule = rule_head;
    while (current_rule != NULL)
    {
        QueryNode *current_query = current_rule->queries;
        while (current_query != NULL)
        {
            QueryNode *next_query = current_query->next;
            free(current_query);
            current_query = next_query;
        }
        RuleNode *next_rule = current_rule->next;
        free(current_rule);
        current_rule = next_rule;
    }
    rule_head = NULL;
    pthread_mutex_unlock(&rule_mutex);
}