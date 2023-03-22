#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>
//#include <bits/mathcalls.h>
//#include <math.h>
///Lior Zadah - 318162930
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

int isDir(const char *);

bool file_exists(char *filename);

int poolFunc(void *argument);

char *errorHendel(int errNUM, char *path, char *timeBuf, char *errorResponse);

char *get_mime_type(char *name);

char *response(char *path, char *timebuf, char *finalResponse, int contentLen, char *modyTIME);

void dir_response(char *path, char *timebuf, char *finalResponse, long contentLength, char *modificationTime);

int isReadable(char *path);
/*
 * start with saving the port num, poll size and the max request number, then, creat the thread pool
 * and build the socket des, bind and listen just like the presentation.
 * after i call the main func that deal whit each thread.
 */
int main(int argc, char *argv[]) {
    if(argc != 4){ //usage if the amount isn't like we predict
        printf( "Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    }
    int port = (int)strtol(argv[1], NULL, 10);
    int poolSize = (int)strtol(argv[2], NULL, 10);
    int requestMaxNumber = (int)strtol(argv[3], NULL, 10);
    if ((port <= 0 || port > 65536) || (poolSize > 200 || poolSize <= 0) || (requestMaxNumber <= 0)) {
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    }
    //creat threadPool - like presentation
    threadpool *pool = create_threadpool(poolSize);
    if(pool == NULL){
        destroy_threadpool(pool);
        perror("create_threadpool FAIL");
        exit(0);
    }
    int fd;/* socket descriptor */
    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        destroy_threadpool(pool);
        perror("socket Descriptor fail");
        close(fd);
        exit(0);
    }
    struct sockaddr_in srv;    /* used by bind() create the socket *//* bind: use the Internet address family */
    //struct sockaddr_in srv2;    /* used by bind() create the socket *//* bind: use the Internet address family */
    srv.sin_family = PF_INET;
/* bind: socket ‘fd’ to port*/
    srv.sin_port = htons(port);
/* bind: a client may connect to any of my addresses */
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *) &srv, sizeof(srv)) < 0) {
        destroy_threadpool(pool);
        perror("bind fail");
        close(fd);
        exit(0);
    }
    if (listen(fd, 5) < 0) {
        destroy_threadpool(pool);
        perror("listen fail");
        close(fd);
        exit(0);
    }
    struct sockaddr_in cli;
    //int newfd;                  /* returned by accept() */
    int cli_len = sizeof(cli);    /* used by accept() */
/* 1) create the socket 2) bind the socket to a port 3) listen on the socket */
    int soketArr[requestMaxNumber];//array of request
    int indSOCK = 0;
    while (indSOCK < requestMaxNumber) {
        soketArr[indSOCK] = accept(fd, (struct sockaddr *) &cli, (socklen_t *) &cli_len);
        if (soketArr[indSOCK] < 0) {
            destroy_threadpool(pool);
            perror("accept fail");
            close(fd);
            exit(0);
        }
        dispatch(pool, (dispatch_fn)poolFunc, (void *) &soketArr[indSOCK]);
        indSOCK++;//come back here after pool func
    }
    destroy_threadpool(pool);
    close(fd);
}
/*
 * start whit read the request, save the protocol, methood and path,
 * checks them if they are in the rigth format and coninue to check which case we in.
 */
int poolFunc(void *argument) {
    time_t now;
    char timebuf[128], blank[10] = "";
    char finalReturn[5000] = ""; // the final char* that we will print
    memset(finalReturn,'\0',5000);
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    char buf[512];
    memset(buf,'\0',512);
    //memset(initialRequest,'\0',2000);
    int nbytes = 0;/* used by read() */
    int *indSOCK = (int *) argument;
    if ((nbytes = (int)read(*indSOCK, buf, sizeof(buf))) < 0) {
        errorHendel(500,blank, timebuf,finalReturn);
        close(*indSOCK);
        exit(0);
    }
    if(strlen(buf) == 0)
        return 0;
    //take the initial request from the buf we read
    char *initialRequest = strtok(buf, "\r\n");
    if (initialRequest == NULL) {
        printf("initial request is NULL\n");
        close(*indSOCK);
        exit(0);
    }
    strcat(initialRequest,"\0");
    /*
     * Now, using a while loop i'll get the method, path and protocol from the request.
     * check if there was a path and every thing went well, and if something is missing send error 400
     */
    char method[4] ="", path[200]="", protocol[10]="";
    int firstSpace = 0, secondSpace = 0;
    //by using the stat we can get a lot of information on the files. here i take the size&mody date
    int reqInd = 0, metInd = 0, pathInd = 0, proInd = 0, pathFlag = 0, proFlag = 0,metFlag = 0;
    while (reqInd < strlen(initialRequest) && initialRequest[reqInd] != '\0') {
        if (firstSpace == 0 && secondSpace == 0) {
            while (initialRequest[reqInd] != ' ') {
                method[metInd] = initialRequest[reqInd];
                method[++metInd] = '\0';
                reqInd++;
                metFlag = 1;
            }
            firstSpace = 1;
            reqInd++;
        }
        if (firstSpace == 1 && secondSpace == 0) {
            while (initialRequest[reqInd] != ' ') {
                path[pathInd] = initialRequest[reqInd];
                path[++pathInd] = '\0';
                reqInd++;
                pathFlag = 1;
            }
            secondSpace = 1;
            reqInd++;
        } else {//(firstSpace == 1 && secondSpace == 1)
            while (initialRequest[reqInd] != '\0') {
                protocol[proInd] = initialRequest[reqInd];
                protocol[++proInd] = '\0';
                reqInd++;
                proFlag = 1;
            }
            break;
        }
    }
    if((metFlag == 0) || (proFlag == 0) || (pathFlag == 0)){
        errorHendel(400, path, timebuf, finalReturn);
        if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
            errorHendel(500,path, timebuf,finalReturn);
        }
        close(*indSOCK);
        return 0;
    }

    if (strcmp(method, "GET") != 0) {//check if the method is good.
        errorHendel(501, path, timebuf, finalReturn);
        if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
            errorHendel(500,path, timebuf,finalReturn);
            write(*indSOCK, finalReturn, strlen(finalReturn));
        }
        close(*indSOCK);
        return 0;
    }

    if ((strcmp(protocol, "HTTP/1.0") != 0) &&
        (strcmp(protocol, "HTTP/1.1") != 0)) {//check if this is the right protocol.
        errorHendel(400, path, timebuf, finalReturn);
        if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
            errorHendel(500,path, timebuf,finalReturn);
            write(*indSOCK, finalReturn, strlen(finalReturn));
        }
        close(*indSOCK);
        return 0;
    }
    char dirPath[1000] = "";//it will contain the hole path with cwd- the hole path from the top.
    getcwd(dirPath, sizeof (dirPath));//the cwd gives me the full path
    strcat(dirPath, path);
    dirPath[strlen(dirPath)] = '\0';
    struct stat stats;//first definition of stat
    stat(dirPath,&stats);// the first attachment

    if (stat(dirPath, &stats) != 0) {//check if the path is right, if not, send 404
        errorHendel(404, path, timebuf, finalReturn);
        if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
            errorHendel(500,path, timebuf,finalReturn);
            write(*indSOCK, finalReturn, strlen(finalReturn));
        }
        close(*indSOCK);
        return 0;
    }
    //set up the mody time and the file content length so i could send it to the func as argument.
    char modyTIME[128]="";
    strftime(modyTIME, sizeof(modyTIME), RFC1123FMT, localtime(&stats.st_mtime));
    long fileSIZE = stats.st_size;

    //checks if the pate is directory->folder.
    int isDirInt = isDir(dirPath);
    if (isDirInt == 0) {//if the path is directory
        if (path[strlen(path) - 1] != '/') {//5 if it doesn't end with a'/' - 302
            errorHendel(302, path, timebuf, finalReturn);
            if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                errorHendel(500,path, timebuf,finalReturn);
                write(*indSOCK, finalReturn, strlen(finalReturn));
            }
            close(*indSOCK);
            return 0;
        }
        //if it doses end with a'/' - check if the 'index.html' exist. if so, check if its permissions.
        if (path[strlen(path) - 1] == '/') {//6 -
            char semiPath[1000] = "";
            char indexBuf[500] = "";//the buf for the index.html return
            char tablePath[500] = ".";
            //using couple string because i want to save the data couple of times
            strcpy(semiPath, path);
            strcat(tablePath, path);// we want to save the path without index.html
            strcat(semiPath, "index.html");//here to check permission
            strcat(dirPath, "index.html");//here to check if file exist
            if (file_exists(dirPath)) {//if index.html exist - return it
                int res = isReadable(path);//checks if there are good permissions
                if (res == 0) {///indexBuf? what size? maybe alloc? only the size matter
                    struct stat statsInd;//first definition of stat
                    stat(dirPath,&statsInd);//to know the content length
                    response(path, timebuf, finalReturn, (int) statsInd.st_size, modyTIME);
                    if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){//write the header
                        errorHendel(500,path, timebuf,finalReturn);
                        write(*indSOCK, finalReturn, strlen(finalReturn));
                        close(*indSOCK);
                        return 0;
                    }
                    int fd = open(dirPath, O_RDONLY);//open to read and return the index html
                    long temp4Byts;
                    while((temp4Byts = read(fd, indexBuf, sizeof (indexBuf))) > 0){
                        if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                            errorHendel(500,path, timebuf,finalReturn);
                            write(*indSOCK, finalReturn, strlen(finalReturn));
                            close(*indSOCK);
                            close(fd);
                            return 0;
                        }
                    }
                    if(temp4Byts < 0){//if the read func failed
                        errorHendel(500,path, timebuf,finalReturn);
                        write(*indSOCK, finalReturn, strlen(finalReturn));
                    }
                    close(*indSOCK);//after i read & write the request close socket and fd
                    close(fd);
                    return 0;// good!
                } else {//if it doesn't have read & ex premmision!!
                    errorHendel(403, path, timebuf, finalReturn);//"403 Forbidden"
                    if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                        errorHendel(500,path, timebuf,finalReturn);
                        write(*indSOCK, finalReturn, strlen(finalReturn));
                    }
                    close(*indSOCK);
                    return 0;
                }
            }
                ///else, return the content like dir_content.txt
            else {
                char dirContentBuf[10000] = "";
                memset(dirContentBuf, '\0', sizeof (dirContentBuf));
                dir_response(tablePath, timebuf, finalReturn, fileSIZE, modyTIME);//ERROR
                if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                    errorHendel(500,path, timebuf,finalReturn);
                    write(*indSOCK, finalReturn, strlen(finalReturn));
                }
                close(*indSOCK);
                return 0;
            }
        }
    }
        /*
         *7) if(the path is file){
         *   check if there is a 'read' permission for everyone.
         *   and if the file is in some directory,
         *   all the directories in the path must have executing permissions.
         *   if there is, return the file in the format of file.txt
         *   else, send "403 Forbidden" - errorHendel(403),
         *   }
         */

    else { //if the path isn't directory(7)
        int result = isReadable(path); // R_OK for readable
        if (result == 0) {//if there is
            // permission to read, I checked if it's readable for all the path :)
            //here i'm going to read the whole file to insert it inside the response
            //start with write the header and read the content
            unsigned char bufferToReadFile[10000] = "";
            memset(bufferToReadFile, '\0', sizeof (bufferToReadFile));
            struct stat statsNotDir;//first definition of stat for this type of case
            stat(dirPath,&statsNotDir);///check if failed?
            response(path, timebuf, finalReturn, (int) statsNotDir.st_size,
                     modyTIME);//like in file.txt(char* path, char* timebuf, int errNum)
            if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                errorHendel(500,path, timebuf,finalReturn);
                write(*indSOCK, finalReturn, strlen(finalReturn));
                close(*indSOCK);
                return 0;
            }//open the file to read and write to socket
            int fd = open(dirPath, O_RDONLY);
            if(fd < 0){
                errorHendel(500,path, timebuf,finalReturn);
                write(*indSOCK, finalReturn, strlen(finalReturn));
                close(fd);
                close(*indSOCK);
                return 0;
            }
            long rbyts;
            while(1) {
                rbyts=read(fd, bufferToReadFile, sizeof (bufferToReadFile));
                if(rbyts == 0)//finish to read
                    break;
                if(rbyts < 0){ //if fail
                    errorHendel(500,path, timebuf,finalReturn);
                    write(*indSOCK, finalReturn, strlen(finalReturn));
                    close(fd);
                    close(*indSOCK);
                    return 0;
                }

                if((write(*indSOCK, bufferToReadFile, rbyts)) < 0){
                    errorHendel(500,path, timebuf,finalReturn);
                    write(*indSOCK, finalReturn, strlen(finalReturn));
                    close(fd);
                    close(*indSOCK);
                    return 0;
                }
            }
            close(fd);
            close(*indSOCK);
            return 0;

        } else { //the file isn't readable
            errorHendel(403, path, timebuf, finalReturn);//"403 Forbidden"
            if(write(*indSOCK, finalReturn, strlen(finalReturn)) < 0){
                errorHendel(500,path, timebuf,finalReturn);
                write(*indSOCK, finalReturn, strlen(finalReturn));
            }
            close(*indSOCK);
            return 0;//V
        }
    }
    return 0;
}
//here i hendel the error return to the user, bulid for each of them the string that need to appear on the screen.
char *errorHendel(int num, char *path, char *timebuf, char *errorResponse) {
    //save each case tamplate and sprintf all after
    char status[200] = "";
    char contentLen[10] = "";
    char errNum[4] = "";
    char bodyAlert[100] = "";
    //char location[100] = "";
    //int Flag302 = 0;
    if (num == 501) {//not supported
        strcpy(status, "Not supported");
        strcpy(contentLen, "129");
        strcpy(errNum, "501");
        strcpy(bodyAlert, "Method is not supported");
    }
    if (num == 404) {
        strcpy(status, "Not Found");
        strcpy(contentLen, "112");
        strcpy(errNum, "404");
        strcpy(bodyAlert, "File not found");
    }
    if (num == 302) {//because its unice i do it here and return
        strcat(path, "/");
        sprintf(errorResponse,"HTTP/1.1 302 Found\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Location: %s\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 123\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n"
                              "<BODY><H4>302 Found</H4>\r\n"
                              "Directories must end with a slash.\r\n"
                              "</BODY></HTML>\r\n\r\n",timebuf, path);
        return errorResponse;
    }

    if (num == 500) {
        strcpy(status, "Internal Server Error");
        strcpy(contentLen, "144");
        strcpy(errNum, "500");
        strcpy(bodyAlert, "Some server side error");
    }
    if (num == 403) {
        strcpy(status, "Forbidden");
        strcpy(contentLen, "111");
        strcpy(errNum, "403");
        strcpy(bodyAlert, "Access denied");
    }
    if (num == 400) {
        strcpy(status, "Bad Request");
        strcpy(contentLen, "113");
        strcpy(errNum, "400");
        strcpy(bodyAlert, "Bad Request");
    }
    sprintf(errorResponse,"HTTP/1.1 %d %s\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: %s\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n"
                          "<BODY><H4>%d %s</H4>\r\n"
                          "%s.\r\n"
                          "</BODY></HTML>\r\n\r\n", num, status, timebuf, contentLen,
            num, status,num, status, bodyAlert);
    return errorResponse;

}

//this is the response of file
char *response(char *path, char *timebuf, char *finalResponse, int contentLen, char *modyTIME) {
    char *contentType = get_mime_type(path);//insert the content into the char*
    if (contentType != NULL) {// in case i have / dont have contentType
        sprintf(finalResponse, "HTTP/1.1 200 OK\r\n"
                               "Server: webserver/1.0\r\n"
                               "Date: %s\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %d\r\n"
                               "Last-Modified: %s\r\n"
                               "Connection: close\r\n"
                               "\r\n", timebuf, contentType, contentLen, modyTIME);
    } else {
        sprintf(finalResponse, "HTTP/1.1 200 OK\r\n"
                               "Server: webserver/1.0\r\n"
                               "Date: %s\r\n"
                               "Content-Length: %d\r\n"
                               "Last-Modified: %s\r\n"
                               "Connection: close\r\n"
                               "\r\n", timebuf, contentLen, modyTIME);
    }
    return finalResponse;
}
//the responce of the user with the content of each folder.
void dir_response(char *path, char *timebuf, char *finalResponse, long contentLength, char *modificationTime) {//V
    //divide the response to three parts:header, items info, footer
    char totalBody[9000] = "";
    char heder[5000] = "";
    char firstPartRes[5000] = "";
    char secondPartRes[5000] = "";
    char thirdPartRes[500] = "";
    //bulid the first part of response.
    //printf("%s", path);
    sprintf(firstPartRes,
            "<HTML>\n"
            "<HEAD><TITLE>Index of %s</TITLE></HEAD>\n"
            "\n"
            "<BODY>\n"
            "<H4>Index of %s</H4>\n"
            "\n"
            "<table CELLSPACING=8>\n"
            "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>"
            , path, path);
    strcpy(totalBody, firstPartRes);//adding the first part to the final response already
    //by this DIR i can read the info it contains.
    DIR *openFile = opendir(path);
    struct dirent *direntStruct;
    struct stat stats;
    direntStruct = readdir(openFile);
    //moving on this loop allow me to read all names and put them inside the table
    while (direntStruct != NULL) {
        char dirPath[1000] = "";
        strcpy(dirPath, path);
        strcat(dirPath, "/");
        strcat(dirPath, direntStruct->d_name);//adding to the path the /d.name
        stat(dirPath, &stats);

        if ((stat(dirPath, &stats) == 0) && (S_ISREG(stats.st_mode) != 0)) {//if its file - write file size
            char modyTIME[128]="";
            strftime(modyTIME, sizeof(modyTIME), RFC1123FMT, localtime(&stats.st_mtime));
            sprintf(secondPartRes, "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td>\n"
                                   "<td>%ld</td>\n""</tr>\n", direntStruct->d_name, direntStruct->d_name,
                    modyTIME, stats.st_size);
            strcat(totalBody, secondPartRes);

        } else {//if not then without
            char modyTIME[128]="";
            strftime(modyTIME, sizeof(modyTIME), RFC1123FMT, localtime(&stats.st_mtime));
            sprintf(secondPartRes, "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td>\n""</tr>\n",
                    direntStruct->d_name, direntStruct->d_name, modyTIME);
            strcat(totalBody, secondPartRes);
        }
        direntStruct = readdir(openFile);//go to the next one
    }
    sprintf(thirdPartRes, "</table>\n"
                          "<HR>\n"
                          "<ADDRESS>webserver/1.0</ADDRESS>\n"
                          "</BODY></HTML>");
    strcat(totalBody, thirdPartRes);//conect the last part to the response
    sprintf(heder, "HTTP/1.0 200 OK\r\n"
                   "Server: webserver/1.0\r\n"
                   "Date: %s\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: %ld\r\n"
                   "Last-Modified: %s\r\n"
                   "Connection: close\r\n\r\n", timebuf, strlen(totalBody), modificationTime);
    strcpy(finalResponse, heder);
    strcat(finalResponse, totalBody);
    closedir(openFile);
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
/*
 * This func checks if the path is readable trow the hole path.
 * it starts with filling the array and check each sell if it's readable.
 */
int isReadable(char *path) {
    struct stat statsForRead;//first definition of stat
    char semiPath[1000] = "";
    getcwd(semiPath,sizeof (semiPath));
    strcat(semiPath, path);
    char savePath[1000] = "";
    strcpy(savePath, path);
    char deli[] = "/";
    //in token for last now there is the file we want to go and know it is had exe permission.
    stat(semiPath, &statsForRead);
    if (!(statsForRead.st_mode &(S_IROTH))) {//if the last one doesn't have permission return.
        return -1;
    }
    //now i'll check if the hole path have exe permissions.
    char *token = strtok(semiPath, deli);
    char temp[2000] = "";
    while (token != NULL) {
        strcat(temp, "/");
        strcat(temp, token);
        //here i check if all the path until the last one is x_OK and if the last one is R_OK
        stat(temp, &statsForRead);
        if ((!(statsForRead.st_mode & S_IXOTH)) && S_ISREG(statsForRead.st_mode) == 0) {//for execute.
            return -1;
        }
        token = strtok(NULL, deli);
    }
    return 0;
}
bool file_exists(char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

int isDir(const char *fileName) {//check if the path i got is a folder
    struct stat path;
    stat(fileName, &path);
    return S_ISREG(path.st_mode);
}
//solve the problem of the sigSEVE and premmissions
//why the paths are not good when i write them in the URL