/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// command line을 통해 넘겨받은 인자(Port)로 listening socket을 열어 요청을 준비한다.(open_listenfd)
// accept 함수를 통해 클라이언트 요청을 받아 연결을 수립한다. 연결이 되면, getnameinfo 함수를 통해 소켓 주소 구조체를 풀어서 요청한 hostname과 port를 출력한다.
// client에서 받은 요청을 처리하는 doit함수로 들어간다.
// 마지막으로 close함수를 통해 연결 요청을 닫는다.
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // check command-line args
  if (argc != 2) {
    fprintf(stderr, "usage : %s <port> \n", argv[0]);
    exit(1);
  }

  // 인자로 받은 port에 listenfd 생성
  listenfd = Open_listenfd(argv[1]);
  while(1)
  {
    clientlen = sizeof(clientaddr);
    // listenfd에 연결을 요청한 client의 주소를 sockaddr_storage에 저장
    // client의 주소, 크기를 받아서 저장할 곳의 포인터를 인자로 받음
    // accept의 세번째 인자는 일단 addr의 크기를 설정하고(input) 접속이 완료되면 실제로 addr에 설정된 접속한 client의 주소 정보 크기를 저장
    // 무한 서버 루프 실행, 반복적으로 연결 요청을 접수한다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accept connection from (%s, %s) \n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // initiation
  Rio_readlineb(&rio, buf, MAXLINE);  // read and parse the request line
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // buf의 내용을 각각 method, uri, version에 저장

  // Tiny only acceps the "GET" method
  if (!(strcasecmp(method, "GET")) == 0 || strcasecmp(method, "HEAD") == 0){   // strcasecmp: 대소문자 구분 없이 두 문자열 비교
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
    return;
  }

  /* Read and ignore request headers */
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // flags indicating whether request is for static or dynamic content
  if (stat(filename, &sbuf) < 0){  // get file information and save it to sbuf(struct stat)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve static content */
  if (is_static){
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){  // regular file and permission to read
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the fuke");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  /* Serve dynamic content */
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // executable file
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char*errnum, char *shortmsg, char *longmsg){

  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body,  "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body,  "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body,  "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body,  "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  // header
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // body
  Rio_writen(fd, body, strlen(body));
}

/* Ignore header: TINY does not use header */
void read_requesthdrs(rio_t *rp){

  char buf[MAXLINE];

  // header ends with '\r\n'
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { // static content
    strcpy(cgiargs, ""); //uri에 cgi-bin과 일치하는 문자열이 없다면 cgiargs에는 빈 문자열을 저장
    strcpy(filename, "."); // 아래 줄과 더불어 상대 리눅스 경로이름으로 변환(./index.html과 같은)
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/') // uri가 '/'로 끝난다면 기본 파일 이름추가
      strcat(filename, "home.html");
    return 1;
  }
  else { // dynamic content
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, ptr + 1);
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // client에게 response header 보내기
  get_filetype(filename, filetype); // 아래의 함수에서 알아보자
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  // O_RDONLY -> 파일을 읽기 전용으로 열기, O_WRONLY -> 쓰기전용으로 열기, O_RDWR -> 읽고 쓰고 다하기
  srcfd = Open(filename, O_RDONLY, 0);
  // mmap는 요청한 파일을 가상 메모리 영역으로 매핑함
  // Mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
  // fd로 지정된 파일에서 offset을 시작으로 length바이트 만큼 start주소로 대응시키도록 한다.
  // start주소는 단지 그 주소를 사용했으면 좋겠다는 정도로 보통 0을 지정한다.
  // mmap는 지정된 영역이 대응된 실제 시작위치를 반환한다.
  // prot 인자는 원하는 메모리 보호모드(:12)를 설정한다
  // -> PROT_EXEC - 실행가능, PROT_READ - 읽기 가능, NONE - 접근 불가, WRITE - 쓰기 가능
  // flags는 대응된 객체의 타입, 대응 옵션, 대응된 페이지 복사본에 대한 수정이 그 프로세스에서만 보일 것인지, 다른 참조하는 프로세스와 공유할 것인지 설정
  // MAP_FIXED - 지정한 주소만 사용, 사용 못한다면 실패 / MAP_SHARED - 대응된 객체를 다른 모든 프로세스와 공유 / MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음

  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // srcfd의 첫번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기허용 가상 메모리 영역으로 매핑
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); // 파일을 클라이언트에게 전송
  // Munmap(srcp, filesize); // 매핑된 가상메모리 주소를 반환
  Free(srcp);
}

/*
 * get_filetype: Derive file type from filename
 */
void get_filetype(char *filename, char* filetype) {

  if (strstr(filename, ".html"))      // strstr(대상문자열, 검색할문자열) --> 문자열의 시작 포인터 or NULL 반환
    strcpy(filetype, "text/html");    // strcpy(char *dest, const char *origin) --> origin문자열을 dest로 복사
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  /* Problem 11.7: add mp4 filetype */
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *mathod) {

  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 )K\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 이부분 공부 더 필요함!
  /* Child */
  if (Fork() == 0) {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);     // 새로운 환경변수 추가
    Dup2(fd, STDOUT_FILENO);                /* Redirect stdout to client */
    Execve(filename, emptylist, environ) ;  /* Run CGI prigram */
  }
  Wait(NULL);   /* Parent waits for and reaps child */
}