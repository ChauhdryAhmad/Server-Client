#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include "p4.c"

#define PORT 8080
#define users "users.txt"
#define MAX_CLIENTS 10
#define QUEUE_SIZE 10

typedef struct FileNode {
    char filename[256];
    int uid; 
    int storage;
    struct FileNode *next;
} FileNode;

typedef struct FileList {
    FileNode *head;
    pthread_mutex_t mutex;
} FileList;

FileList file_list;
FileList st_list;

void init_file_list(FileList *list) 
{
    list->head = NULL;
    pthread_mutex_init(&list->mutex, NULL);
}

// int is_file_in_use(FileList *list, const char *filename, int uid) 
// {
//     pthread_mutex_lock(&list->mutex);
//     FileNode *current = list->head;
//     while (current != NULL) {
//         if (strcmp(current->filename, filename) == 0 && current->uid != uid) {
//             pthread_mutex_unlock(&list->mutex);
//             return 1; 
//         }
//         current = current->next;
//     }
//     pthread_mutex_unlock(&list->mutex);
//     return 0; 
// }

int is_file_in_use(FileList *list, int uid) 
{
    pthread_mutex_lock(&list->mutex);
    FileNode *current = list->head;
    while (current != NULL) {
        if (current->uid != uid) {
            pthread_mutex_unlock(&list->mutex);
            return 1; 
        }
        current = current->next;
    }
    pthread_mutex_unlock(&list->mutex);
    return 0; 
}

void add_file_to_list(FileList *list, const char *filename, int uid, int storage) {
    pthread_mutex_lock(&list->mutex);
    FileNode *new_node = malloc(sizeof(FileNode));
    strcpy(new_node->filename, filename);
    new_node->uid = uid;
    new_node->next = list->head;
    new_node->storage = storage;
    list->head = new_node;
    pthread_mutex_unlock(&list->mutex);
}

FileNode* remove_file_from_list(FileList *list, const char *filename, int uid) {
    pthread_mutex_lock(&list->mutex);
    FileNode *current = list->head;
    FileNode *prev = NULL;
    FileNode *res;

    while (current != NULL) {
        if (current->uid == uid) {
            if (prev == NULL) { 
                list->head = current->next;
            } else {
                prev->next = current->next;
            }
            res = current;
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&list->mutex);
    return res;
}

typedef struct request {
    int client_socket;         
    int uid;    
    int storage;               
    char command[16];          
    char filename[256];
    char user_dir[128];
    int iscomplete;        
    struct request *next;      
} Request;

typedef struct queue {
    Request *front;            
    Request *rear;    
    int sz;         
    pthread_mutex_t lock;      
    pthread_cond_t not_empty;  
} Queue;

Queue request_queue;
Queue working_queue;

void init_queue(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->sz = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void enqueue(Queue *q, Request *req) 
{
    // Allocate memory for a new Request
    Request *new_request = (Request *)malloc(sizeof(Request));
    if (new_request == NULL) {
        perror("Failed to allocate memory for new request");
        exit(EXIT_FAILURE);
    }

    *new_request = *req;
    new_request->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->rear == NULL) {
        q->front = q->rear = new_request;  
    } else {
        q->rear->next = new_request;       
        q->rear = new_request;             
    }
    q->sz++;
    pthread_cond_signal(&q->not_empty);    
    pthread_mutex_unlock(&q->lock);
}

Request *dequeue(Queue *q) 
{
    pthread_mutex_lock(&q->lock);
    while (q->front == NULL) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    Request *req = q->front;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    q->sz--;
    pthread_mutex_unlock(&q->lock);
    return req;
}

int get_queue_size(Queue *q) {
    pthread_mutex_lock(&q->lock);
    int size = q->sz;  // Get current queue size
    pthread_mutex_unlock(&q->lock);
    return size;
}

int isEmpty(Queue *q) {
    pthread_mutex_lock(&q->lock);
    int empty = (q->sz == 0);
    pthread_mutex_unlock(&q->lock);
    return empty;
}

void destroy_queue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    Request *current = q->front;
    while (current) {
        Request *temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_unlock(&q->lock);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
}





pthread_t client_threads[MAX_CLIENTS];
pthread_mutex_t client_mutex;

int client_arr[MAX_CLIENTS];

int client_count = 0;

int find_empty_slot() 
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_arr[i] == 0) {
            client_arr[i] = 1;
            return i;
        }
    }
    return -1;
}

typedef struct {
    int client_socket;
    int index;
} ThreadArgs;

int client_queue[QUEUE_SIZE];
int front = -1, rear = -1;

int is_queue_full() {
    return ((rear + 1) % QUEUE_SIZE == front);
}

int is_queue_empty() {
    return (front == -1);
}

void insert_queue(int client) {
    if (is_queue_full()) {
        printf("Queue is full, rejecting client.\n");
        close(client); 
        return;
    }
    if (is_queue_empty()) {
        front = rear = 0;
    } else {
        rear = (rear + 1) % QUEUE_SIZE;
    }
    client_queue[rear] = client;
}

int pop_queue() {
    if (is_queue_empty()) {
        return -1;
    }
    int client = client_queue[front];
    if (front == rear) {
        front = rear = -1; 
    } else {
        front = (front + 1) % QUEUE_SIZE;
    }
    return client;
}

struct client_info
{
    int uid;
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
            int total;
            fscanf(file, "%d\n", &total);
            memset(buffer, '\0', sizeof(buffer));
            int check = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                int curr;
                char existing[20];
                sscanf(buffer, "%d %s", &curr, existing);
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
            fprintf(file, "\n%d %s %s 99999999", total, username, password);
            c.uid = total;
            total++;
            rewind(file);  // Go back to the beginning of the file
            fprintf(file, "%d\n", total);
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
            int total;
            fscanf(file, "%d\n", &total);
            long int s;
            memset(buffer, '\0', sizeof(buffer));
            int check = 0;
            int curr;
            while (fgets(buffer, sizeof(buffer), file))
            {
                char name[20], pass[20];
                sscanf(buffer, "%d %s %s %ld", &curr, name, pass, &s);
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
            c.uid = curr;
            printf("Welcome %s\n", username);
            strncpy(c.username, username, 20);
            strncpy(c.password, password, 20);
            c.storage = s;
            return c;
        }
    }
}

void UPLOAD_command(int client, const char *user_dir, const char *filename, long int *storage)
{
    
    char buffer[1024];

    
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

    if(send(client, "Send File size", 14, 0) < 0)
    {
        perror("Error sending file size message");
        exit(EXIT_FAILURE);
    }

    long int size;
    if(recv(client, &size, sizeof(size), 0) < 0)
    {
        perror("Error receiving file size");
        exit(EXIT_FAILURE);
    }

    if (*storage - size < 0)
    {
        printf("Not enough storage\n");
        fflush(stdout);
        if (send(client, "LS", 2, 0) < 0)
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
        if (send(client, &bytes_read, sizeof(int), 0) < 0)
        {
            perror("Error sending bytes read message");
            exit(EXIT_FAILURE);
        }
        printf("Bytes read: %d\n", bytes_read);
    }

    fclose(file);

    *storage -= size;

    if (send(client, "File uploaded successfully", 26, 0) < 0)
    {
        perror("Error sending file uploaded successfully message");
        exit(EXIT_FAILURE);
    }

    printf("File uploaded successfully\n");
    fflush(stdout);
}

void DOWNLOAD_command(int client, const char *user_dir, const char *filename)
{
    
    char buffer[200];
    memset(buffer, '\0', sizeof(buffer));
    

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", user_dir, filename);

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

void DELETE_command(int client, const char *user_dir, const char *file_name, long int *storage)
{
    
    char buffer[128];
    memset(buffer, '\0', sizeof(buffer));
    

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

void END_command(int client, int uid, long int storage)
{
    if (write(client, "Goodbye", 7) < 0) {
        perror("Error sending goodbye message");
        exit(EXIT_FAILURE);
    }

    printf("Received 'END' command. Updating storage and closing connection.\n");

    FILE *file = fopen(users, "r+");
    if (file == NULL) {
        perror("ERROR opening users.txt for reading and updating");
        exit(EXIT_FAILURE);
    }

    int total_users = 0;
    if (fscanf(file, "%d\n", &total_users) != 1) {
        perror("ERROR reading total users from file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    FILE *temp_file = fopen("temp_users.txt", "w");
    if (temp_file == NULL) {
        perror("ERROR opening temp file for writing");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fprintf(temp_file, "%d\n", total_users);

    char line[256];
    int id;
    long int current_storage;
    char existing_user[50], existing_password[50];

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%d %s %s %ld", &id, existing_user, existing_password, &current_storage) == 4) {
            if (id == uid) {
                printf("Updating storage for user ID %d (%s) from %ld to %ld\n", id, existing_user, current_storage, storage);
                fprintf(temp_file, "%d %s %s %ld\n", id, existing_user, existing_password, storage);
            } else {
                fprintf(temp_file, "%d %s %s %ld\n", id, existing_user, existing_password, current_storage);
            }
        }
    }

    fclose(file);
    fclose(temp_file);

    if (remove(users) != 0) {
        perror("ERROR removing original users file");
        exit(EXIT_FAILURE);
    }

    if (rename("temp_users.txt", users) != 0) {
        perror("ERROR renaming temp file to users file");
        exit(EXIT_FAILURE);
    }

    printf("Storage updated successfully for user ID %d. Closing connection.\n", uid);

}

pthread_cond_t req_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t req_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        void* ptr1 = my_malloc(64);
        void* ptr2 = my_malloc(128);
        void* ptr3 = my_malloc(32);
        void* ptr4 = my_malloc(256);
        void* ptr5 = my_malloc(16);
        my_free(ptr1);
        print_bins();
        printf("\nAllocated: %p, %p, %p, %p, %p\n", ptr1, ptr2, ptr3, ptr4, ptr5);

    
        printf("\nState of bins after allocation:\n");
        print_bins();

        char buffer[64];
        Request *req = (Request *)malloc(sizeof(Request));
        if (req == NULL) {
            perror("Failed to allocate memory for new request");
            exit(EXIT_FAILURE);
        }
        memset(buffer, '\0', sizeof(buffer));
        if (recv(client, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Error in reciving command");
            exit(EXIT_FAILURE);
        }
        printf("Client says: %s\n", buffer);

        strncpy(req->user_dir, user_dir, sizeof(user_dir));

        if (strncmp(buffer, "UPLOAD", 6) == 0)
        {

            if (send(client, "Upload command recived", 22, 0) < 0)
            {
                perror("Error sending upload command message");
                exit(EXIT_FAILURE);
            }
            char filename[20];
            memset(filename, '\0', sizeof(filename));

            if (recv(client, filename, sizeof(filename), 0) < 0)
            {
                perror("Error receiving file path");
                exit(EXIT_FAILURE);
            }

            printf("File path: %s\n", filename);
            fflush(stdout);

            if(send(client, "Filename recived", 16, 0) < 0)
            {
                perror("Error sending file recived message");
                exit(EXIT_FAILURE);
            }

            req->client_socket = client;
            req->iscomplete = 1;
            req->uid = c.uid;
            req->storage = c.storage;
            strncpy(req->filename, filename, sizeof(filename));
            strncpy(req->command, "UPLOAD", 6);
            // UPLOAD_command(client, user_dir, &c.storage);
        }

        else if (strncmp(buffer, "DOWNLOAD", 8) == 0)
        {
            if (send(client, "Download command received", 25, 0) < 0)
            {
                perror("Error sending download command message");
                exit(EXIT_FAILURE);
            }
            char filename[64];
            memset(filename, '\0', sizeof(filename));

            if (recv(client, filename, sizeof(filename), 0) < 0)
            {
                perror("Error receiving file name");
                exit(EXIT_FAILURE);
            }
            printf("File name: %s\n", filename);
            fflush(stdout);
            req->client_socket = client;
            req->iscomplete = 1;
            req->uid = c.uid;
            req->storage = c.storage;
            strncpy(req->filename, filename, sizeof(filename));
            strncpy(req->command, "DOWNLOAD", 8);
            // DOWNLOAD_command(client, user_dir, filename);
        }

        else if (strncmp(buffer, "LIST", 4) == 0)
        {
            req->client_socket = client;
            req->iscomplete = 1;
            req->uid = c.uid;
            req->storage = c.storage;
            strncpy(req->filename, "\0", 1);
            strncpy(req->command, "LIST", 4);
            // LIST_command(client, user_dir);
        }

        else if (strncmp(buffer, "DELETE", 6) == 0)
        {

            if (send(client, "Delete command received", 23, 0) < 0)
            {
                perror("Error sending delete command message");
                exit(EXIT_FAILURE);
            }
            char file_name[64];

            memset(file_name, '\0', sizeof(file_name));
            if (recv(client, file_name, sizeof(file_name), 0) < 0)
            {
                perror("Error receiving file name");
                exit(EXIT_FAILURE);
            }
            printf("File name: %s\n", file_name);
            fflush(stdout);
            req->client_socket = client;
            req->iscomplete = 1;
            req->uid = c.uid;
            req->storage = c.storage;
            strncpy(req->filename, file_name, sizeof(file_name));
            strncpy(req->command, "DELETE", 6);
            
            // DELETE_command(client, user_dir, &c.storage);
        }

        else if (strncmp(buffer, "END", 3) == 0) 
        {
            req->client_socket = client;
            req->iscomplete = 1;
            req->uid = c.uid;
            req->storage = c.storage;
            strncpy(req->filename, "\0", 1);
            strncpy(req->command, "END", 3);
            enqueue(&request_queue, req);
            while(req->iscomplete == 0);
            free(req);
            break;
        }

        enqueue(&request_queue, req);
        printf("Going for next command1\n");

        while(req->iscomplete == 1)
        {
            printf("FUCK1!! req val %d\n", req->iscomplete);


            pthread_cond_wait(&req_cond, &req_mutex);
            printf("FUCK2!! req val %d\n", req->iscomplete);
            break;
        }
        printf("Going for next command2\n");

        free(req);
        printf("Going for next command\n");
        my_free(ptr2);
        my_free(ptr4);
        
        printf("\nFreed: %p, %p\n", ptr2, ptr4);

        
        printf("\nState of bins after freeing some blocks:\n");
        print_bins();

    }
}

void *thread_client(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    int client = args->client_socket;
    int i = args->index;
    if (send(client, "Server connected successfully", 29, 0) < 0)
    {
        perror("Error receiving");
        exit(EXIT_FAILURE);
    }

    printf("Server connected successfully\n");
    fflush(stdout);
    handle_client(client);
    pthread_mutex_lock(&client_mutex);
    client_arr[i] = 0;
    client_count--;
    pthread_mutex_unlock(&client_mutex);
    close(client);

}



void *client_handler(void *arg) 
{

    Queue *q = (Queue *)arg;
    init_file_list(&file_list);
    init_file_list(&st_list);

    while (1) 
    {
        if(isEmpty(q))
        {
            printf("Empty queue\n");
            while(isEmpty(q));
        }

        printf("Queue size in handler %d\n", get_queue_size(q));

        Request *req = dequeue(q);
        printf("Dequed %d user requests\n", req->uid);
        if (!req) 
        {
            continue; 
        }

        if(is_file_in_use(&st_list, req->uid))
        {
            printf("User in storage if in list in handler is currently in use. UID=%d is waiting...\n", req->uid);
            FileNode *res = remove_file_from_list(&st_list, req->filename, req->uid);
            req->storage = res->storage;
            free(res);
            add_file_to_list(&st_list, req->filename, req->uid, req->storage);
        }

        
        printf("User not in storage if in list in handler is currently in use. UID=%d is waiting...\n", req->uid);

        if (req->filename[0] != '\0' && is_file_in_use(&file_list, req->uid)) 
        {
            printf("File '%s' is currently in use. UID=%d is waiting...\n", 
                   req->filename, req->uid);
              
            enqueue(q, req);
            free(req); 
            sleep(1);  
            continue;
        }
        add_file_to_list(&file_list, req->filename, req->uid, req->storage);

        printf("Enque user %d\n", req->uid);
        enqueue(&working_queue, req);

        // remove_file_from_list(&file_list, req->filename, req->uid);

        free(req);
    }

    return NULL;
}

void proccessor(Request *req)
{
    // pthread_mutex_lock(&req_mutex);
    // Process the request here
    printf("Processing %s request for with UID=%d\n", req->command, req->uid);

    if (strncmp(req->command, "UPLOAD", 6) == 0)
    {
        UPLOAD_command(req->client_socket, req->user_dir, req->filename, &req->storage);
    }
    if (strncmp(req->command, "DOWNLOAD", 8) == 0)
    {
        DOWNLOAD_command(req->client_socket, req->user_dir, req->filename);
    }
    if (strncmp(req->command, "LIST", 4) == 0)
    {
        LIST_command(req->client_socket, req->user_dir);
    }
    if (strncmp(req->command, "DELETE", 6) == 0)
    {
        DELETE_command(req->client_socket, req->user_dir, req->filename, &req->storage);
    }
    if (strncmp(req->command, "END", 3) == 0)
    {
        END_command(req->client_socket, req->uid, &req->storage);
    }
    
    sleep(1);

    printf("FUCK2!! req val %d\n", req->iscomplete);

    
    req->iscomplete = 0;
    printf("FUCK2!! req val %d\n", req->iscomplete);

    // Notify waiting threads
    pthread_cond_signal(&req_cond);

    // pthread_mutex_unlock(&req_mutex);
    printf("Finished %s request for file with UID=%d\n", req->command, req->uid);
}

void *worker(void *arg)
{
    Queue *q = (Queue *)arg;

    while (1) 
    {
        if(isEmpty(q))
        {
            printf("Empty queue in Processor\n");
            while (isEmpty(q));
        }

        Request *req = dequeue(q);
        if (!req) 
        {
            continue; 
        }

        printf("Entering proccessor for %s request for with UID=%d\n", req->command, req->uid);

        proccessor(req);

        printf("Exiting proccessor for %s request for with UID=%d\n", req->command, req->uid);

        remove_file_from_list(&file_list, req->filename, req->uid);
        if(is_file_in_use(&st_list, req->uid))
        {
            printf("User %d already exists in case of storage in Proccessor\n", req->uid);
            FileNode *res = remove_file_from_list(&st_list, req->filename, req->uid);
            free(res);
        }
        add_file_to_list(&st_list, req->filename, req->uid, req->storage);
        printf("User %d dosent already exists in case of storage in Proccessor\n", req->uid);

        free(req);
    }

    return NULL;
}



int main(int argc, char **argv)
{
    int server, client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clilen = sizeof(client_addr);
    int opt = 1;

    init_queue(&request_queue);
    init_queue(&working_queue);

    pthread_t handler_thread;

    // Start the queue handler thread
    if (pthread_create(&handler_thread, NULL, client_handler, (void *)&request_queue) != 0)
    {
        perror("Error creating queue handler thread");
        exit(EXIT_FAILURE);
    }

    pthread_t worker_thread;

    // Start the queue handler thread
    if (pthread_create(&worker_thread, NULL, worker, (void *)&working_queue) != 0)
    {
        perror("Error creating queue handler thread");
        exit(EXIT_FAILURE);
    }

    memset(client_arr, 0, sizeof(client_arr));

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

    if (listen(server, 5) < 0)
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

    while(1)
    {   
        client = accept(server, (struct sockaddr *)&client_addr, &clilen);
        if (client < 0)
        {
            perror("Error accepting");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&client_mutex);
        if(client_count == MAX_CLIENTS - 1)
        {
            insert_queue(client);
        }
        else
        {
            ThreadArgs *new_client = malloc(sizeof(ThreadArgs));

            new_client->client_socket = client;
            int i = find_empty_slot();
            new_client->index = i;
            pthread_create(&client_threads[i], NULL, thread_client, new_client);
        }
        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if(client_arr[i] == 0)
            {
                pthread_join(client_threads[i], NULL);
                if(!is_queue_empty())
                {
                    client_arr[i] = 1;
                    client_count++;
                    int dq_client = pop_queue();
                    ThreadArgs *new_client = malloc(sizeof(ThreadArgs));
                    new_client->client_socket = dq_client;
                    new_client->index = i;
                    pthread_create(&client_threads[i], NULL, thread_client, new_client);
                }
            }
        }
        pthread_mutex_unlock(&client_mutex);
        

        
    }

    pthread_cancel(handler_thread);
    pthread_join(handler_thread, NULL);

    destroy_queue(&request_queue);

    close(server);
    close(client);

    return 0;
}