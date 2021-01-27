/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE], arg3[MAXLINE], arg4[MAXLINE];
    int n1=0, n2=0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
	*p = '\0';
	strcpy(arg1, buf);
	strcpy(arg2, p+1);

    p=strchr(arg1, '=');
    *p = '\0';
    strcpy(arg3, p+1);

    p=strchr(arg2, '=');
    *p = '\0';
    strcpy(arg4, p+1);

	n1 = atoi(arg3);
	n2 = atoi(arg4);
    }

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);
  
    /* Generate the HTTP response */
    printf("Content-length: %d\r\n", (int)strlen(content) ) ;
    printf("Content-type: text/html\r\n\r\n");
    printf("%s fff", content);
    fflush(stdout);
    printf("%s fff", content);
    fflush(stdout);

}
/* $end adder */