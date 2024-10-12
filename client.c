#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void authentication(int client)
{
    char auth;
    char username[20], password[20];
    char buffer[256];

    while(1)
    {
        printf("Signup(S)/Login(L): ");
        fflush(stdout);
        scanf(" %c", &auth);
        if(auth == 'S' || auth == 's')
        {
            if(send(client, &auth, sizeof(auth), 0) < 0)
            {
                perror("Error sending authentication token");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter username: ");
            fflush(stdout);
            memset(username, '\0', sizeof(username));
            //Error here fgets dont run now trying scanf
            //fgets(&username, sizeof(username), stdin);
            scanf("%s", username);
            if(send(client, username, sizeof(username), 0) < 0)
            {
                perror("Error sending username");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving username received message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter password: ");
            fflush(stdout);
            memset(password, '\0', sizeof(password));
            // Same issue to password as username
            // fgets(password, sizeof(password), stdin);
            scanf("%s", password);
            if(send(client, password, sizeof(password), 0) < 0)
            {
                perror("Error sending password");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving password received message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));

            if (send(client, "Dose user exists?", 17, 0) < 0)
            {
                perror("Error sending check user existence message");
                exit(EXIT_FAILURE);
            }

            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving user existence response");
                exit(EXIT_FAILURE);
            }

            if (buffer[0] == 'E')
            {
                printf("User already exists. Please choose another username.\n");
                continue;
            }

            if (send(client, "Confirmation!!", 14, 0) < 0)
            {
                perror("Error sending confirmation message");
                exit(EXIT_FAILURE);
            }

            memset(buffer, '\0', sizeof(buffer));
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving confirmation response");
                exit(EXIT_FAILURE);
            }

            printf("User created successfully.\n Welcome %s\n", username);

            fflush(stdout);
            break;
        }
        if(auth == 'L' || auth == 'l')
        {
            if(send(client, &auth, sizeof(auth), 0) < 0)
            {
                perror("Error sending authentication token");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter username: ");
            fflush(stdout);
            memset(username, '\0', sizeof(username));
            //Error here fgets dont run now trying scanf
            //fgets(&username, sizeof(username), stdin);
            scanf("%s", username);
            if(send(client, username, sizeof(username), 0) < 0)
            {
                perror("Error sending username");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving username received message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter password: ");
            fflush(stdout);
            memset(password, '\0', sizeof(password));
            // Same issue to password as username
            // fgets(password, sizeof(password), stdin);
            scanf("%s", password);
            if(send(client, password, sizeof(password), 0) < 0)
            {
                perror("Error sending password");
                exit(EXIT_FAILURE);
            }
            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving password received message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));

            if (send(client, "Dose user exists?", 17, 0) < 0)
            {
                perror("Error sending check user existence message");
                exit(EXIT_FAILURE);
            }

            if(recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving user existence response");
                exit(EXIT_FAILURE);
            }
            if('F' == buffer[0])
            {
                printf("User does not exist. Please sign up.\n");
                continue;
            }
            printf("Login Successfull\nWelcome %s", username);
            break;
        }
    }
}

int main(int argc, char** argv)
{
    int client;
    struct sockaddr_in client_addr;

    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(client, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Error connecting");
        exit(EXIT_FAILURE);
    }

    char buffer[64];
    if(recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving welcome message");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", buffer);

    authentication(client);

    close(client);
    
    return 0;
}