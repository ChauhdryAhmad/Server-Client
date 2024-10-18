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

    while (1)
    {
        printf("Signup(S)/Login(L): ");
        fflush(stdout);
        scanf(" %c", &auth);
        if (auth == 'S' || auth == 's')
        {
            if (send(client, &auth, sizeof(auth), 0) < 0)
            {
                perror("Error sending authentication token");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter username: ");
            fflush(stdout);
            memset(username, '\0', sizeof(username));
            // Error here fgets dont run now trying scanf
            // fgets(&username, sizeof(username), stdin);
            scanf("%s", username);
            if (send(client, username, sizeof(username), 0) < 0)
            {
                perror("Error sending username");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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
            if (send(client, password, sizeof(password), 0) < 0)
            {
                perror("Error sending password");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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

            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving confirmation response");
                exit(EXIT_FAILURE);
            }

            printf("User created successfully.\n Welcome %s\n", username);

            fflush(stdout);
            break;
        }
        if (auth == 'L' || auth == 'l')
        {
            if (send(client, &auth, sizeof(auth), 0) < 0)
            {
                perror("Error sending authentication token");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Server says: %s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
            printf("Enter username: ");
            fflush(stdout);
            memset(username, '\0', sizeof(username));
            // Error here fgets dont run now trying scanf
            // fgets(&username, sizeof(username), stdin);
            scanf("%s", username);
            if (send(client, username, sizeof(username), 0) < 0)
            {
                perror("Error sending username");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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
            if (send(client, password, sizeof(password), 0) < 0)
            {
                perror("Error sending password");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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

            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving user existence response");
                exit(EXIT_FAILURE);
            }
            if ('F' == buffer[0])
            {
                printf("User does not exist. Please sign up.\n");
                continue;
            }
            printf("Login Successfull\nWelcome %s", username);
            break;
        }
    }
}

void UPLOAD_command(int client, char *command)
{
    char file_path[64];
    char buffer[1024];
    char filename[20];

    memset(file_path, '\0', sizeof(file_path));
    memset(buffer, '\0', sizeof(buffer));
    memset(filename, '\0', sizeof(filename));

    strncpy(file_path, command + 8, strlen(command) - 8);
    file_path[strlen(buffer) - 8] = '\0';

    printf("Uploading file: %s\n", file_path);
    fflush(stdout);

    if (access(file_path, F_OK) == -1)
    {
        printf("Error: File not found.\n");
        return;
    }

    const char *filename_ptr = strrchr(file_path, '/');
    if (filename_ptr)
        strcpy(filename, filename_ptr + 1);
    else
        strcpy(filename, file_path);

    printf("File name: %s\n", filename);
    fflush(stdout);

    if (send(client, "UPLOAD", 6, 0) < 0)
    {
        perror("Error sending upload command");
        exit(EXIT_FAILURE);
    }
    

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving Confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));

    // strncpy(file_path, command + 8, strlen(command) - 8);
    // file_path[strlen(buffer) - 8] = '\0';

    // printf("Uploading file: %s\n", file_path);
    // fflush(stdout);

    if (send(client, filename, sizeof(filename), 0) < 0)
    {
        perror("Error sending filename");
        exit(EXIT_FAILURE);
    }

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving Validation message");
        exit(EXIT_FAILURE);
    }

    // if (strncmp(buffer, "IV", 2) == 0)
    // {
    //     printf("Invalid path provided\n");
    //     return;
    // }

    printf("Server says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));

    if (send(client, "Check File Existence!", 21, 0) < 0)
    {
        perror("Error sending check file existence message");
        exit(EXIT_FAILURE);
    }

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving file existence response");
        exit(EXIT_FAILURE);
    }

    if (strncmp(buffer, "E", 1) == 0)
    {
        printf("File already exists\n");
        char resp;
        printf("Do you want to overwrite it? (Y/N): ");
        fflush(stdout);
        scanf(" %c", &resp);
        if (resp == 'Y')
        {
            printf("Overwriting file...\n");
            if (send(client, "Yes", 3, 0) < 0)
            {
                perror("Error sending overwrite confirmation");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            printf("File not overwritten\n");
            return;
        }
    }
    else
    {
        if (send(client, "Proceed", 2, 0) < 0)
        {
            perror("Error sending proceed confirmation");
            exit(EXIT_FAILURE);
        }
    }

    memset(buffer, '\0', sizeof(buffer));

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving file size");
        exit(EXIT_FAILURE);
    }

    if (strncmp(buffer, "LS", 2) == 0)
    {
        printf("Not enough storage\n");
        return;
    }

    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int byte_size;
    memset(buffer, '\0', sizeof(buffer));

    while (byte_size = fread(buffer, 1, sizeof(buffer), file))
    {
        if (send(client, buffer, byte_size, 0) < 0)
        {
            perror("Error sending file data");
            exit(EXIT_FAILURE);
        }
        printf("Sent %d bytes\n", byte_size);
        fflush(stdout);
        memset(buffer, '\0', sizeof(buffer));

        if (recv(client, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Error receiving confirmation message");
            exit(EXIT_FAILURE);
        }
        memset(buffer, '\0', sizeof(buffer));
    }

    if (send(client, "EOF", 3, 0) < 0)
    {
        perror("Error sending EOF message");
        exit(EXIT_FAILURE);
    }

    memset(buffer, '\0', sizeof(buffer));

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error recieving EOF message");
        exit(EXIT_FAILURE);
    }

    printf("Server says: %s\n", buffer);
}

void DOWNLOAD_command(int client, char *command)
{
    char file_name[20];
    char buffer[1024];
    if (send(client, "DOWNLOAD", 8, 0) < 0)
    {
        perror("Error sending download command");
        exit(EXIT_FAILURE);
    }
    memset(file_name, '\0', sizeof(file_name));
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));
    strncpy(file_name, command + 10, strlen(command) - 10);
    file_name[strlen(buffer) - 10] = '\0';
    printf("Downloading file: %s\n", file_name);
    fflush(stdout);
    if (send(client, file_name, sizeof(file_name), 0) < 0)
    {
        perror("Error sending file name");
        exit(EXIT_FAILURE);
    }
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving validation message");
        exit(EXIT_FAILURE);
    }
    if (strncmp(buffer, "NE", 2) == 0)
    {
        printf("File not found\n");
        return;
    }
    char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        perror("ERROR getting home directory");

    char path[512];
    snprintf(path, sizeof(path), "%s/Downloads/%s", home_dir, file_name);
    printf("File will download at: %s\n", path);
    fflush(stdout);
    if (send(client, "Proceed", 7, 0) < 0)
    {
        perror("Error sending proceed confirmation");
        exit(EXIT_FAILURE);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    int bytes_read;
    memset(buffer, '\0', sizeof(buffer));
    while ((bytes_read = recv(client, buffer, sizeof(buffer), 0)) > 0)
    {
        if (strncmp(buffer, "EOF", 3) == 0)
        {
            break;
        }
        fwrite(buffer, 1, bytes_read, file);
        memset(buffer, '\0', sizeof(buffer));
        if (send(client, &bytes_read, sizeof(buffer), 0) < 0)
        {
            perror("Error sending bytes read message");
            exit(EXIT_FAILURE);
        }
        printf("Bytes read: %d\n", bytes_read);
    }

    if (send(client, "EOF", 3, 0) < 0)
    {
        perror("Error sending EOF confirmation message");
        exit(EXIT_FAILURE);
    }
    fclose(file);

    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
}

void LIST_command(int client, const char *command)
{
    char file_name[20];
    char buffer[256];
    if (send(client, "LIST", 4, 0) < 0)
    {
        perror("Error sending download command");
        exit(EXIT_FAILURE);
    }
    memset(file_name, '\0', sizeof(file_name));
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));
    if (send(client, "Send Files...", 13, 0) < 0)
    {
        perror("Error sending confirmation message");
        exit(EXIT_FAILURE);
    }
    memset(buffer, '\0', sizeof(buffer));
    printf("Your files :\n");
    fflush(stdout);
    int byte_size;
    while (byte_size = recv(client, buffer, sizeof(buffer), 0))
    {
        // printf("%d\n", byte_size);
        if (strncmp(buffer, "EOF", 3) == 0)
        {
            break;
        }
        // Its working by putting this because otherwise it was taking something in buufer
        if (buffer[0] != '$')
            continue;
        printf("%s\n", buffer);
        memset(buffer, '\0', sizeof(buffer));
        if (send(client, "NEXT", 4, 0) < 0)
        {
            perror("Error sending confirmation message");
            exit(EXIT_FAILURE);
        }
        memset(buffer, '\0', sizeof(buffer));
    }
    if (send(client, "EOF recived", 11, 0) < 0)
    {
        perror("Error sending confirmation message");
        exit(EXIT_FAILURE);
    }
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error recieving confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
}

void DELETE_command(int client, const char *command)
{
    char file_name[20];
    char buffer[1024];
    if (send(client, "DELETE", 6, 0) < 0)
    {
        perror("Error sending delete command");
        exit(EXIT_FAILURE);
    }
    memset(file_name, '\0', sizeof(file_name));
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));
    strncpy(file_name, command + 8, strlen(command) - 8);
    file_name[strlen(buffer) - 8] = '\0';
    printf("Deleting file: %s\n", file_name);
    fflush(stdout);
    if (send(client, file_name, sizeof(file_name), 0) < 0)
    {
        perror("Error sending file name");
        exit(EXIT_FAILURE);
    }
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving validation message");
        exit(EXIT_FAILURE);
    }
    if (strncmp(buffer, "NE", 2) == 0)
    {
        printf("File not found\n");
        return;
    }
    memset(buffer, '\0', sizeof(buffer));
    if (send(client, "Proceed", 7, 0) < 0)
    {
        perror("Error sending proceed confirmation");
        exit(EXIT_FAILURE);
    }
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving validation message");
        exit(EXIT_FAILURE);
    }
    printf("Server says: %s\n", buffer);
    fflush(stdout);
}

void handle_command(int client)
{
    char buffer[256];
    while (1)
    {
        memset(buffer, '\0', sizeof(buffer));
        printf("Enter command: ");
        scanf("%s", buffer);

        if (strncmp(buffer, "$UPLOAD$", 8) == 0)
        {
            UPLOAD_command(client, buffer);
        }
        if (strncmp(buffer, "$DOWNLOAD$", 10) == 0)
        {
            DOWNLOAD_command(client, buffer);
        }
        if (strncmp(buffer, "$LIST", 5) == 0)
        {
            LIST_command(client, buffer);
        }
        if (strncmp(buffer, "$DELETE$", 8) == 0)
        {
            DELETE_command(client, buffer);
        }
    }
}

int main(int argc, char **argv)
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
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving welcome message");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", buffer);

    authentication(client);
    handle_command(client);

    close(client);

    return 0;
}