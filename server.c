#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 8080
#define users "users.txt"

void authentication(int client)
{
    char auth;
    char username[20], password[20];
    char buffer[256];

    while(1)
    {
        if (recv(client, &auth, sizeof(auth), 0) < 0)
        {
            perror("Error receiving authentication token");
            exit(EXIT_FAILURE);
        }

        if(auth == 'S' || auth == 's')
        {
            if(send(client, "Sign up\0", 8, 0) < 0)
            {
                perror("Error sending sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Authentication Type: Sign up\n");
            fflush(stdout);

            if(recv(client, username, sizeof(username), 0) < 0)
            {
                perror("Error receiving username");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Username recived\0", 17, 0) < 0)
            {
                perror("Error sending username received message");
                exit(EXIT_FAILURE);
            }

            if(recv(client, password, sizeof(password), 0) < 0)
            {
                perror("Error receiving password");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Password recived\0", 17, 0) < 0)
            {
                perror("Error sending password received message");
                exit(EXIT_FAILURE);
            }

            printf("Username: %s\nPassword: %s\n", username, password);

            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving message");
                exit(EXIT_FAILURE);
            }

            printf("Client says: %s\n", buffer);

            FILE *file;
            file = fopen(users, "r");
            if (file == NULL)
            {
                perror("Error opening users.txt");
                exit(EXIT_FAILURE);
            }
            memset(buffer, '\0', sizeof(buffer));
            int check = 0;
            while(fgets(buffer, sizeof(buffer), file))
            {
                char existing[20];
                sscanf(buffer, "%s", existing);
                if(strcmp(existing, username) == 0)
                {
                    check = 1;
                    printf("Username already exists\n");
                    if(send(client, "E", 1, 0) < 0)
                    {
                        perror("Error sending existing message");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                memset(buffer, '\0', sizeof(buffer));
                memset(existing, '\0', sizeof(existing));
            }
            if(check == 1)
            {
                fclose(file);
                continue;
            }
            printf("Username dose not exists\n");
            if(send(client, "NE", 2, 0) < 0)
            {
                perror("Error sending not existing message");
                exit(EXIT_FAILURE);
            }
            memset(buffer, '\0', sizeof(buffer));
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving message");
                exit(EXIT_FAILURE);
            }
            printf("Client says: %s\n", buffer);
            fclose(file);

            file = fopen(users, "a");
            if (file == NULL)
            {
                perror("Error opening users.txt");
                exit(EXIT_FAILURE);
            }
            fprintf(file, "\n%s %s 10", username, password);
            fclose(file);
            if (send(client, "User created successfully", 25, 0) < 0)
            {
                perror("Error sending user created message");
                exit(EXIT_FAILURE);
            }
            printf("Signup successfully\n");
            break;
        }
        if (auth == 'L' || auth == 'l')
        {
            if(send(client, "Login\0", 7, 0) < 0)
            {
                perror("Error sending sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Authentication Type: Login\n");
            fflush(stdout);
            if(recv(client, username, sizeof(username), 0) < 0)
            {
                perror("Error receiving username");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Username recived\0", 17, 0) < 0)
            {
                perror("Error sending username received message");
                exit(EXIT_FAILURE);
            }

            if(recv(client, password, sizeof(password), 0) < 0)
            {
                perror("Error receiving password");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Password recived\0", 17, 0) < 0)
            {
                perror("Error sending password received message");
                exit(EXIT_FAILURE);
            }

            printf("Username: %s\nPassword: %s\n", username, password);

            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving message");
                exit(EXIT_FAILURE);
            }

            printf("Client says: %s\n", buffer);

            FILE *file;
            file = fopen(users, "r");
            if(file == NULL)
            {
                perror("Error opening users.txt");
                exit(EXIT_FAILURE);
            }
            int s;
            memset(buffer, '\0', sizeof(buffer));
            int check = 0;
            while(fgets(buffer, sizeof(buffer), file))
            {
                char name[20], pass[20];
                sscanf(buffer, "%s %s %d", name, pass, &s);
                if ((strcmp(username, name) == 0)  && (strcmp(password, pass) == 0))
                {
                    check = 1;
                    printf("Authentication successful\n");
                    if(send(client, "S", 1, 0) < 0)
                    {
                        perror("Error sending login success message to client");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                memset(buffer, '\0', sizeof(buffer));
            }
            if(!check)
            {
                printf("Authentication failed\n");
                if(send(client, "F", 1, 0) < 0)
                {
                    perror("Error sending authentication failed message");
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            printf("Welcome %s\n", username);
            break;
        }
    }
    
}


int main(int argc, char** argv)
{
    int server, client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clilen = sizeof(client_addr);
    int opt = 1;

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(server, 3) < 0)
    {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }

    client = accept(server, (struct sockaddr *)&client_addr, &clilen);
    if (client < 0)
    {
        perror("Error accepting");
        exit(EXIT_FAILURE);
    }

    if (send(client, "Server connected successfully", 29, 0) < 0)
    {
        perror("Error receiving");
        exit(EXIT_FAILURE);
    }

    printf("Server connected successfully\n");
    fflush(stdout);

    authentication(client);

    close(server);
    close(client);

    return 0;
}