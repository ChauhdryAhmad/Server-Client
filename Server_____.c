#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>


#define PORT 8080
#define MAX_CLIENTS 3
#define QUEUE_SIZE 10

const char* user_file = "users.txt";


bool thread_finished[MAX_CLIENTS] = { false };

typedef struct 
{
    int id;
    int socket_client;
    struct sockaddr_in cli_addr;
} client_info;

void error(const char *msg) 
{
    perror(msg);
    exit(1);
}

void removeNullBytes(char* str) {
    int i, j;
    for (i = 0, j = 0; str[i] != '\0'; i++) {
        if (str[i] != '\0') {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}

void UPLOAD_command(int socket_client, const char *user_dir, int* storage)
{
    char file_name[20];
    char buffer[1024];

    if (read(socket_client, buffer, 255) < 0)
        error("ERROR in reading from client");

    if(strncmp(buffer, "Valid", 5) != 0)
    {
        printf("File cannot be upload invalid path\n");
        return;
    }

    // Reading the filename
    if (read(socket_client, file_name, 20) < 0) 
        error("ERROR in reading the filename from client");
    
    printf("Here is the file name: %s\n", file_name);

    if (write(socket_client, "I got your file name", 21) < 0) 
        error("ERROR in sending the filename received message to client");
    
    // Creating file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, file_name);
    printf("File path: %s\n", file_path);

    // Check if the file already exists
    if (access(file_path, F_OK) != -1) {

        printf("File already exists: %s\n", file_path);

        // File exists, ask the client if they want to replace it
        if (write(socket_client, "Replace", 7) < 0) 
            error("ERROR in sending file existence query to client");
        
        // Read client's response
        bzero(buffer, sizeof(buffer));
        if (read(socket_client, buffer, sizeof(buffer) - 1) < 0) 
            error("ERROR in reading replace confirmation from client");
        
        if (strncmp(buffer, "yes", 3) != 0) {
            // Client chose not to replace the file
            if (write(socket_client, "File not replaced.", 19) < 0) 
                error("ERROR in sending file not replaced message to client");
            return;
        }
        if (write(socket_client, "File replacing.", 15) < 0) 
                error("ERROR in sending file replaced message to client");
    }
    else 
    {

        printf("File does not exist\n");

        if(write(socket_client, "No", 2) < 0)
            error("ERROR in sending file existence query to client");
    }

    off_t file_size = 0;

    if(read(socket_client, &file_size, sizeof(file_size)) < 0)
        error("ERROR in reading from client");

    printf("File size is %ld\n", file_size);

    if(write(socket_client, "received", 8) < 0)
        error("ERROR in sending file");

    bzero(buffer, sizeof(buffer));

    printf("Client sendings\n");

    if(read(socket_client, buffer, 255) < 0)
        error("ERROR in reading from client");

    printf("Client sent\n");

    if(write(socket_client,"sizecheck", 8) < 0)
        perror("ERROR in writing");

    int size;

    if(read(socket_client, &size, sizeof(size)) < 0)
        perror("ERROR in reading from client");
   
   // size = (int)buffer;

    if(*storage - size < 0)
    {
        printf("Storage not available\n");

        // Sending Not ok
        if(write(socket_client, "Not OK", 6) < 0)
            error("ERROR sending acknowledgment to client");
        
        return;
    }
    printf("Storage available\n");

    // Sending Not ok
    if(write(socket_client, "OK", 2) < 0)
        error("ERROR sending acknowledgment to client");

    *storage -= size;

    // Creating file (will overwrite if the client chose to replace)
    int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        error("ERROR creating file");

    bzero(buffer, sizeof(buffer));

    // Reading the content of the file
    int n;
    while ((n = read(socket_client, buffer, sizeof(buffer))) > 0) {
        if (strncmp(buffer, "eof", 3) == 0)
            break;
        printf("Bytes read: %d\n", n);
        if (write(fd, buffer, n) < 0)
            error("ERROR writing to file");
        bzero(buffer, sizeof(buffer));
        if(write(socket_client, "ok", 2) < 0)
            error("ERROR sending acknowledgment to client");
        printf("OK\n");

    }

    // Close the file
    close(fd);
    printf("File content successfully written to %s\n", file_path);

    if (write(socket_client, "I have got the content", 23) < 0)
        error("ERROR sending acknowledgment to client");

    if (write(socket_client, "File uploaded successfully", 26) < 0)
        error("ERROR sending confirmation message to client");
}

void DOWNLOAD_command(int socket_client, const char *user_dir)
{
    char file_name[20];
    char buffer[256];

    if (read(socket_client, file_name, 20) < 0) 
        error("ERROR in reading the filename from client");

    if (write(socket_client, "File name received", 18) < 0)
        error("ERROR in sending to client");

    printf("Here is the file name: %s\n", file_name);

    // Creating file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, file_name);
    printf("File path: %s\n", file_path);

    // Check if the file exists
    if (access(file_path, F_OK) == -1) {
        if (write(socket_client, "No", 2) < 0)
            error("ERROR in sending file existence query to client");
        printf("File does not exist\n");
        return;
    }

    if (write(socket_client, "exist", 5) < 0)
        error("ERROR in sending");

    printf("File exists: %s\n", file_path);

    // Opening the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) 
        error("ERROR opening file");
    
    printf("File opened\n");

    char file_buffer[100];
    int bytes_read;

    bzero(buffer, sizeof(buffer));
    read(socket_client, buffer, sizeof(buffer));

    printf("Going to read\n");
    // Reading and sending file content to server
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        printf("Sending Total Bytes: %d\n", bytes_read);
        // printf("Recieved Bytes");
        if (write(socket_client, file_buffer, bytes_read) < 0)
            error("ERROR writing file content to socket");
        printf("Reading\n");
        bzero(file_buffer, sizeof(file_buffer));
        if (read(socket_client, file_buffer, 3) < 0)
            error("ERROR reading acknowledgment from server");
        printf("OK\n");
        bzero(file_buffer, sizeof(file_buffer));
    }
    printf("File written successfully\n");

    if(write(socket_client, "eof", 3) < 0)
        error("ERROR sending EOF to server");

    bzero(file_buffer, sizeof(file_buffer));

    // if(write(socket_client, "eof", 3) < 0
    //     error("ERROR sending EOF to server");


    fclose(file);

    // bzero(file_buffer, sizeof(file_buffer));
    printf("File written successfully\n");

    if(read(socket_client, file_buffer, 25) < 0)
        error("ERROR reading acknowledgment from server");

    if(write(socket_client, "ok", 2) < 0)
    printf("File downloaded successfully");

    printf("File downloaded successfully\n");

}

void LIST_command(int socket_client, const char *user_dir, int* storage) 
{
    char file_list[1024];
    bzero(file_list, sizeof(file_list));
    int files = 0;

    if(read(socket_client, file_list, sizeof(file_list)) < 0)
        perror("ERROR reading acknowledgment from server");

    // Open the user's directory
    DIR *dir = opendir(user_dir);
    if (dir == NULL) {
        perror("ERROR opening directory");
        if (write(socket_client, "No", 2) < 0)
            perror("ERROR writing to socket");
        return;
    }

    if (write(socket_client, "Yes", 3) < 0)
        perror("ERROR writing to socket");

    printf("Yes\n");

    if(read(socket_client, file_list, sizeof(file_list)) < 0)
        perror("ERROR reading acknowledgment from server");

    printf("Client Response: %s\n", file_list);

    bzero(file_list, sizeof(file_list));
    char filename[256] = {};

    struct stat file_stat;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip '.' and '..'
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {

            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, entry->d_name);

            FILE *file = fopen(file_path, "rb");
            if (file == NULL) {
                perror("ERROR opening file");
                continue; 
            }

            fseek(file, 0L, SEEK_END);
            long file_size = ftell(file);
            rewind(file);  

            fclose(file); 

            snprintf(file_list, sizeof(file_list), "%s$%ld", entry->d_name, (long)file_size);

            if (write(socket_client, file_list, strlen(file_list)) < 0) {
                perror("ERROR writing to socket");
                closedir(dir);
                return;
            }
            bzero(file_list, sizeof(file_list));
            if(read(socket_client, file_list, sizeof(file_list)) < 0) 
                perror("ERROR reading acknowledgment from server");
            printf(file_list, "\n");
            files++;
            bzero(file_list, sizeof(file_list));
        }
    }
    closedir(dir);

    if(files == 0)
    {
        if(write(socket_client,"No file found", 13) < 0)
            perror("ERROR sending confirmation message to client");
        printf("No file found\n");
    }   

    // Indicate the end of the list
    if (write(socket_client, "EOF", 3) < 0) {
        perror("ERROR writing EOF to socket");
    }
    printf("Files sent successfully\n");

    bzero(file_list, sizeof(file_list));

    if(read(socket_client, file_list, sizeof(file_list)) < 0) 
        perror("ERROR reading acknowledgment from server");

}

void DELETE_command(int socket_client, const char *user_dir, int* storage) 
{
    char file_name[20];
    int n;
    if ((n = read(socket_client, file_name, 20)) < 0) 
        error("ERROR reading the filename from client");

    file_name[n] = '\0';

    if(write(socket_client, "Filename received", 17) < 0)
    error("ERROR in sending to client");

    printf("Here is the file name: %s\n", file_name);

    // Creating file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, file_name);
    printf("File path: %s\n", file_path);

    // Check if the file exists
    if (access(file_path, F_OK)!= -1) {

        FILE *file = fopen(file_path, "rb");
        if (file == NULL) 
            error("ERROR opening file");

        fseek(file, 0L, SEEK_END);
        long file_size = ftell(file);
        rewind(file);

        fclose(file);

        if (remove(file_path) == 0)
        {
            printf("File deleted successfully\n");
            if (write(socket_client, "File deleted successfully", 25) < 0)
              error("ERROR sending confirmation message to client");
            *storage += file_size;
            
        }
        else
            error("Error deleting file\n");
    } 
    else
    {
        printf("File not found\n");
        if (write(socket_client, "File not found", 14) < 0)
            error("ERROR sending confirmation message to client");
    }
}

void handle_client(int socket_client) 
{

    // int flags = fcntl(socket_client, F_GETFL, 0);
    // fcntl(socket_client, F_SETFL, flags & ~O_NONBLOCK);

    char auth[64];
    char user_name[50];
    char password[50];
    char u_and_p[100] = {};
    char buffer[256] = {};
    int storage;
    int id;
    while(1)
    {
        int status;
        memset(auth, 0, sizeof(auth));
        if (read(socket_client, auth, sizeof(auth)) < 0)
        error("ERROR reading authentication token from client");

        if(write(socket_client,"ok", 2) < 0)
            perror("ERROR writing authentication token");
    
        if(strcmp(auth, "signup") == 0)
        {

            storage = 10000;

            bzero(u_and_p, sizeof(u_and_p));

            // Read the username from the client
            if (read(socket_client, u_and_p, sizeof(u_and_p) - 1) < 0) 
                error("ERROR reading username from client");

            size_t len = strlen(u_and_p);
            if (len > 0 && u_and_p[len - 1] == '\n') {
                u_and_p[len - 1] = '\0';
            }

            bzero(&user_name, sizeof(user_name));
            bzero(&password, sizeof(password));

            char* token = strtok(u_and_p, "$");
            if(token != NULL)
                strcpy(user_name, token);
            else {
                if(write(socket_client, "Invalid username", 16) < 0)
                    perror("ERROR writing authentication token");
                continue;
            }

            token = strtok(NULL, "$");
            if(token!= NULL)
                strcpy(password, token);
            else {
                if(write(socket_client, "Invalid password", 16) < 0)
                    perror("ERROR writing authentication token");
                continue;
            }

            // Trim newline character from the username
            len = strlen(user_name);
            if (len > 0 && user_name[len - 1] == '\n') {
                user_name[len - 1] = '\0';
            }

            // Trim newline character from the password
            len = strlen(password);
            if (len > 0 && password[len - 1] == '\n') {
                password[len - 1] = '\0';
            }

            printf("Username received: %s\n", user_name);
            printf("Password received: %s\n", password);

            // Check if username already exists
            FILE *read_file = fopen(user_file, "r+");
            if (read_file == NULL) {
                error("ERROR opening users.txt");
            }

            int total_users = 0;
            fscanf(read_file, "%d\n", &total_users);

            char line[256];
            while (fgets(line, sizeof(line), read_file)) {
                char existing_user[50];
                int num;
                sscanf(line, "%d %s", &num, existing_user);
                if (strcmp(existing_user, user_name) == 0) {
                    printf("Username already exists: %s\n", user_name);
                    if (write(socket_client, "E", 1) < 0)
                        perror("Error in sending to client");
                    fclose(read_file);
                    continue;
                }
            }
            fclose(read_file);
            if (write(socket_client, "NE", 2) < 0)
                perror("Error in sending to client");

            FILE *write_file = fopen(user_file, "r+");
            if (write_file == NULL) {
                error("ERROR opening users.txt for writing");
            }

            total_users++;
            rewind(write_file); 
            fprintf(write_file, "%d\n", total_users); 
            id = total_users;

            fseek(write_file, 0, SEEK_END); 
            fprintf(write_file, "%d %s %s %d\n", total_users, user_name, password, storage);  

            fclose(write_file);
            
            printf("User signed up successfully: %s\n", user_name);

            status = 1;
        }
        else
        {    

            bzero(u_and_p, sizeof(u_and_p));

            // Read the username and password from the client
            if (read(socket_client, u_and_p, sizeof(u_and_p) - 1) < 0) 
                error("ERROR reading username and password from client");

            size_t len = strlen(u_and_p);
            if (len > 0 && u_and_p[len - 1] == '\n') {
                u_and_p[len - 1] = '\0';
            }

            bzero(&user_name, sizeof(user_name));
            bzero(&password, sizeof(password));

            // Split the username and password using '$' as the delimiter
            char* token = strtok(u_and_p, "$");
            if (token != NULL)
                strcpy(user_name, token);
            else {
                if (write(socket_client, "Invalid username", 16) < 0)
                    perror("ERROR writing authentication token");
                continue;
            }

            token = strtok(NULL, "$");
            if (token != NULL)
                strcpy(password, token);
            else {
                if (write(socket_client, "Invalid password", 16) < 0)
                    perror("ERROR writing authentication token");
                continue;
            }

            // Trim newline character from the username and password
            len = strlen(user_name);
            if (len > 0 && user_name[len - 1] == '\n') {
                user_name[len - 1] = '\0';
            }

            len = strlen(password);
            if (len > 0 && password[len - 1] == '\n') {
                password[len - 1] = '\0';
            }

            printf("Username received: %s\n", user_name);
            printf("Password received: %s\n", password);

            // Open users.txt in read mode to verify credentials
            FILE *read_file = fopen(user_file, "r");
            if (read_file == NULL) {
                error("ERROR opening users.txt");
            }

            char line[256];
            int login_success = 0;
            int s;
            // Check if the username and password match any entry in the file
            while (fgets(line, sizeof(line), read_file)) {
                char existing_user[50], existing_pass[50];
                
                sscanf(line, "%d %s %s %d", &id, existing_user, existing_pass, &s);

                if (strcmp(existing_user, user_name) == 0) {
                    if (strcmp(existing_pass, password) == 0) {
                        login_success = 1;
                        break;
                    } else {
                        login_success = 0;
                        break;
                    }
                }
            }
            fclose(read_file);

            if (login_success) {
                printf("Login successful for user: %s\n", user_name);
                if (write(socket_client, "Login successful", 16) < 0) {
                    perror("ERROR sending login success message to client");
                }
                status = 1;
                storage = s; 
            } else {
                printf("Login failed for user: %s\n", user_name);
                if (write(socket_client, "Login failed", 12) < 0) {
                    perror("ERROR sending login failure message to client");
                }
                status = 0;
            }

            
        }
        
        if(status == 0)
            continue;

        break;
    }

    // Creating path of user named directory
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "database/%s", user_name);

    // Creating directory if it doesn't exist
    struct stat dir = {0};
    if (stat(user_dir, &dir) == -1) {
        if (mkdir(user_dir, 0700) == 0) 
        {
            printf("Directory created: %s\n", user_dir);
        }
        else 
            error("ERROR creating user directory");
        
    } else 
    {    
        printf("Directory already exists: %s\n", user_dir);
    }


    while (1) 
    {
        bzero(buffer, 256);

        // Reading commands
        if (read(socket_client, buffer, 255) < 0) 
            error("ERROR on reading command from client");
        
        // File Upload command
        if (strncmp(buffer, "UPLOAD", 6) == 0)
        {
            if (write(socket_client, "Upload command", 14) < 0)
                error("ERROR writing to socket");
            UPLOAD_command(socket_client, user_dir, &storage);
        }

        // File Download command
        if(strncmp(buffer, "DOWNLOAD", 8) == 0)
        {   
            if (write(socket_client, "Download command", 16) < 0)
                error("ERROR writing to socket");
            DOWNLOAD_command(socket_client, user_dir);
        }

        // List command
        if (strncmp(buffer, "LIST", 4) == 0) 
        {   
            if (write(socket_client, "List command", 12) < 0)
                error("ERROR writing to socket");
            LIST_command(socket_client, user_dir, &storage);
        }

        // Delete command
        if (strncmp(buffer, "DELETE", 6) == 0) 
        {   
            if (write(socket_client, "Delete command", 14) < 0)
                error("ERROR writing to socket");
            DELETE_command(socket_client, user_dir, &storage);
        }
        
        // Storages command
        if (strncmp(buffer, "STORAGE", 7) == 0) {
            if (write(socket_client, &storage, sizeof(storage)) < 0)
                perror("Error in sending storage to client");
            printf("Sent storage: %d\n", storage);
        }

        // End command
        if (strncmp(buffer, "END", 3) == 0) {
            printf("Received 'END' command. Closing connection.\n");

            FILE *file = fopen(user_file, "r+");
            if (file == NULL) {
                error("ERROR opening users.txt for reading and updating");
            }

            int total_users = 0;
            fscanf(file, "%d\n", &total_users);  

            // Create a temporary file to store updated user information
            FILE *temp_file = fopen("temp_users.txt", "w");
            if (temp_file == NULL) {
                error("ERROR opening temp file for writing");
            }

            fprintf(temp_file, "%d\n", total_users);  

            char line[256];
            int user_id, current_storage;
            char existing_user[50], existing_password[50];

            while (fgets(line, sizeof(line), file)) {
                sscanf(line, "%d %s %s %d", &user_id, existing_user, existing_password, &current_storage);

                if (id == user_id) {
                    printf("Updating storage for user ID %d (%s) from %d to %d\n", user_id, existing_user, current_storage, storage);
                    fprintf(temp_file, "%d %s %s %d\n", id, existing_user, existing_password, storage);  // Write updated storage
                } else {
                    fprintf(temp_file, "%d %s %s %d\n", user_id, existing_user, existing_password, current_storage);
                }
            }

            fclose(file);
            fclose(temp_file);

            remove(user_file);
            rename("temp_users.txt", user_file);  

            printf("Storage updated successfully for user ID %d. Closing connection.\n", id);

            break;  
        }
    }

    // Close the client's socket
    close(socket_client);
}

void *handle_client_thread(void *client_data) 
{
    client_info *c_info = (client_info *)client_data;
    if (write(c_info->socket_client, "Connected", 9) < 0)
        perror("ERROR writing to socket");
    handle_client(c_info->socket_client);
    thread_finished[c_info->id] = false;
    close(c_info->socket_client);
    free(c_info);
    pthread_exit(NULL);
}


int main1() 
{
    int socket_server;
    socklen_t clilen;
    struct sockaddr_in serv_addr;

    // Initialize socket
    socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_server < 0)
        error("ERROR on creating socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Bind the socket to an address and port
    if (bind(socket_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // Start listening for incoming connections
    listen(socket_server, 5);

    // Ensure user file exists
    if (access(user_file, F_OK) == -1)
    {
        FILE *fp = fopen(user_file, "w");
        fclose(fp);
    }

    while (1) {
        client_info c_info;
        clilen = sizeof(c_info.cli_addr);

        // Accept incoming connection
        c_info.socket_client = accept(socket_server, (struct sockaddr *)&c_info.cli_addr, &clilen);
        if (c_info.socket_client < 0)
            error("ERROR on accepting client");

        // Process the client in a single thread (main thread)
        printf("Handling client...\n");

        if (write(c_info.socket_client, "Connected", 9) < 0)
            perror("ERROR writing to socket");

        handle_client(c_info.socket_client);

        // Close the client's socket after handling the client
        close(c_info.socket_client);
    }

    // Close the server socket
    close(socket_server);
    return 0;
}



int main() 
{
    int socket_server;
    socklen_t clilen;
    struct sockaddr_in serv_addr;
    pthread_t threads[MAX_CLIENTS];
    int client_count = 0;

    client_info *client_queue[QUEUE_SIZE];
    int queue_front = 0, queue_rear = 0;

    socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_server < 0)
        error("ERROR on creating socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(socket_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(socket_server, 5);

    if(access(user_file, F_OK) == -1)
    {
        FILE *fp = fopen(user_file, "w");
        fclose(fp);
    }

    while (1) {
        client_info *c_info = malloc(sizeof(client_info));
        if (!c_info)
            error("ERROR allocating memory for client_info");

        clilen = sizeof(c_info->cli_addr);
        c_info->socket_client = accept(socket_server, (struct sockaddr *)&(c_info->cli_addr), &clilen);
        if (c_info->socket_client < 0) {
            free(c_info);
            error("ERROR on accepting client");
        }

        if (client_count < MAX_CLIENTS) {
            c_info->id = client_count;
            if (pthread_create(&threads[client_count], NULL, handle_client_thread, (void *)c_info) < 0)
                error("ERROR creating thread");
            thread_finished[client_count] = true;
            pthread_detach(threads[client_count]);
            client_count++;
        } else {
            if ((queue_rear + 1) % QUEUE_SIZE == queue_front) {
                close(c_info->socket_client);
                free(c_info);
                printf("Client queue is full. Connection closed.\n");
            } else {
                client_queue[queue_rear] = c_info;
                queue_rear = (queue_rear + 1) % QUEUE_SIZE;
                printf("Client added to the queue.\n");
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            if (!thread_finished[i]) 
            {
                if (pthread_join(threads[i], NULL) == 0) {
                    client_count--;

                    if (queue_front != queue_rear) {
                        client_info *queued_client = client_queue[queue_front];
                        queue_front = (queue_front + 1) % QUEUE_SIZE;
                        queued_client->id = i;
                        if (pthread_create(&threads[i], NULL, handle_client_thread, (void *)queued_client) < 0)
                            error("ERROR creating thread for queued client");
                        thread_finished[i] = true;
                        pthread_detach(threads[i]);
                        client_count++;
                    }
                }
            }
        }
    }

    close(socket_server);
    return 0;
}














