#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>


#define PORT 8080

void error(const char *msg) 
{
    perror(msg);
    exit(1);
}


int encrypt(const char *a, char* output)
{
    //static char encrypted[500];

    char ch = ' ';
    char pre = ' ';
    int count = 0;
    int size = strlen(a);
    int index = 0;

    for (int i = 0; i < size; i++)
    {
        ch = a[i];
        if (ch == pre)
        {
            count++;
        }
        else
        {
            if (count > 0)
            {
                output[index++] = pre;
                output[index++] = '0' + count;
            }
            count = 1;
        }
        pre = ch;
    }
    return index;
   
}

int decryption(const char *a, char* output)
{
    
    int size = strlen(a);
    int index = 0;

    char ch = ' ';
    for (int i = 0; i < size; i += 2)
    {
        ch = a[i];
        for (int j = 0; j < a[i + 1] - '0'; j++)
        {
            output[index++] = ch;
        }
    }

    return index;
}

void UPLOAD_command(int sockfd, const char *buffer)
{
    char command[7];
    char file_path[246];
    char file_name[20];
    char recive[255];

    // Sending command type to server
    strncpy(command, "UPLOAD", 6);
    if(write(sockfd, command, 6) < 0)
        error("Error in sending upload command to client");

    bzero(recive, sizeof(recive));

    if(read (sockfd, recive, 255) < 0)
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);

    bzero(recive, sizeof(recive));

    // Extracting file path
    strncpy(file_path, buffer + 8, strlen(buffer) - 9);
    file_path[strlen(buffer) - 9] = '\0';
    
    // Going to file location if it
    struct stat bufer;
    if (stat(file_path, &bufer) == 0) 
    {
        if(write(sockfd, "Valid", 5) < 0)
            error("ERROR writing to socket");
        // Geting filename
        const char *filename_ptr = strrchr(file_path, '/');
        if (filename_ptr) 
            strcpy(file_name, filename_ptr + 1);  
        else 
            strcpy(file_name, file_path);  

        printf("File Path: %s\n", file_path);
        printf("File Name: %s\n", file_name);

        if (write(sockfd, file_name, 20) < 0) 
            error("ERROR writing to socket");
        
        if (read(sockfd, recive, 21) < 0)
            error("ERROR reading from socket");

        printf("Server Response: %s\n", recive);
        bzero(recive, sizeof(recive));

        if (read(sockfd, recive, 255) < 0)
            error("ERROR reading from socket");

        if(strncmp(recive, "Replace", 7) == 0)
        {
            char rp;
            printf("File already exists. Do you want to replace it (y/n)?");
            scanf(" %c", &rp);
            if(rp == 'y')
            {
                if(write(sockfd, "yes", 3) < 0)
                    error("ERROR sending replace command to server");

                bzero(recive, sizeof(recive));

                if (read(sockfd, recive, 255) < 0)
                    error("ERROR reading from socket");

                printf("Server Response: %s\n", recive);
            }
            else
            {
                if(write(sockfd, "no", 2) < 0)
                    error("ERROR sending no command to server");

                bzero(recive, sizeof(recive));

                if (read(sockfd, recive, 255) < 0)
                    error("ERROR reading from socket");

                printf("Server Response: %s\n", recive);
                return;
            }
        }

        off_t file_size = bufer.st_size;  // File size retrieved using stat
        printf("File size: %ld bytes\n", file_size);

        if (write(sockfd, &file_size, sizeof(file_size)) < 0)
            error("ERROR sending file size to server");

        bzero(recive, sizeof(recive));

        if (read(sockfd, recive, sizeof(recive)) < 0)
            error("ERROR reading from socket after sending size");

        bzero(recive, sizeof(recive));

        if(write(sockfd, "Sending Content", 15) < 0)
            error("ERROR sending sending content to server");


        FILE *file = fopen(file_path, "rb");
        if (file == NULL) 
            error("ERROR opening file");

        bzero(recive, sizeof(recive));

        if(read(sockfd, recive, sizeof(recive)) < 0)
            perror("ERROR reading from socket after sending");

        fseek(file, 0L, SEEK_END);
        int size = ftell(file);  
        rewind(file);

        if(write(sockfd, &size, sizeof(size)) < 0)
            perror("ERROR writing to socket");

        bzero(recive, sizeof(recive));

        if(read(sockfd, recive, sizeof(recive)) < 0)
            perror("ERROR reading from socket after sending");

        if(strncmp(recive, "OK", 2) != 0)
        {
            printf("Server Response: Storage Not Available\n");
            return;
        }

        printf("Server Response: Storage Available\n");

        char file_buffer[1024];
        size_t bytes_read;
        char encode[2048];

        // Reading and sending file content to server
        while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
            printf("Bytes read: %ld\n", bytes_read);
            bzero(encode, sizeof(encode));
            int sz = encrypt(file_buffer, encode);
            printf("Size %d\n", sz);
            if (write(sockfd, encode, sz) < 0)
                error("ERROR writing file content to socket");
            if(read(sockfd, recive, 255) < 0)
                error("ERROR sending to server");
            bzero(recive, sizeof(recive));
            //printf("OK\n");
        }

        if(write(sockfd, "eof", 3) < 0)
            error("ERROR sending EOF to server");

        fclose(file);

        if(read(sockfd, recive, 23) < 0)
            error("ERROR reading from socket");

        // printf("Server Response: %s\n", recive);
        bzero(recive, sizeof(recive));

        if(read(sockfd, recive, 26) < 0)
            error("ERROR reading from socket");

        printf("Server Response: %s\n", recive);
    }   
    else 
    {
        if(write(sockfd, "Invalid", 7) < 0)
            error("ERROR writing to socket");
        printf("File does not exist at the given path: %s\n", file_path);     
    }  

}

void DOWNLOAD_command(int sockfd, const char *buffer)
{
    char command[8];
    char file_name[20];
    char recive[1024];

    // Sending command type to server
    strncpy(command, "DOWNLOAD", 8);
    if (write(sockfd, command, 8) < 0)
        error("Error in sending download command to client");

    if (read(sockfd, recive, sizeof(recive)) < 0) 
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);

    bzero(recive, sizeof(recive));

    // Extracting file name
    strncpy(file_name, buffer + 10, strlen(buffer) - 11);
    file_name[strlen(buffer) - 11] = '\0';

    // Sending file name
    if (write(sockfd, file_name, 20) < 0) 
        error("ERROR writing to socket");

    if (read(sockfd, recive, sizeof(recive)) < 0) 
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);

    bzero(recive, sizeof(recive));

    if (read(sockfd, recive, sizeof(recive)) < 0) 
        error("ERROR reading from socket");

    // Check if file exists on server
    if (strncmp(recive, "exist", 5) != 0) {
        printf("Server Response: File of such name does not exist\n");
        return;
    }

    printf("Server Response: File of such name exists\n");

    // Creating the full path to the Downloads directory
    char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        error("ERROR getting home directory");

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/Downloads/%s", home_dir, file_name);

    // Opening the file in Downloads directory
    // FILE *file = fopen(full_path, "wb");
    // if (file == NULL) 
    //     error("ERROR opening file in Downloads directory");

    FILE *file = fopen(file_path, "wb");
    if (file == NULL) 
        error("ERROR opening file");
    
    printf("File opened\n");;


    // Receive and write the file content
    bzero(recive, sizeof(recive));
    printf("Going to read\n");

    char decode[2048];
    char file_reader[100];
    int n = 0;
    write(sockfd, "BOK", 3);
    bzero(file_reader, sizeof(file_reader));
    while ((n = read(sockfd, file_reader, 100)) > 0) 
    {
         bzero(decode, sizeof(decode));
        // run_length_decode(file_reader, decode);

        int sz = decryption(file_reader, decode);
        printf("Reading Total Bytes: %d\n", n);
        if (strncmp(file_reader, "eof", 3) == 0)
            break;
        printf("Writing\n");
        if (fwrite(decode, sz, 1, file) < 0)
            error("ERROR writing to file");
        printf("Sending\n");
        if(write(sockfd, "ok", 2) < 0)
            error("ERROR sending ok to server");
        printf("ok\n");
        bzero(recive, sizeof(recive));
        n = 0;
    }

    if (n < 0)
        error("ERROR reading file content from socket");

    fclose(file);

    if(write(sockfd, "File read successfully", 22) < 0)
        error("ERROR sending EOF to server");

    if(read(sockfd, recive, 22) < 0)
    error("ERROR reading from socket");

    printf("File %s successfully downloaded to %s\n", file_name, file_path);
}

void LIST_command(int sockfd)
{
    char command[7];
    char recive[1024];
    strcpy(command, "LIST");

    if (write(sockfd, command, 7) < 0) 
        error("ERROR writing to socket");

    bzero(recive, sizeof(recive));

    if(read(sockfd, recive, sizeof(recive)) < 0)
        error("ERROR reading from socket");

    printf("Server response: %s\n", recive);

    bzero(recive, sizeof(recive));

    if (write(sockfd, command, 7) < 0) 
        error("ERROR writing to socket");
        
    bzero(recive, sizeof(recive));

    if(read(sockfd, recive, sizeof(recive)) < 0)
        error("ERROR reading from socket");

    if(strncmp(recive, "No", 2) == 0)
    {
        printf("Server Response: Directory could'nt open try again later!!\n");
        return;
    }
    bzero(recive, sizeof(recive));
    if(write(sockfd, "Ready", 5) < 0)
        error("ERROR sending ready to server");

     printf("Files on server:\n");
    int n;
    while ((n = read(sockfd, recive, sizeof(recive) - 1)) > 0) {
        recive[n] = '\0'; // Null-terminate the string
        if (strncmp(recive, "EOF", 3) == 0)
            break;
        printf("%s\n", recive);
        bzero(recive, sizeof(recive)); // Clear the buffer for the next read
        if(write(sockfd, "Recived", 7) < 0)
            error("ERROR sending ready to server");
    }

    if (n < 0) 
        error("ERROR reading from socket");

    if(write(sockfd, "Recived All Files", 17) < 0)
        error("ERROR sending ready to server");
}

void DELETE_command(int sockfd, const char *buffer) 
{
    char file_name[20];
    char recive[256];
    strncpy(file_name, buffer + 8, strlen(buffer) - 9);
    file_name[strlen(buffer) - 9] = '\0';

    char command[10];
    strcpy(command, "DELETE ");

    if (write(sockfd, command, strlen(command)) < 0)
        error("ERROR sending DELETE command to server");

    bzero(recive, sizeof(recive));

    if (read(sockfd, recive, sizeof(recive)) < 0)
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);

    bzero(recive, sizeof(recive));

    if (write(sockfd, file_name, strlen(file_name)) < 0)
        error("ERROR writing to socket");

    if (read(sockfd, recive, sizeof(recive)) < 0)
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);

    bzero(recive, sizeof(recive));

    if (read(sockfd, recive, sizeof(recive)) < 0)
        error("ERROR reading from socket");

    printf("Server Response: %s\n", recive);
}

void handle_command(int sockfd) {

    char buffer[256];
    while(1)
    {
        bzero(buffer, sizeof(buffer));

        // Taking the command
        fgets(buffer, 255, stdin);

         // Upload command
        if (strncmp(buffer, "$UPLOAD$", 8) == 0)    
            UPLOAD_command(sockfd, buffer);
        
        // Download command
        if(strncmp(buffer, "$DOWNLOAD$", 8) == 0)
            DOWNLOAD_command(sockfd, buffer);

        // List command
        if(strncmp(buffer, "$LIST", 5) == 0)
            LIST_command(sockfd);

        // Delete command
        if(strncmp(buffer, "$DELETE$", 8) == 0)
            DELETE_command(sockfd, buffer);

        // Storage command
        if(strncmp(buffer, "$STORAGE", 8) == 0)
        {
            if (write(sockfd, "STORAGE", 7) < 0)
                error("ERROR sending STORAGE to server");
            int storage;
            if(read(sockfd, &storage, sizeof(storage)) < 0)
                error("ERROR reading from socket");
            printf("Remaing Storage: %d Bytes\n", storage);
        }

        // Help command
        if(strncmp(buffer, "$HELP", 5) == 0)
            printf("Available commands:\n$UPLOAD$(file path): To upload file to server\n$DOWNLOAD$(filename): To download file from server\n$LIST: To veiw all files\n$DELETE$(filename): To delete the file\n$STORAGE: To see remaing storage avalible\n$HELP: To see the avalible commands\n$END: To end the communication\n");
        
        // End command
        if(strncmp(buffer, "$END", 4) == 0) 
        {
            char command[10];
            strcpy(command, "END");
            if (write(sockfd, command, strlen(command)) < 0)
                error("ERROR sending END to server");

            break;
        }
    }

   
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[256];

    // Creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // Setup server address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) 
        error("ERROR invalid address");

    // Connecting to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    if (read(sockfd, buffer, sizeof(buffer)) < 0) 
        error("ERROR reading from socket");

    printf("Server Response: %s\n", buffer);

    char auth;
    char username[50];
    char password[50];
    char u_and_p[100] = {};

    while(1)
    {
        printf("You want to Signup(S)/Login(L): ");
        scanf(" %c", &auth);
        if(auth == 'S' || auth =='s')
        {
            if(write(sockfd, "signup", 6) < 0)
                error("Error in sending signup");

            bzero(buffer, sizeof(buffer));
            if(read(sockfd, buffer, sizeof(buffer)) < 0)
                perror("Error in reading server response");
            printf("Server says: %s\n", buffer);

            bzero(u_and_p, sizeof(u_and_p));
            printf("Enter your username$password: ");
            fflush(stdout);
            scanf("%s", u_and_p);
            size_t len = strlen(u_and_p);
            u_and_p[len] = '\0';
            if(write(sockfd, u_and_p, sizeof(u_and_p)) < 0)
                error("Error in sending username");

            bzero(buffer, sizeof(buffer));
            if(read(sockfd, buffer, sizeof(buffer)) < 0)
                perror("Error in reading server response");
            if(strncmp(buffer, "NE", 2) != 0)
            {
                printf("Server says: %s\n", buffer);
                continue;
            }
            printf("Server says: %s\n", buffer);

            if(strcmp(buffer, "E") == 0)
            {
                printf("Username already exists. Please try again.");
                continue;
            }
            char* token = strtok(u_and_p, "$");
            if(token != NULL)
                strcpy(username, token);

            printf("Server Response: Signup successfull\n");
            printf("Welcome %s\n", username);
        }
        else if (auth == 'L' || auth == 'l')
        {
            // Send the "login" command to the server
            if (write(sockfd, "login", 5) < 0)
                error("Error in sending login");

            bzero(buffer, sizeof(buffer));
            
            // Read server's response after sending "login"
            if (read(sockfd, buffer, sizeof(buffer)) < 0)
                perror("Error in reading server response");
            printf("Server says: %s\n", buffer);

            // Input username and password in the format username$password
            bzero(u_and_p, sizeof(u_and_p));
            printf("Enter your username$password: ");
            fflush(stdout);
            scanf("%s", u_and_p);

            size_t len = strlen(u_and_p);
            u_and_p[len] = '\0';  // Ensure the string is null-terminated

            // Send the username and password to the server
            if (write(sockfd, u_and_p, sizeof(u_and_p)) < 0)
                error("Error in sending username and password");

            // bzero(buffer, sizeof(buffer));

            // // Read the server's response for verification (ok2 or error)
            // if (read(sockfd, buffer, sizeof(buffer)) < 0)
            //     perror("Error in reading server response");

            // printf("Server says: %s\n", buffer);

            // if(write(sockfd, "help", 4) < 0)
            // {
            //     error("Error in sending help");
            // }
            

            bzero(buffer, sizeof(buffer));

            // Read the server's response for verification (ok3 or error)
            if (read(sockfd, buffer, sizeof(buffer)) < 0)
                perror("Error in reading server response");
            
            if (strncmp(buffer, "Login successful", 16) != 0)
            {
                printf("Server says: %s\n", buffer);
                printf("ggggg\n");
                continue;
            }
            printf("Server says: %s\n", buffer);

            // if(write(sockfd, buffer, sizeof(buffer)) < 0)
            // {
            //     error("Error in sending help");
            // }

            // bzero(buffer, sizeof(buffer));

            // // Check the final response for login success or failure
            // if (read(sockfd, buffer, sizeof(buffer)) < 0)
            //     perror("Error in reading server response");

            if (strcmp(buffer, "Login successful") == 0)
            {
                printf("Login successful. Welcome, %s!\n", u_and_p);

            }
            else if (strcmp(buffer, "Login failed") == 0)
            {
                printf("Login failed. Invalid username or password.\n");
                continue;
            }
        }

        break;
    }

    // // Taking username
    // char username[50];
    // bzero(&username, sizeof(username));
    // printf("Enter your username: ");
    // fgets(username, 50, stdin);
    // if(write(sockfd, username, sizeof(username)) < 0)
    //     error("Error in sending username");

    // bzero(buffer, sizeof(buffer));
    // if(read(sockfd, buffer, sizeof(buffer)) < 0)
    //     error("Error in reading server response");

    // if(strncmp(buffer, "new", 3) == 0)
    //     printf("Welcome %s\n", username);
    // else
    //     printf("Welcome back %s\n", username);

    printf("Type $HELP to see the available commands\n");

    // Process the command
    handle_command(sockfd);

    // Closing the socket
    close(sockfd);

    return 0;
}
