#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define PORT 8080
#define users "users.txt"

struct client_info
{
    char *username;
    char *password;
    long int storage;
};

struct client_info authentication(int client)
{
    char auth;
    char username[20], password[20];
    char buffer[256];
    struct client_info c;
    c.username = (char *)malloc(21 * sizeof(char));
    c.password = (char *)malloc(21 * sizeof(char));

    while (1)
    {
        if (recv(client, &auth, sizeof(auth), 0) < 0)
        {
            perror("Error receiving authentication token");
            exit(EXIT_FAILURE);
        }

        if (auth == 'S' || auth == 's')
        {
            if (send(client, "Sign up\0", 8, 0) < 0)
            {
                perror("Error sending sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Authentication Type: Sign up\n");
            fflush(stdout);

            if (recv(client, username, sizeof(username), 0) < 0)
            {
                perror("Error receiving username");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Username recived\0", 17, 0) < 0)
            {
                perror("Error sending username received message");
                exit(EXIT_FAILURE);
            }

            if (recv(client, password, sizeof(password), 0) < 0)
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

            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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
            while (fgets(buffer, sizeof(buffer), file))
            {
                char existing[20];
                sscanf(buffer, "%s", existing);
                if (strcmp(existing, username) == 0)
                {
                    check = 1;
                    printf("Username already exists\n");
                    if (send(client, "E", 1, 0) < 0)
                    {
                        perror("Error sending existing message");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                memset(buffer, '\0', sizeof(buffer));
                memset(existing, '\0', sizeof(existing));
            }
            if (check == 1)
            {
                fclose(file);
                continue;
            }
            printf("Username dose not exists\n");
            if (send(client, "NE", 2, 0) < 0)
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
            fprintf(file, "\n%s %s 99999999", username, password);
            fclose(file);
            if (send(client, "User created successfully", 25, 0) < 0)
            {
                perror("Error sending user created message");
                exit(EXIT_FAILURE);
            }
            printf("Signup successfully\n");
            strncpy(c.username, username, 20);
            strncpy(c.password, password, 20);
            c.storage = 99999999;
            return c;
        }
        if (auth == 'L' || auth == 'l')
        {
            if (send(client, "Login\0", 7, 0) < 0)
            {
                perror("Error sending sign up message");
                exit(EXIT_FAILURE);
            }
            printf("Authentication Type: Login\n");
            fflush(stdout);
            if (recv(client, username, sizeof(username), 0) < 0)
            {
                perror("Error receiving username");
                exit(EXIT_FAILURE);
            }

            if (send(client, "Username recived\0", 17, 0) < 0)
            {
                perror("Error sending username received message");
                exit(EXIT_FAILURE);
            }

            if (recv(client, password, sizeof(password), 0) < 0)
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

            if (recv(client, buffer, sizeof(buffer), 0) < 0)
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
            long int s;
            memset(buffer, '\0', sizeof(buffer));
            int check = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                char name[20], pass[20];
                sscanf(buffer, "%s %s %ld", name, pass, &s);
                if ((strcmp(username, name) == 0) && (strcmp(password, pass) == 0))
                {
                    check = 1;
                    printf("Authentication successful\n");
                    if (send(client, "S", 1, 0) < 0)
                    {
                        perror("Error sending login success message to client");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                memset(buffer, '\0', sizeof(buffer));
            }
            if (!check)
            {
                printf("Authentication failed\n");
                if (send(client, "F", 1, 0) < 0)
                {
                    perror("Error sending authentication failed message");
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            printf("Welcome %s\n", username);
            strncpy(c.username, username, 20);
            strncpy(c.password, password, 20);
            // c.storage = 10;
            return c;
        }
    }
}

void UPLOAD_command(int client, const char *user_dir, long int *storage)
{
    if (send(client, "Upload command recived", 22, 0) < 0)
    {
        perror("Error sending upload command message");
        exit(EXIT_FAILURE);
    }
    char filename[20];
    char buffer[1024];

    memset(filename, '\0', sizeof(filename));

    if (recv(client, filename, sizeof(filename), 0) < 0)
    {
        perror("Error receiving file path");
        exit(EXIT_FAILURE);
    }

    printf("File path: %s\n", filename);
    fflush(stdout);

    // if (access(file_path, F_OK) == -1)
    // {
    //     printf("File does not exist\n");
    //     fflush(stdout);
    //     if (send(client, "IV", 2, 0) < 0)
    //     {
    //         perror("Error sending file does not exist message");
    //         exit(EXIT_FAILURE);
    //     }
    //     return;
    // }
    // else
    // {
    //     printf("File exists\n");
    //     fflush(stdout);
    //     if (send(client, "V", 1, 0) < 0)
    //     {
    //         perror("Error sending file exists message");
    //         exit(EXIT_FAILURE);
    //     }
    // }

    if(send(client, "Filename recived", 16, 0) < 0)
    {
        perror("Error sending file recived message");
        exit(EXIT_FAILURE);
    }

    // const char *filename_ptr = strrchr(file_path, '/');
    // if (filename_ptr)
    //     strcpy(file_name, filename_ptr + 1);
    // else
    //     strcpy(file_name, file_path);

    // printf("File name: %s\n", file_name);
    // fflush(stdout);

    memset(buffer, '\0', sizeof(buffer));

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }
    printf("Client says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));

    char path[64];
    snprintf(path, sizeof(path), "%s/%s", user_dir, filename);
    if (access(path, F_OK) == 0)
    {
        printf("File already exists in user directory\n");
        fflush(stdout);
        if (send(client, "E", 2, 0) < 0)
        {
            perror("Error sending file already exists message");
            exit(EXIT_FAILURE);
        }
        if (recv(client, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Error receiving Replacing file message");
            exit(EXIT_FAILURE);
        }
        memset(buffer, '\0', sizeof(buffer));
        if (strncmp(buffer, "No", 2) == 0)
        {
            printf("Not Replacing file\n");
            return;
        }
        printf("Replacing file\n");
        struct stat file_st;
        stat(path, &file_st);
        *storage += file_st.st_size;
    }
    else
    {
        printf("File dose not exist in user directory\n");
        fflush(stdout);
        if (send(client, "NE", 2, 0) < 0)
        {
            perror("Error sending file already exists message");
            exit(EXIT_FAILURE);
        }
        if (recv(client, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Error receiving Replacing file message");
            exit(EXIT_FAILURE);
        }
        memset(buffer, '\0', sizeof(buffer));
    }

    struct stat file_st;
    stat(filename, &file_st);
    if (*storage - file_st.st_size < 0)
    {
        printf("Not enough storage\n");
        fflush(stdout);
        if (send(client, "LS", 1, 0) < 0)
        {
            perror("Error sending not enough storage message");
            exit(EXIT_FAILURE);
        }
        return;
    }
    else
    {
        printf("Enough storage\n");
        fflush(stdout);
        if (send(client, "ES", 2, 0) < 0)
        {
            perror("Error sending enough storage message");
            exit(EXIT_FAILURE);
        }
    }
    FILE *file;
    file = fopen(path, "w");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    int bytes_read;
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

    fclose(file);

    *storage -= file_st.st_size;

    if (send(client, "File uploaded successfully", 26, 0) < 0)
    {
        perror("Error sending file uploaded successfully message");
        exit(EXIT_FAILURE);
    }

    printf("File uploaded successfully\n");
    fflush(stdout);
}

void DOWNLOAD_command(int client, const char *user_dir)
{
    if (send(client, "Download command received", 25, 0) < 0)
    {
        perror("Error sending download command message");
        exit(EXIT_FAILURE);
    }
    char file_name[64];
    char buffer[1024];
    memset(file_name, '\0', sizeof(file_name));
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, file_name, sizeof(file_name), 0) < 0)
    {
        perror("Error receiving file name");
        exit(EXIT_FAILURE);
    }
    printf("File name: %s\n", file_name);
    fflush(stdout);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", user_dir, file_name);

    if (access(path, F_OK) == -1)
    {
        printf("File does not exist\n");
        fflush(stdout);
        if (send(client, "NE", 2, 0) < 0)
        {
            perror("Error sending file does not exist message");
            exit(EXIT_FAILURE);
        }
        return;
    }
    printf("File exists\n");
    fflush(stdout);
    if (send(client, "E", 1, 0) < 0)
    {
        perror("Error sending file exists message");
        exit(EXIT_FAILURE);
    }

    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }

    printf("Client says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));

    FILE *file;
    file = fopen(path, "rb");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    int byte_read;
    while (byte_read = fread(buffer, 1, sizeof(buffer), file))
    {
        if (send(client, buffer, byte_read, 0) < 0)
        {
            perror("Error sending file data");
            exit(EXIT_FAILURE);
        }
        printf("Sent %d bytes\n", byte_read);
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
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    memset(buffer, '\0', sizeof(buffer));
    if (send(client, "File Downloaded successfully", 28, 0) < 0)
    {
        perror("Error sending file downloaded successfully message");
        exit(EXIT_FAILURE);
    }
    printf("File downloaded successfully\n");
}

void LIST_command(int client, const char *user_dir)
{
    if (send(client, "List command received", 21, 0) < 0)
    {
        perror("Error sending list command message");
        exit(EXIT_FAILURE);
    }
    char buffer[256];
    struct dirent *de;
    DIR *dr = opendir(user_dir);
    if (dr == NULL)
    {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }

    printf("Client says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));

    while (de = readdir(dr))
    {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            struct stat file_st;
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, de->d_name);
            stat(file_path, &file_st);
            char filename[512];
            snprintf(filename, sizeof(filename), "$%s$%ld", de->d_name, file_st.st_size);
            if (send(client, filename, sizeof(filename), 0) < 0)
            {
                perror("Error sending filename");
                exit(EXIT_FAILURE);
            }
            if (recv(client, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Error receiving confirmation message");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));
        }
    }
    if (send(client, "EOF", 3, 0) < 0)
    {
        perror("Error sending EOF message");
        exit(EXIT_FAILURE);
    }
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving confirmation message");
        exit(EXIT_FAILURE);
    }
    if (send(client, "All Files sent successfully", 27, 0) < 0)
    {
        perror("Error sending all files sent successfully message");
        exit(EXIT_FAILURE);
    }
    printf("All files sent successfully\n");
    fflush(stdout);
}

void DELETE_command(int client, const char *user_dir, long int *storage)
{
    if (send(client, "Delete command received", 23, 0) < 0)
    {
        perror("Error sending delete command message");
        exit(EXIT_FAILURE);
    }
    char file_name[64];
    char buffer[128];
    memset(file_name, '\0', sizeof(file_name));
    memset(buffer, '\0', sizeof(buffer));
    if (recv(client, file_name, sizeof(file_name), 0) < 0)
    {
        perror("Error receiving file name");
        exit(EXIT_FAILURE);
    }
    printf("File name: %s\n", file_name);
    fflush(stdout);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", user_dir, file_name);

    if (access(path, F_OK) == -1)
    {
        printf("File does not exist\n");
        fflush(stdout);
        if (send(client, "NE", 2, 0) < 0)
        {
            perror("Error sending file does not exist message");
            exit(EXIT_FAILURE);
        }
        return;
    }
    printf("File exists\n");
    fflush(stdout);
    if (send(client, "E", 1, 0) < 0)
    {
        perror("Error sending file does not exist message");
        exit(EXIT_FAILURE);
    }
    if (recv(client, buffer, sizeof(buffer), 0) < 0)
    {
        perror("Error receiving message");
        exit(EXIT_FAILURE);
    }
    printf("Client says: %s\n", buffer);
    fflush(stdout);
    memset(buffer, '\0', sizeof(buffer));
    struct stat file_st;
    stat(path, &file_st);
    *storage -= file_st.st_size;
    remove(path);
    if (send(client, "File deleted successfully", 25, 0) < 0)
    {
        perror("Error sending file deleted successfully message");
        exit(EXIT_FAILURE);
    }
    printf("File deleted successfully\n");
    fflush(stdout);
}

void handle_client(int client)
{
    struct client_info c;
    c = authentication(client);
    char buffer[256];

    // Creating directory if it doesn't exist
    char user_dir[128];
    snprintf(user_dir, sizeof(user_dir), "database/%s", c.username);
    struct stat dir;
    if (stat(user_dir, &dir) == -1)
    {
        mkdir(user_dir, 0700);
    }

    while (1)
    {
        memset(buffer, '\0', sizeof(buffer));
        if (recv(client, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Error in reciving command");
            exit(EXIT_FAILURE);
        }
        printf("Client says: %s\n", buffer);
        if (strncmp(buffer, "UPLOAD", 6) == 0)
        {
            UPLOAD_command(client, user_dir, &c.storage);
        }
        if (strncmp(buffer, "DOWNLOAD", 8) == 0)
        {
            DOWNLOAD_command(client, user_dir);
        }
        if (strncmp(buffer, "LIST", 4) == 0)
        {
            LIST_command(client, user_dir);
        }
        if (strncmp(buffer, "DELETE", 6) == 0)
        {
            DELETE_command(client, user_dir, &c.storage);
        }
    }
}

int main(int argc, char **argv)
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

    if (access(users, F_OK) == -1)
    {
        FILE *fp;
        fp = fopen(users, "w");
    }

    struct stat dir;
    if (stat("/database", &dir) == -1)
    {
        mkdir("/database", 0700);
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

    // authentication(client);

    handle_client(client);

    close(server);
    close(client);

    return 0;
}