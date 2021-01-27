#include "csapp.h"

// doit은 무엇을 하는 함수일까요? 트랜잭션 처리함수. 들어온 요청을 읽고 분석.
// GETrequest가 들어오면 정적인지 동적인지 파악하여서 각각에 맞는 함수를 실행시킴. 오류시 에러표시도 포함.
void doit(int fd);

// rio -> ROBUST I/O
void read_requesthdrs(rio_t *rp);

// parse_uri는 무엇을 하는 함수일까요? 폴더안에서 특정 이름을 찾아서 파일이 동적인건지 정적인건지 알려줌.
int parse_uri(char *uri, char *filename, char *cgiargs);

// serve_static은 무엇을 하는 함수일까요? 정적인 파일일때 파일을 클라이언트로 응답
void serve_static(int fd, char *filename, int filesize);

// get_filetype은 무엇을 하는 함수일까요? http,text,jpg,png,gif파일을 찾아서 serve_static에서 사용
void get_filetype(char *filename, char *filetype);

// serve_dynamic은 무엇을 하는 함수일까요? 동적인 파일을 받았을때 fork 함수로 자식프로세스를 만든후에 거기서 CGI프로그램 실행한다. 
void serve_dynamic(int fd, char *filename, char *cgiargs);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// main이 받는 변수 argc 와 argv는 무엇일까 -> 배열 길이, filename, port
// main에서 하는 일은? 무한 루프를 돌면서 대기하는 역할
int main(int argc, char **argv) 
{
    int listenfd, connfd;                               // 여기서의 fd는 도대체 무슨 약자인걸까? -> file description
    char hostname[MAXLINE], port[MAXLINE];              // hostname:port -> localhost:4000
    socklen_t clientlen;                                //sizeof address
    struct sockaddr_storage clientaddr;                 //어떤 타입의 소켓 주소가 오든 감당할 수 있을 만큼 충분히 큰 구조체, 패밀리밖에 안들어감, 나머지 멤버는 다 패딩
    // 패밀리밖에 안들어가는데 여기서 어떻게 호스트네임이랑 포트를 가지고 오는 것일까...? -> 비공개라는 말이 있는데, 정보가 담겨있기는 한가봄..
    // SOCKADDR_STORAGE 구조체는  sockaddr 구조체가 쓰이는 곳에 사용할 수 있다.

    /* Check command line args */
    if (argc != 2) {        // 프로그램 실행 시 port를 안썼으면,
	fprintf(stderr, "usage: %s <port>\n", argv[0]);     //argv[0]의 사용법은 파일명 <port> 이다 라고 사용자에게 알려주는것
	exit(1);
    }

    // listenfd -> 이 포트에 대한 듣기 소켓 오픈~
    listenfd = Open_listenfd(argv[1]);

    // 무한 서버루프 실행
    while (1) 
    {
	    clientlen = sizeof(clientaddr);
	    
        // 연결 요청 접수
        // 연결 요청 큐에 아무것도 없을 경우 기본적으로 연결이 생길때까지 호출자를 막아둔다.
        // 소켓이 non-blocking 모드일 경우엔 에러를 띄운다.
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	    doit(connfd);                                             //line:netp:tiny:doit
	    Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 요청 라인 읽고 분석하기...
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);                 //rio 구조체 초기화
    
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    
    printf("Request headers: \n");
    printf("%s", buf);                                   //buf에는 Request headers가 저장되고 method, uri, version의 정보가 들어가 있을 것이다. 그걸 출력한다.
    sscanf(buf, "%s %s %s", method, uri, version);       //buf에 있는 method, uri, version의 정보를 각각 method, uri, version이란 변수에 저장한다.

    // 메소드가 get이 아니면 에러 띄우고 끝내기
    if (strcasecmp(method, "GET")) {                     //strcasecmp 는 string case compare의 약자로, 두 문자를 비교하는 함수다. 같으면 0을 리턴한다. 우리는 get요청만 처리한다.
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr
    // get인 경우 다른 요청 헤더 무시.

    //request headers를 읽는다.
    read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs

    // URI 분석하기
    // 파일이 없는 경우 에러 띄우기
    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }                                                    //line:netp:doit:endnotfound

    // 정적 컨텐츠를 요구하는 경우
    if (is_static)
    { /* Serve static content */
        // 파일이 읽기권한이 있는지 확인하기          
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        { 
            clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't read the file");
            return;
	    }
        // 그렇다면 클라이언트에게 파일 제공
	serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    }
    else 
    { /* Serve dynamic content */
        // 파일이 실행가능한 것인지
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        { 
            clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't run the CGI program");
            return;
        }
    //그렇다면 클라이언트에게 파일 제공
	serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
// 요청헤더 읽기
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);        // MAXLINE 까지 읽기
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {            // eof(한 줄이 전체가 개행문자인 곳) 만날 때까지 계속 읽기
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {                  //cgi-bin가 없다면
	strcpy(cgiargs, "");                            //line:netp:parseuri:clearcgi
	strcpy(filename, ".");                          //line:netp:parseuri:beginconvert1
	strcat(filename, uri);                          //line:netp:parseuri:endconvert1
	if (uri[strlen(uri)-1] == '/')                  //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");              //line:netp:parseuri:appenddefault
        // ./home.html이 된다.
	return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	//CGI 인자 추출
    if (ptr) {
        //물음표 뒤에 인자 다 같다 붙인다.
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
        // 포인터는 문자열 마지막으로 바꾼다.
	}
	else 
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	
    // ./uri의 형태가 된다.
    strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)         //// fd 응답받는 소켓(연결식별자), 파일 이름, 파일 사이즈
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    // 파일 접미어 검사해서 파일 이름에서 타입 가지고 오기
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype

    //한 줄씩 buf에 입력해서 클라이언트에 응답 보내기
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    //파일 열어서 파일의 fd를 srcfd에 저장
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    
    //PROT_READ -> 페이지는 읽을 수만 있다.
    // 파일을 어떤 메모리 공간에 대응시키고 첫주소를 리턴
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    Close(srcfd);                       //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write

    //대응시킨 녀석을 풀어준다. 유효하지 않은 메모리로 만듦
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap

}

/*
 * get_filetype - derive file type from file name
 */
// 파일 형식 추출하기
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { // 자식 생성하여 프로세스 진행
    
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:s   ervedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
//클라이언트에게 오류 보내기
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */