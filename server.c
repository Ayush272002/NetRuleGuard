#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h> // For IP address conversion (inet_pton) ??? TODO : can get rid of this ???

#define MAX_RULES 100
#define MAX_QUERIES 100
#define BUFFER_SIZE 1024

typedef struct
{
    char rule[256];
    char queries[MAX_QUERIES][256];
    int num_queries;
} FirewallRule;

FirewallRule rules[MAX_RULES];
int num_rules = 0;
char all_requests[MAX_QUERIES][BUFFER_SIZE];
int num_requests = 0;

// Function Prototypes
void process_command(char *request);
void list_requests(void);
void run_interactive_mode(void);
void run_network_mode(int port);
int add_rule(const char *rule);
int check_connection(char *ip, int port);
int delete_rule(char *arg);
void list_rules(void);
int is_valid_rule(const char *rule);
int is_valid_ip(const char *ip);
int is_valid_port(int port);
int is_ip_in_range(const char *ip_start, const char *ip_end, const char *ip);
int is_port_in_range(int port_start, int port_end, int port);
int ip_to_int(const char *ip_str, uint32_t *ip_out);

int main(int argc, char **argv)
{
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
    return 0;
}

void run_interactive_mode(void) {
    char command[BUFFER_SIZE];

    while (1) {
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        if (num_requests < MAX_QUERIES) {
            strcpy(all_requests[num_requests], command);
            num_requests++;
        }

        process_command(command);
    }
}

void run_network_mode(int port) {
    printf("Starting network mode on port %d\n", port);
    // Network code would go here
}

void process_command(char *request) {
    if(request == NULL) {
        return;
    }

    const char command = request[0];
    char *arg = request + 2; // Skip the command and the space

    switch (command) {
        case 'R':
            list_requests();
            break;

        case 'A':
            if(is_valid_rule(arg) && add_rule(arg) == 0) {
                printf("Rule added\n");
            } else {
                printf("Invalid rule\n");
            }
            break;

        case 'C': {
            char ip[256];
            int port;
            if(sscanf(arg, "%s %d", ip, &port) != 2) {
                printf("Invalid command\n");
                break;
            }

            int result = check_connection(ip, port);
            if(result == -1) {
                printf("Illegal IP address or port specified\n");
            } else if(result == 1) {
                printf("Connection accepted\n");
            } else {
                printf("Connection rejected\n");
            }
            break;
        }

        case 'D':
            if(is_valid_rule(arg) && delete_rule(arg)) {
                printf("Rule deleted\n");
            } else {
                printf("Rule not found\n");
            }
            break;

        case 'L':
            list_rules();
            break;

        default:
            printf("Illegal request\n");
    }
}

void list_requests(void) {
    for(int i = 0; i < num_requests; i++) {
        printf("%s\n", all_requests[i]);
    }
}

int add_rule(const char *rule) {
    if(num_rules >= MAX_RULES) {
        return -1;
    }

    strcpy(rules[num_rules].rule, rule);
    rules[num_rules].num_queries = 0;
    num_rules++;
    return 0;
}

int ip_to_int(const char *ip_str, uint32_t *ip_out) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) == 1) {
        *ip_out = ntohl(addr.s_addr);
        return 1;
    }
    return 0;
}

int is_ip_in_range(const char *ip_start, const char *ip_end, const char *ip) {
    uint32_t start, end, target;

    if (!ip_to_int(ip_start, &start) || !ip_to_int(ip_end, &end) || !ip_to_int(ip, &target)) {
        return 0;
    }

    return (target >= start && target <= end);
}


int is_port_in_range(int port_start, int port_end, int port) {
    return port >= port_start && port <= port_end;
}

int check_connection(char *ip, int port) {
    if (!is_valid_ip(ip) || !is_valid_port(port)) {
        return -1;
    }

    for (int i = 0; i < num_rules; i++) {
        char ip_range[256], port_range[256];
        sscanf(rules[i].rule, "%255s %255s", ip_range, port_range);

        int ip_match = 0;
        char ip_start[256], ip_end[256];
        if (strchr(ip_range, '-')) {
            sscanf(ip_range, "%255[^-]-%255s", ip_start, ip_end);
            ip_match = is_ip_in_range(ip_start, ip_end, ip);
        } else {
            ip_match = (strcmp(ip_range, ip) == 0);
        }

        if (!ip_match) {
            continue;
        }

        int port_match = 0;
        int port_start, port_end;
        if (strchr(port_range, '-')) {
            sscanf(port_range, "%d-%d", &port_start, &port_end);
            port_match = is_port_in_range(port_start, port_end, port);
        } else {
            port_match = (atoi(port_range) == port);
        }


        if (port_match) {
            snprintf(rules[i].queries[rules[i].num_queries++], sizeof(rules[i].queries[0]), "%s %d", ip, port);
            return 1;
        }
    }

    return 0;
}

int delete_rule(char *rule) {
    for(int i = 0; i < num_rules; i++) {
        if(strcmp(rules[i].rule, rule) == 0) {
            for(int j = i; j < num_rules - 1; j++) {
                rules[j] = rules[j + 1];
            }
            num_rules--;
            return 1;
        }
    }
    return 0;
}

void list_rules(void) {
    for(int i = 0; i < num_rules; i++) {
        printf("Rule: %s\n", rules[i].rule);
        for(int j = 0; j < rules[i].num_queries; j++) {
            printf("Query: %s\n", rules[i].queries[j]);
        }
    }
}

int is_valid_ip(const char *ip) {
    int segments = 0;
    int curr_num = 0;
    const int len = strlen(ip);

    for(int i = 0; i < len; i++) {
        if(ip[i] == '.') {
            if(curr_num > 255) return 0;
            segments++;
            curr_num = 0;
        } else if(isdigit(ip[i])) {
            curr_num = curr_num * 10 + (ip[i] - '0');
        } else {
            return 0;
        }
    }
    return (segments == 3 && curr_num <= 255);
}

int is_valid_port(int port) {
    return port >= 0 && port <= 65535;
}

int is_valid_rule(const char *rule) {
    char ip_range[256], port_range[256];

    if (sscanf(rule, "%255s %255s", ip_range, port_range) != 2) {
        return 0; 
    }

    if (strchr(ip_range, '-')) {
        char ip_start[256], ip_end[256];
        sscanf(ip_range, "%255[^-]-%255s", ip_start, ip_end);
        if (!is_valid_ip(ip_start) || !is_valid_ip(ip_end)) {
            return 0; 
        }
    } else if (!is_valid_ip(ip_range)) {
        return 0; 
    }

    if (strchr(port_range, '-')) {
        int port_start, port_end;
        sscanf(port_range, "%d-%d", &port_start, &port_end);
        if (!is_valid_port(port_start) || !is_valid_port(port_end)) {
            return 0; 
        }
    } else if (!is_valid_port(atoi(port_range))) {
        return 0; 
    }

    return 1; 
}

