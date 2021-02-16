#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <wait.h>

#define ERROR -1
#define STDERR 2
#define PATH_SIZE 160
#define CONFIG_LINES 3
#define BUFFER_SIZE CONFIG_LINES*PATH_SIZE
#define cFileFound 5
#define otherFileFound 6
#define dirFound 7
#define TRUE 8
#define FALSE 9

//function declarations
int openingFile(int *fileDescriptor, char *path);
void closingFile(int fileDescriptor);
int readFromFile(int *readBytesFile, int fileDescriptor, char bufferFile[BUFFER_SIZE+1]);
int openingDir(DIR **dip, char *path);
void closingDir(DIR *dip);
int handleDirectory(char subDirPath[PATH_SIZE], char *subDirName,int fd,char *inputPath,char *correctOutputPath);
void handleCFile(char* path,char *subDirName,int fd,char *inputPath,char *correctOutputPath);
int returnType(struct dirent *dir, char path[PATH_SIZE]);
int tryToCompile(char *path);
void writeToCSV(const char *subDirName, int fd, const char *resultString, const char *resultScore);
int compareFiles(char *correctOutputPath);

/*
 * main function
 */
int main(int argc, char **argv){
    int fdConfig, readBytes;
    char bufferConfig[BUFFER_SIZE+1], subDirPath[PATH_SIZE];
    char directoryPath[PATH_SIZE], inputPath[PATH_SIZE], correctOutputPath[PATH_SIZE];

    if (argc < 2){
        printf("Not enough arguments\n");
        return ERROR;
    }
    else if(openingFile(&fdConfig, argv[1])==ERROR){
        return ERROR;
    }
    else{
        if(readFromFile(&readBytes,fdConfig,bufferConfig)==ERROR){
            return ERROR;
        }
        else{
            closingFile(fdConfig);

            strcpy(directoryPath, strtok(bufferConfig, "\n"));
            strcpy(inputPath, strtok(NULL, "\n"));
            strcpy(correctOutputPath, strtok(NULL, "\n"));

            printf("%s\n", directoryPath);
            printf("%s\n", inputPath);
            printf("%s\n", correctOutputPath);

            DIR *dip;
            struct dirent *subDir;
            if(openingDir(&dip, directoryPath)==ERROR){
                return ERROR;
            }
            else{
                //opening a csv file for the results with permission to write, read and execute
                int fd=open("results.csv",O_CREAT|O_TRUNC|O_WRONLY,S_IRWXU);
                if (fd < 0 ) {
                    printf("Error while opening the results.csv file\n");
                    char *error = "Error in system call";
                    write(STDERR, error, sizeof(error));
                    return ERROR;
                }
                //go throw students directories
                while ((subDir = readdir(dip)) != NULL ) {
                    if(strcmp(subDir->d_name,".")!=0 && strcmp(subDir->d_name,"..")!=0) {
                        printf("\n%s\n", subDir->d_name);
                        strcpy(subDirPath, directoryPath);
                        strcat(subDirPath, "/");
                        strcat(subDirPath, subDir->d_name);
                        int result=handleDirectory(subDirPath,subDir->d_name,fd,inputPath,correctOutputPath);
                        if(result != cFileFound){
                            writeToCSV(subDir->d_name,fd,"NO_C_FILE","0");
                        }
                    }
                }
                unlink("output");
                unlink("cFile");
                closingFile(fd);
                closingDir(dip); //closing the main directory
            }
        }
    }
}

/***
 * handles given directory
 * @param subDirPath
 * @param subDirName
 * @param fd
 * @param inputPath
 * @param correctOutputPath
 * @return if a c file was found or not
 */
int handleDirectory(char subDirPath[PATH_SIZE], char *subDirName,int fd,char *inputPath,char *correctOutputPath){
    DIR *dip;
    struct dirent *dir;

    if(openingDir(&dip, subDirPath)==ERROR){
        return ERROR;
    }
    else {
        while ((dir = readdir(dip)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                printf("%s\n", dir->d_name);
                char path[PATH_SIZE];
                strcpy(path, subDirPath);
                strcat(path, "/");
                strcat(path, dir->d_name);
                int result=returnType(dir, path);
                if(result == cFileFound){
                    handleCFile(path,subDirName,fd,inputPath,correctOutputPath);
                    closingDir(dip);
                    return cFileFound;
                }
                else if(result == dirFound) {
                    if (handleDirectory(path,subDirName,fd,inputPath,correctOutputPath) == cFileFound) {
                        closingDir(dip);
                        return cFileFound;
                    }
                }
            }
        }
        closingDir(dip);
    }
}

/***
 * tells the type of the file/directory
 * @param dir
 * @param path
 * @return c file/otherfile/directory
 */
int returnType(struct dirent *dir, char path[PATH_SIZE]){
    char *extension=strrchr(dir->d_name, '.');
    if (extension!=NULL && strcmp(extension, ".c") == 0) {
        return cFileFound;
    }
    else{
        DIR* directory = opendir(path);
        if(directory != NULL)
        {
            closedir(directory);
            return dirFound;
        }
        if(errno == ENOTDIR)
        {
            return otherFileFound;
        }
    }
}

/***
 * handles the c file of a given directory
 * @param path
 * @param subDirName
 * @param fdCSV
 * @param inputPath
 * @param correctOutputPath
 */
void handleCFile(char* path,char *subDirName,int fdCSV,char *inputPath,char *correctOutputPath) {
    int compiled = tryToCompile(path);
    printf("%d\n",compiled);
    if (compiled == FALSE) {
        writeToCSV(subDirName, fdCSV, "COMPILATION_ERROR", "0");
        return;
    } else if (compiled == TRUE) {
        //run
        int value, status;
        pid_t pid = fork();
        if(pid < 0){
            char *error = "Error in system call";
            write(STDERR, error, sizeof(error));
            return;
        }
        else if (pid == 0) { //in child
            int fdInput, fdOutput;
            if (openingFile(&fdInput, inputPath) == ERROR)
                return;
            unlink("output");
            fdOutput = open("output", O_CREAT|O_TRUNC|O_WRONLY, S_IRWXU);
            if (fdOutput < 0) {
                printf("Error while opening the output file\n");
                char *error = "Error in system call";
                write(STDERR, error, sizeof(error));
                return;
            }
            if(dup2(fdInput, STDIN_FILENO)==ERROR) {//replace standard input with input file
                char *error = "Error in system call";
                write(STDERR, error, sizeof(error));
                return;
            }
            if(dup2(fdOutput, STDOUT_FILENO)==ERROR) {//replace standard output with output file
                char *error = "Error in system call";
                write(STDERR, error, sizeof(error));
                return;
            }
            //close unused file descriptors
            closingFile(fdInput);
            closingFile(fdOutput);

            char *args[] = {"./cFile", NULL};
            value = execvp(args[0], &args[0]);
            if (value == -1) {
                char *error = "Error in system call";
                write(STDERR, error, sizeof(error));
                return;
            }
        } else { //in parent
            sleep(5);
            if (waitpid(pid, &status, WNOHANG) == 0) {
                writeToCSV(subDirName, fdCSV, "TIMEOUT", "0");
                return;
            }else{ //we compare the files
                int score=compareFiles(correctOutputPath);
                printf("%d\n",score);
                switch(score){
                    case 1:{//different
                        writeToCSV(subDirName, fdCSV, "BAD_OUTPUT", "60");
                        return;
                    }
                    case 2:{//similar
                        writeToCSV(subDirName, fdCSV, "SIMILAR_OUTPUT", "80");
                        return;
                    }
                    case 3:{//equal
                        writeToCSV(subDirName, fdCSV, "GREAT_JOB", "100");
                        return;
                    }
                }
            }
        }
    }
}

/***
 * compares the output with the correct output using comp.out from ex31
 * @param correctOutputPath
 * @return the output of the comparison 1/2/3
 */
int compareFiles(char *correctOutputPath) {
    int value,status;
    char *args2[] = {"./comp.out", "output", correctOutputPath, NULL};
    pid_t pid = fork();
    if(pid < 0){
        char *error = "Error in system call";
        write(STDERR, error, sizeof(error));
        return ERROR;
    }
    else if (pid == 0) { //in child
        value = execvp(args2[0], &args2[0]);
        if (value == -1) {
            char *error = "Error in system call";
            write(STDERR, error, sizeof(error));
            return ERROR;
        }
    }
    else{//in parent
        waitpid(pid,&status,0);
        if(WIFEXITED(status))
            return WEXITSTATUS(status);
    }
}

/***
 * writing to the csv file
 * @param subDirName
 * @param fd
 * @param resultString
 * @param resultScore
 */
void writeToCSV(const char *subDirName, int fd, const char *resultString, const char *resultScore) {
    char msg[sizeof(subDirName) + PATH_SIZE]="\0";
    strcpy(msg, subDirName);
    strcat(msg, ",");
    strcat(msg, resultScore);
    strcat(msg, ",");
    strcat(msg, resultString);
    strcat(msg, "\n");
    if(write(fd, msg, sizeof(msg))<0) {
        printf("error writing to csv");
        char *error = "Error in system call";
        write(STDERR, error, sizeof(error));
    }
}

/***
 * compiles and searches for the compiled file
 * @param path
 * @return true if success compiling or false otherwise
 */
int tryToCompile(char *path) {
    unlink("cFile");
    char *args[]={"gcc", "-o", "cFile", path, NULL};
    int value;
    int compiled=FALSE;
    pid_t pid=fork();
    if(pid == 0){ //in child
        value=execvp(args[0],&args[0]);
        if(value == -1) {
            char *error="Error in system call";
            write(STDERR, error, sizeof(error));
            return compiled;
        }
    }
    else { //in parent
        waitpid(pid, NULL, 0);
        //look for the file
        char directory[PATH_SIZE];
        DIR *dip2;
        struct dirent *dir2;

        getcwd(directory, PATH_SIZE);
        if(openingDir(&dip2, directory)==ERROR){
            return ERROR;
        }
        while ((dir2 = readdir(dip2)) != NULL ) {
            if(strcmp(dir2->d_name,"cFile") == 0){
                compiled=TRUE;
                break;
            }
        }
        closingDir(dip2);
        return compiled;
    }
}

/**
 * opens the file in the given path
 * @param fileDescriptor
 * @param path
 * @return success (0) or fail (ERROR)
 */
int openingFile(int *fileDescriptor, char *path){
    *fileDescriptor=open(path, O_RDONLY);
    if (*fileDescriptor < 0 ){
        printf("Error while opening the file\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
        return ERROR;
    }
    return 0;
}

/**
 * closes the file
 * @param fileDescriptor
 */
void closingFile(int fileDescriptor) {
    if(close(fileDescriptor) < 0){
        printf("Error while closing the file\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
    }
}

/**
 * reads bytes from the given file into the given buffer, handles error
 * @param readBytes
 * @param fileDescriptor
 * @param bufferFile
 * @return success (0) or fail (ERROR)
 */
int readFromFile(int *readBytes, int fileDescriptor, char bufferFile[BUFFER_SIZE+1]){
    *readBytes=read(fileDescriptor, bufferFile, BUFFER_SIZE);
    if (readBytes < 0){
        printf("Error while reading from file\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
        closingFile(fileDescriptor);
        return ERROR;
    }
    bufferFile[*readBytes]='\0';
    return 0;
}

/***
 * opens the dir in the given path
 * @param dip
 * @param path
 * @return success (0) or fail (ERROR)
 */
int openingDir(DIR **dip, char *path){
    if((*dip=opendir(path))==NULL){
        printf("Error while opening the directory\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
        return ERROR;
    }
    else{
        return 0;
    }
}

/***
 * closes the file
 * @param dip
 */
void closingDir(DIR *dip) {
    if(closedir(dip)==ERROR){
        printf("Error while closing the directory\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
    }
    return;
}
