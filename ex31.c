#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define IDENTICAL 3
#define SIMILAR 2
#define DIFFERENT 1
#define ERROR -1
#define STDERR 2
#define BUFFERSIZE 20

//function declarations
int openFile(int *fileDescriptor, char *path);
void closeFile(int fileDescriptor);
int readBytesFromFile(int *readBytesFile, int fileDescriptor, char bufferFile[BUFFERSIZE], int *count);
int keepReadingFile(char *bufferFile, int status, int *readBytesFile, int *count, int fileDescriptor,
                    int otherFileDescriptor);

/**
 * main function
 * reads to each buffer and compares char by char, if there is some kind of space we skip it and mark the status SIMILAR.
 * same mark(SIMILAR) for different case of letters. completely different letters change the status to DIFFERENT.
 * we keep reading into buffers that ended until the files end. or until the status changes to DIFFERENT and we return.
 */

int sain(int argc, char **argv) {
    char bufferFile1[BUFFERSIZE],bufferFile2[BUFFERSIZE];
    int status=IDENTICAL;
    int readBytesFile1, readBytesFile2, fileDescriptor1, fileDescriptor2;
    int i=0,j=0;

    if (argc < 3){
        printf("Not enough arguments\n");
        return ERROR;
    }
    else{
        if(openFile(&fileDescriptor1, argv[1])==ERROR){
            return ERROR;
        }
        else if(openFile(&fileDescriptor2, argv[2])==ERROR){
            closeFile(fileDescriptor1);
            return ERROR;
        }
        //both files opened successfully
        else{
            if(readBytesFromFile(&readBytesFile1,fileDescriptor1,bufferFile1,&i)==ERROR){
                closeFile(fileDescriptor2);
                return ERROR;
            }
            else if(readBytesFromFile(&readBytesFile2,fileDescriptor2,bufferFile2,&j)==ERROR){
                closeFile(fileDescriptor1);
                return ERROR;
            }
            //both files read into buffer successfully
            else{
                //no file has ended
                while (readBytesFile1 > 0 && readBytesFile2 > 0){
                    //no buffer has reached its end
                    while (bufferFile1[i] != '\0' && bufferFile2[j] != '\0'){
                        if (bufferFile1[i] == bufferFile2[j]){
                            i++;
                            j++;
                        }
                        else if (tolower(bufferFile1[i]) == tolower(bufferFile2[j])){
                            status=SIMILAR;
                            i++;
                            j++;
                        }
                        else if (bufferFile1[i]=='\n' || bufferFile1[i]==' '){
                            status=SIMILAR;
                            i++;
                        }
                        else if(bufferFile2[j]=='\n' || bufferFile2[j]==' '){
                            status=SIMILAR;
                            j++;
                        }
                        else {
                            status=DIFFERENT;
                            closeFile(fileDescriptor1);
                            closeFile(fileDescriptor2);
                            return status;
                        }
                    }
                    //one or two buffers had reached the end
                    if(bufferFile1[i] == '\0'){
                        if(readBytesFromFile(&readBytesFile1,fileDescriptor1,bufferFile1,&i)==ERROR){
                            closeFile(fileDescriptor2);
                            return ERROR;
                        }
                    }
                    if(bufferFile2[j] == '\0'){
                        if(readBytesFromFile(&readBytesFile2,fileDescriptor2,bufferFile2,&j)==ERROR){
                            closeFile(fileDescriptor1);
                            return ERROR;
                        }
                    }
                }
                //one or two files had reached the end
                //both ended
                if(readBytesFile1 == 0 && readBytesFile2 == 0){
                    return status;
                }
                //file 1 ended, keep reading file 2
                else if(readBytesFile1 == 0){
                    return keepReadingFile(bufferFile2, status, &readBytesFile2, &j, fileDescriptor1, fileDescriptor2);
                }
                //file 2 ended, keep reading file 1
                else{
                    return keepReadingFile(bufferFile1, status, &readBytesFile1, &i, fileDescriptor2, fileDescriptor1);
                }
            }
        }
    }
}

/**
 * opens the file in the given path
 * @param fileDescriptor
 * @param path
 * @return success (0) or fail (ERROR)
 */
int openFile(int *fileDescriptor, char *path){
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
void closeFile(int fileDescriptor) {
    if(close(fileDescriptor) < 0){
        printf("Error while closing the file\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
    }
}

/**
 * reads bytes from the given file into the given buffer, handles error
 * @param readBytesFile
 * @param fileDescriptor
 * @param bufferFile
 * @param count
 * @return success (0) or fail (ERROR)
 */
int readBytesFromFile(int *readBytesFile, int fileDescriptor, char bufferFile[BUFFERSIZE], int *count){
    *readBytesFile=read(fileDescriptor, bufferFile, BUFFERSIZE-1);
    if (readBytesFile < 0){
        printf("Error while reading from file\n");
        char *error="Error in system call";
        write(STDERR, error, sizeof(error));
        closeFile(fileDescriptor);
        return ERROR;
    }
    bufferFile[*readBytesFile]='\0';
    *count=0;
    return 0;
}

/**
 * reads from the file into the buffer each time until the file ends
 * @param bufferFile
 * @param status
 * @param readBytesFile
 * @param count
 * @param fileDescriptor
 * @param otherFileDescriptor
 * @return ERROR if an error occur, otherwise the status
 */
int keepReadingFile(char *bufferFile, int status, int *readBytesFile, int *count, int fileDescriptor,
                    int otherFileDescriptor) {
    closeFile(fileDescriptor);
    status=SIMILAR;
    while((*readBytesFile) > 0){
        while (bufferFile[(*count)] != '\0'){
            if(bufferFile[(*count)] == '\n' || bufferFile[(*count)] == ' '){
                (*count)++;
            }
            else{
                status=DIFFERENT;
                closeFile(otherFileDescriptor);
                return status;
            }
        }
        if(readBytesFromFile(readBytesFile, otherFileDescriptor, bufferFile, count) == ERROR){
            return ERROR;
        }
    }
    closeFile(otherFileDescriptor);
    return status;
}
