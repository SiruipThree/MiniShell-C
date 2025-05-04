#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> //open(),read()
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define MAX_SIZE 1024

// return 0 or 1 to determine what kind of mode 

int determineMode(int argc){
    int isBatchMode;
    if (argc == 1 && isatty(STDIN_FILENO)){
        isBatchMode = 0;
    }
    else{
        isBatchMode = 1; // interactive mode 
    }
    return isBatchMode;
}

//read from the command line 
// char* readLine(int argc, int fd){ //file descripter for fd
//     int modeStatus = determineMode(argc); //which mode for input 
//     char *buffer = malloc(64);
//     char singleCharBuffer = ' '; //single letter for buffer
//     if (modeStatus ==1 ){
//         int counter = 0;
//         while (singleCharBuffer != '\n') //read till reach the end 
//         {
//             int readByte = read(fd, singleCharBuffer, 1);
//             if(readByte)
//             {
//                 buffer[counter] = singleCharBuffer;
//                 counter++;
//                 continue;
//             }
//             else { //didn't read anything
//                 break;
//             }
//         }
//         buffer[counter] = 0; //means the end 
//     }
//     else{
//          int byteRead = read(STDERR_FILENO,buffer,64);    //interactive mode
//          buffer[byteRead] = 0;
//     }
//     return buffer;//the thing that we read from the command line 
// }

// //after batch mode 
// //TDL: determine the num of child process
// int deterNumofChildProcess(char *buffer){
//     for(int i = 0; i < strlen(buffer); i ++ ){
//         if (buffer[i] == '|'){ // | means pipeline, so if we read | means we have pipe line, and we have two children process.
//             return 2;
//         }
//     }
//     return 1;
// }
//TDL: path of executable file 
//TDL: the argument strings:file后的argument（executable file的参数）
//TDL: file for standard input and output 
typedef struct {
    char specialToken; //single char  0 if it's a normal token
    char *normalToken; //it is a string NULL if it is a speical token 
}tokenType;

int tokenizing(char *buffer,tokenType *token){
    int numOfToken =0;
    int isToken =0;
    char *currentStart;
    while(1){
        char curr = *buffer;
        if (curr == 0) {
            break;
        }
        if(isToken == 1){ //determine if it is currently in token 
            //normal token case
            if(curr == ' '){
                if(token != NULL){
                    *buffer = 0;
                    token[numOfToken] = (tokenType){0,currentStart};//normal token
                }
                numOfToken++;
                isToken = 0;
            }
            else if (curr == '|' || curr == '>' || curr == '<'){
                if(token != NULL){
                    *buffer =0;//since the token ends here 
                    token[numOfToken] =(tokenType){0,currentStart};
                    token[numOfToken+1] = (tokenType){curr, NULL}; 
                }
                numOfToken += 2;
                isToken = 0;
            }
        }
        else{//not in token yet 
            if (curr == '|' || curr == '>' || curr == '<' ){
                if(token == NULL){
                    numOfToken++;
                }
                else{
                    token[numOfToken] = (tokenType){curr,NULL};
                    numOfToken++;
                }
            }
            else if (curr != ' '){
                currentStart  = buffer;
                isToken = 1;
            }
        }
        buffer ++;
    }
    if (isToken) {
        if(token != NULL){
            token[numOfToken] = (tokenType){0,currentStart};//normal token
        }
        numOfToken ++;
    }
    return numOfToken;
}

int tokenized (char *buffer, tokenType **token){
    int numOfToken = tokenizing(buffer,NULL);
    tokenType * tokenArray = malloc(numOfToken*sizeof(tokenType));
    tokenizing(buffer, tokenArray);
    *token = tokenArray;
    return numOfToken;
}

int readLine(int fd,char *buffer, char *line, char **start, int *length){
    while (1) {
        //transfer buffer content to line
        while ( *length > 0 && **start !='\n'){
            *line = **start;
            line ++;
            (*start)++;
            (*length)--;
        }
        //check if we have finshed reading a line
        if (*length > 0){
            *line = 0;
            (*start)++;
            (*length)--;//skip\n
            return 1;
        }
        //situation: we already finished the content in buffer, read()
        *start = buffer;
        int status = read(fd, buffer,MAX_SIZE);
        if(status < 0){
            return status;
        }
        if (status ==0){
            *line = 0;
            return 0;// success, do not need to read anymore.
        }
        *length = status;
    }
}

int findCommand(char *programName,char *targetPath){
    char *path = getenv("PATH");
    while (1) {
        //to get:
        char *stringPointer = strchr(path, ':');
        int isLastOne;
        char buffer[512];
        if (stringPointer != NULL){
            sprintf(buffer, "%.*s/%s", (int)(stringPointer - path), path, programName);
            
            isLastOne = 0;
        }
        else{
            sprintf(buffer, "%s/%s",path, programName);
            isLastOne = 1;
        }
        if (access(buffer, F_OK) != -1){
            //which mean we find it
            strcpy(targetPath, buffer);
            return 1;
        }
        if (isLastOne){
            return 0; // didnt find it 
        }
        else{
            path = stringPointer + 1;
        }

    }
}

struct command {
    tokenType *arguments; //命令
    int argc;//num of token
    int fd_in, fd_out;
    pid_t pid;
};

pid_t executeCommand(struct command *cmd, char *path, char *argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        printf("fork failed \n");
        return pid;
    }
    if (pid == 0) {
        // child process
        if (cmd->fd_in != -1) {
            dup2(cmd->fd_in, STDIN_FILENO);
        }
        if (cmd->fd_out != -1) {
            dup2(cmd->fd_out, STDOUT_FILENO);
        }
        exit(execv(path, argv));
    }
    return pid;
}

int handleCommand (struct command *cmd){
    if( cmd->argc == 0){
        return -1;
    }
    //determine if the first token is not speical token which isi |,<,>
    if (cmd->arguments[0].specialToken){
        return -1;
    }
    char path[512];
    if (strchr(cmd->arguments[0].normalToken, '/') == NULL) {
        if (findCommand(cmd->arguments[0].normalToken, path) == 0){
            printf("ERROR: unable to find the program\n");
            return -1;//if we didnt find the program
        }
    } else {
        strcpy(path, cmd->arguments[0].normalToken);
    }
    // otherwise we find path of the program 
    //then we need to determine the redirection and argument
    char *arguments[512];
    int currIndex = 1;
    arguments[0] = malloc(strlen(cmd->arguments[0].normalToken)+1);
    strcpy(arguments[0],cmd->arguments[0].normalToken);
    int flag = 0; // 1 means we meet the speical token
    for (int i = 1; i < cmd->argc; i++){
        if(cmd->arguments[i].specialToken){
            flag = 1;
        }
        if(flag == 0){
            
            char *pointer = strchr(cmd->arguments[i].normalToken,'*');//a function used to search * in char array 
            if (pointer == NULL){
                arguments[currIndex] = malloc(strlen(cmd->arguments[i].normalToken)+1);
                strcpy(arguments[currIndex],cmd->arguments[i].normalToken);
                currIndex++;
            }
            else{
                //wildcard expansion
                char *pointerP = strrchr(cmd->arguments[i].normalToken, '/');
                char parentDir[512], searchDir[512];
                char *pattern;
                int prefixLength; // the chars before the *
                int suffixLength; // after the *
                if(pointerP == NULL){
                    pattern = cmd->arguments[i].normalToken;
                    strcpy(parentDir, "");
                    strcpy(searchDir, ".");
                    prefixLength = pointer - cmd->arguments[i].normalToken;
                }
                else {
                    pattern = pointerP + 1;
                    strncpy(parentDir, cmd->arguments[i].normalToken, pointerP - cmd->arguments[i].normalToken + 1);
                    parentDir[pointerP - cmd->arguments[i].normalToken + 1] = 0;
                    strcpy(searchDir, parentDir);
                    prefixLength = pointer - pointerP - 1;
                }
                suffixLength = cmd->arguments[i].normalToken + strlen(cmd->arguments[i].normalToken) - pointer - 1;
                int parentLength = strlen(parentDir);

                // list directory
                struct dirent *entry;
                DIR *dir = opendir(searchDir);
                if (dir == NULL) {
                    printf("ERROR: Unable to open this directory\n");
                    while (currIndex --) {
                        free(arguments[currIndex]);
                    }
                    return -1;
                }
                
                int matched = 0;
                while ((entry = readdir(dir)) != NULL) {
                    int entryLength = strlen(entry->d_name);
                    if (entryLength < prefixLength + suffixLength) {
                        continue;
                    }
                    if (memcmp(entry->d_name, pattern, prefixLength) == 0 
                        && memcmp(entry->d_name + entryLength - suffixLength, pattern + prefixLength + 1, suffixLength) == 0) {
                        // yo find a right file :)
                        matched = 1;
                        arguments[currIndex] = malloc(parentLength + entryLength + 1);
                        sprintf(arguments[currIndex], "%s%s", parentDir, entry->d_name);
                        currIndex ++;
                    }
                }
                closedir(dir);
                if (matched == 0) {
                    arguments[currIndex] = malloc(strlen(cmd->arguments[i].normalToken)+1);
                    strcpy(arguments[currIndex],cmd->arguments[i].normalToken);
                    currIndex++;
                }
            }
        }
        else {
            // handle the redirection tokens
            if (cmd->arguments[i].specialToken == 0 || i == cmd->argc - 1 || cmd->arguments[i + 1].specialToken != 0) {
                printf("ERROR: This argument is incorrect.\n");
                while (currIndex --) {
                    free(arguments[currIndex]);
                }
                return -1;
            }
            char special = cmd->arguments[i].specialToken;
            int redirectionError = 0;
            if (special == '<') {
                // redirect the input
                int fd = open(cmd->arguments[i + 1].normalToken, O_RDONLY);
                if (fd < 0) {
                    printf("ERROR: unable to open the file\n");
                    fd = -1;
                    redirectionError = 1;
                }
                cmd->fd_in = fd;
            }
            else {
                // redirect the output
                mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                int fd = open(cmd->arguments[i + 1].normalToken, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, mode);
                if (fd < 0) {
                    printf("ERROR: unable to open the file\n");
                    fd = -1;
                    redirectionError = 1;
                }
                cmd->fd_out = fd;
            }
            if (redirectionError == 1) {
                while (currIndex --) {
                    free(arguments[currIndex]);
                }
                return -1;
            }
            i ++;
        }
    }
    arguments[currIndex++] = NULL;
    int pid = executeCommand(cmd, path, arguments);
    while (currIndex --) {
        free(arguments[currIndex]);
    }
    if (pid < 0) {
        return -1;
    }
    cmd->pid = pid;
    return 0;
}

#define EQ(A,B) (strcmp(A, B) == 0)

int isBuiltinCommand(char *cmd, tokenType *tokenArray, int tokenArrSize, int isBatchMode) {
    if (strcmp(cmd, "exit") == 0) {
        if (isBatchMode == 0) {
            printf("Exiting my shell.\n");
        }
        exit(0);
    } else if (strcmp(cmd, "cd") == 0) {
        if (tokenArrSize != 2 || tokenArray[1].normalToken == NULL || chdir(tokenArray[1].normalToken) != 0) {
            printf("ERROR: unable to change directory.\n");
        }
        return 1;
    } else if (strcmp(cmd, "pwd") == 0) {
        char cwd[MAX_SIZE];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s\n", cwd);
        } else {
            printf("ERROR: unable to get current directory.\n");
        }
        return 1;
    }
    else if(strcmp(cmd, "which") == 0){
        if (tokenArrSize != 2||tokenArray[1].specialToken 
            ||EQ("exit", tokenArray[1].normalToken)
            ||EQ("cd", tokenArray[1].normalToken)
            ||EQ("pwd", tokenArray[1].normalToken) 
            ||EQ("which", tokenArray[1].normalToken)){
            return 1;
        }
        char targetPath[512];
        if (findCommand(tokenArray[1].normalToken, targetPath)){
            printf("%s\n",targetPath); // %d int %s char* %f float 
        }
        return 1;
    }
    return 0;
}

void handleLine(char *line, int isBatchMode){
    tokenType *tokenArr;
    int numOfToken = tokenized(line,&tokenArr);
    // for (int i = 0; i < numOfToken; i++) {
    //     if (tokenArr[i].specialToken) {
    //         printf("'%c' ", tokenArr[i].specialToken);
    //     } else {
    //         printf("[%s] ", tokenArr[i].normalToken);
    //     }
    // }
    // printf("\n");
    if (numOfToken == 0){
        free(tokenArr);
        return;
    }
    if (tokenArr[0].specialToken != 0){
        printf("invalid command\n");
        free(tokenArr);
        return;
    }
    if (isBuiltinCommand(tokenArr[0].normalToken, tokenArr, numOfToken, isBatchMode)){
        free(tokenArr);
        return;
    }
    struct command cmdArray[2];
    int cmdCount = 1;
    int pipeFd[2];
    cmdArray [0].arguments = tokenArr;
    cmdArray[0].fd_in = -1; // which mean we dont need to redirect
    cmdArray[0].fd_out = -1;// same above
    cmdArray[0].argc = numOfToken;
    for (int i = 0; i < numOfToken; i++){
        if (tokenArr[i].specialToken == '|'){
            if(cmdCount == 1){
                cmdArray[0].argc = i;
                cmdCount ++;
                cmdArray[1].arguments = tokenArr +i +1;
                cmdArray[1].argc = numOfToken - i - 1;
                if(i + 1 == numOfToken){
                    printf("invalid command\n");
                    free(tokenArr);
                    return;
                }
            }
            else{
                printf("invalid command\n");
                free(tokenArr);
                return;
            }
            
        }
    }

    if(cmdCount != 1) {
        if(pipe(pipeFd) < 0){
            printf("ERROR to create Pipe");
            free(tokenArr);
            return;
        }
        cmdArray[0].fd_out = pipeFd[1];
        cmdArray[1].fd_in = pipeFd[0]; // 
    }

    int cmdStatus[2];
    int validCommand = 1;
    for (int i = 0; i < cmdCount; i++) {
        cmdStatus[i] = handleCommand(cmdArray + i);
        if (cmdStatus[i] != 0) {
            validCommand = 0;
        }
        if (cmdArray[i].fd_in != -1) {
            close(cmdArray[i].fd_in);
        }
        if (cmdArray[i].fd_out != -1) {
            close(cmdArray[i].fd_out);
        }
    }
    int errorCode = 0;
    for (int i = 0; i < cmdCount; i++) {
        if (cmdStatus[i] == 0) {
            int status;
            waitpid(cmdArray[i].pid, &status, 0);
            errorCode = status;
        }
    }
    if (validCommand) {
        if (WIFEXITED(errorCode) && WEXITSTATUS(errorCode) != 0) {
            printf("Command failed with code %d\n", WEXITSTATUS(errorCode));
        } else if (WIFSIGNALED(errorCode)) {
            printf("Terminated by signal %d\n", WTERMSIG(errorCode));
        }
    }
    
    free(tokenArr);
}

//main function
int main(int argc, char* argv[]){ // argc number of argument, argv: content of the argument 
    int isBatchMode = determineMode(argc);
    char inputBuffer[MAX_SIZE];
    char *bufferStart;
    int bufferLength = 0;
    char givenBuffer[MAX_SIZE];
    bufferStart = inputBuffer;
    int fd;
    
    if ( isBatchMode ==1){
        if (argc > 1) {
            fd = open(argv[1],O_RDONLY); //why 1 TBD
        }
        else {
            fd = STDIN_FILENO;
        }
    }
    else{
        printf("Welcome to my shell!\nmysh> ");
        fflush(stdout);
        fd = STDIN_FILENO;
    }

    while(1){
        int status = readLine(fd, inputBuffer,givenBuffer, &bufferStart, &bufferLength);
        if (status < 0){
            printf("ERROR:%d\n",status);
            exit(status);
        }
        handleLine(givenBuffer, isBatchMode);
        if(status == 0){
            break;
        }
        if(isBatchMode == 0){
            printf("mysh> ");
            fflush(stdout);
        }
    }
    return 0;
}