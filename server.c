#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h> // For IP address conversion (inet_pton)

#define BUFFER_SIZE 1024

typedef struct QueryNode {
    char query[256];
    struct QueryNode *next;
} QueryNode;

typedef struct RuleNode {
    char rule[256];
    QueryNode *queries;
    struct RuleNode *next;
} RuleNode;

typedef struct RequestNode {
    char request[BUFFER_SIZE];
    struct RequestNode *next;
} RequestNode;

RuleNode *rule_head = NULL;
RequestNode *request_head = NULL;

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

void free_requests(void) {
    RequestNode *current = request_head;
    while (current != NULL) {
        RequestNode *next = current->next;
        free(current);  
        current = next;  
    }
    request_head = NULL;
}

void free_rules(void) {
    RuleNode *current_rule = rule_head;
    while (current_rule != NULL) {
        QueryNode *current_query = current_rule->queries;
        while (current_query != NULL) {
            QueryNode *next_query = current_query->next;
            free(current_query); 
            current_query = next_query;
        }
        RuleNode *next_rule = current_rule->next;
        free(current_rule); 
        current_rule = next_rule;
    }
    rule_head = NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "-i") != 0 && atoi(argv[1]) == 0)) {
        printf("Usage: %s -i OR %s <port>\n", argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-i") == 0) {
        run_interactive_mode();
    } else {
        int port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port number\n");
            return 1;
        }
        run_network_mode(port);
    }
    free_requests();
    free_rules();
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

        RequestNode *new_request = (RequestNode *)malloc(sizeof(RequestNode));
        strcpy(new_request->request, command);
        new_request->next = NULL;

        if (request_head == NULL) {
            request_head = new_request;  
        } else {
            RequestNode *current = request_head;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = new_request;  
        }

        process_command(command);
    }
}

void run_network_mode(int port) {
    printf("Starting network mode on port %d\n", port);
    // Network code would go here
}

void process_command(char *request) {
    if(request == NULL) return;
    const char command = request[0];
    char *arg = request + 2; // Skip the command and the space

    switch (command) {
        case 'R':
            list_requests();
            break;
        case 'A':
            if (strlen(arg) == 0) { 
                printf("Illegal request\n");
                break;
            }
            if (is_valid_rule(arg) && add_rule(arg) == 0) {
                printf("Rule added\n");
            } else {
                printf("Invalid rule\n");
            }
            break;
        case 'C': {
            char ip[256];
            int port;
            if (sscanf(arg, "%s %d", ip, &port) != 2) {
                printf("Illegal request\n");
                break;
            }

            int result = check_connection(ip, port);
            if (result == -1) {
                printf("Illegal IP address or port specified\n");
            } else if (result == 1) {
                printf("Connection accepted\n");
            } else {
                printf("Connection rejected\n");
            }
            break;
        }
        case 'D':
            if (strlen(arg) == 0) {
                printf("Illegal request\n");
                break;
            }
            if (is_valid_rule(arg) && delete_rule(arg)) {
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
    RequestNode *current = request_head;
    while (current != NULL) {
        printf("%s\n", current->request);
        current = current->next;
    }
}

int add_rule(const char *rule) {
    RuleNode *new_rule = (RuleNode *)malloc(sizeof(RuleNode));
    if (new_rule == NULL) return -1;

    strcpy(new_rule->rule, rule);
    new_rule->queries = NULL;
    new_rule->next = rule_head;
    rule_head = new_rule;
    return 0;
}

int delete_rule(char *rule) {
    RuleNode *current = rule_head;
    RuleNode *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->rule, rule) == 0) {
            if (prev == NULL) {
                rule_head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

void list_rules(void) {
    RuleNode *current = rule_head;
    while (current != NULL) {
        printf("Rule: %s\n", current->rule);
        QueryNode *query = current->queries;
        while (query != NULL) {
            printf("Query: %s\n", query->query);
            query = query->next;
        }
        current = current->next;
    }
}

int check_connection(char *ip, int port) {
    if (!is_valid_ip(ip) || !is_valid_port(port)) {
        return -1;
    }

    RuleNode *current = rule_head;
    while (current != NULL) {
        char ip_range[256], port_range[256];
        sscanf(current->rule, "%255s %255s", ip_range, port_range);

        int ip_match = 0;
        char ip_start[256], ip_end[256];
        if (strchr(ip_range, '-')) {
            sscanf(ip_range, "%255[^-]-%255s", ip_start, ip_end);
            ip_match = is_ip_in_range(ip_start, ip_end, ip);
        } else {
            ip_match = (strcmp(ip_range, ip) == 0);
        }

        if (!ip_match) {
            current = current->next;
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
            QueryNode *new_query = (QueryNode *)malloc(sizeof(QueryNode));
            snprintf(new_query->query, sizeof(new_query->query), "%s %d", ip, port);
            new_query->next = current->queries;
            current->queries = new_query;
            return 1;
        }
        current = current->next;
    }

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

int is_valid_ip(const char *ip) {
    int segments = 0;
    int curr_num = 0;
    const int len = strlen(ip);

    for (int i = 0; i < len; i++) {
        if (ip[i] == '.') {
            if (curr_num > 255) return 0;
            segments++;
            curr_num = 0;
        } else if (isdigit(ip[i])) {
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
        
        uint32_t start, end;
        if (!is_valid_ip(ip_start) || !is_valid_ip(ip_end) || 
            !ip_to_int(ip_start, &start) || !ip_to_int(ip_end, &end) || 
            start > end) {
            return 0; 
        }
    } else if (!is_valid_ip(ip_range)) {
        return 0; 
    }

    if (strchr(port_range, '-')) {
        int port_start, port_end;
        if (sscanf(port_range, "%d-%d", &port_start, &port_end) != 2 ||
            !is_valid_port(port_start) || !is_valid_port(port_end) || 
            port_start > port_end) {
            return 0; 
        }
    } else if (!is_valid_port(atoi(port_range))) {
        return 0; 
    }
    return 1; 
}
