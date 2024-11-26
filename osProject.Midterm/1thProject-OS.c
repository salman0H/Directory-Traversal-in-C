#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_FILES 2000
#define MAX_PATH 1024
#define MAX_FILE_TYPES 100
#define MAX_DUPLICATES 1000

struct FileInfo {
    char name[MAX_PATH];
    off_t size;
};

struct DuplicateFile {
    char path[MAX_PATH];
    ino_t inode;
};

struct DuplicateFile duplicateFiles[MAX_DUPLICATES];
int numDuplicates = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct FileInfo *allFiles;
int *fileCount;
char *fileTypes[MAX_FILE_TYPES];
int fileTypeCounts[MAX_FILE_TYPES] = {0};

// Define a structure for the message
struct MessageType {
    long messageType;  // Message type
    int typeIndex;     // Index of the file type
    int count;         // Count of the file type
};

char *getFileType(char *path) {
    char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

int addFileType(char *fileType) {
    for (int i = 0; i < MAX_FILE_TYPES; i++) {
        if (fileTypes[i] == NULL) {
            fileTypes[i] = strdup(fileType);
            return i;
        } else if (strcmp(fileTypes[i], fileType) == 0) {
            return i;
        }
    }
    return -1;
}

int processFileTypeCounts[MAX_FILE_TYPES] = {0};

void traverseDirectory(char *path, int msgqid, int shmid, int processIndex, pthread_mutex_t *mutex) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char fullPath[MAX_PATH];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(fullPath, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                traverseDirectory(fullPath, msgqid, shmid, processIndex, mutex);
            }
        } else {
            pthread_mutex_lock(mutex);
            char *fileType = getFileType(fullPath);
            int typeIndex = addFileType(fileType);
            if (typeIndex != -1) {
                processFileTypeCounts[typeIndex]++;
            }
            pthread_mutex_unlock(mutex);
        }
    }

    closedir(dir);
}

void calculateDirectorySize(char *path, off_t *size) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char fullPath[MAX_PATH];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(fullPath, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                calculateDirectorySize(fullPath, size);
            }
        } else {
            *size += statbuf.st_size;
        }
    }

    closedir(dir);
}

void handleDuplicateFiles(char *rootDirectory) {
    off_t oldDirSize = 0, newDirSize = 0;
    calculateDirectorySize(rootDirectory, &oldDirSize);

    printf("Duplicate file found & removed: %d\n", numDuplicates);
    for (int i = 0; i < numDuplicates; i++) {
        printf("%s\n", duplicateFiles[i].path);
        remove(duplicateFiles[i].path);
    }

    printf("Path size before removing: %ld KB\n", oldDirSize / 1024);
    calculateDirectorySize(rootDirectory, &newDirSize);
    printf("Path size after removing: %ld KB\n", newDirSize / 1024);
}

void printDuplicateInfo(char *rootDirectory) {
    printf("Directory: %s\n", rootDirectory);

    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(rootDirectory)) == NULL) {
        perror("Error opening directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char filePath[MAX_PATH];
        snprintf(filePath, sizeof(filePath), "%s/%s", rootDirectory, entry->d_name);

        struct stat fileStat;
        if (stat(filePath, &fileStat) == 0) {
            if (S_ISDIR(fileStat.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                int count = 0;
                for (int i = 0; i < numDuplicates; i++) {
                    if (strstr(duplicateFiles[i].path, filePath) != NULL) {
                        count++;
                    }
                }
                if (count > 0) {
                    printf("PID: %d, IID: %d, %d duplicate(s) found in %s\n", getpid(), i, count, filePath);
                }
            }
        }
    }

    closedir(dir);
}

int main() {
    char rootDirectory[MAX_PATH];

    // Read the root directory path from the user
    printf("Enter the root directory path: ");
    scanf("%s", rootDirectory);

    // Shared memory initialization
    int shmid = shmget(IPC_PRIVATE, sizeof(struct FileInfo) * MAX_FILES + sizeof(int) + sizeof(char *) * MAX_FILE_TYPES * MAX_FILES, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    allFiles = (struct FileInfo *)shmat(shmid, NULL, 0);
    fileCount = (int *)(allFiles + MAX_FILES);
    char **fileTypeArray = (char **)(fileCount + 1);

    if (allFiles == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    *fileCount = 0;

    // Create a message queue
    int msgqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msgqid == -1) {
        perror("msgget");
        exit(1);
    }

    traverseDirectory(rootDirectory, msgqid, shmid, 0, &mutex);

    // Detach shared memory
    if (shmdt(allFiles) == -1) {
        perror("shmdt");
        exit(1);
    }

    // Print results
    printf("Total number of files: %d\n", *fileCount);
    printf("Number of each file type:\n");
    for (int i = 0; i < MAX_FILE_TYPES && fileTypes[i] != NULL; i++) {
        printf(".%s: %d\n", fileTypes[i], fileTypeCounts[i]);
    }

    handleDuplicateFiles(rootDirectory);
    printDuplicateInfo(rootDirectory);

    // Remove shared memory and message queue
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    if (msgctl(msgqid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}
