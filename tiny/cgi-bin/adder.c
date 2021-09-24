/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    sscanf(buf, "fname=%d", &n1);
    sscanf(p+1, "lname=%d", &n2);
    }

  method = getenv("REQUEST_METHOD");

  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTEH Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d \r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!! \r\n", content);

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  // stdout은 모든 출력값들이 가는 곳. C로 프로그래밍을 할 때, printf를 생각해보거나 파이썬의 print를 생각하면된다.
  // fflush()

  if (strcasecmp(method, "HEAD") != 0)
    printf("%s", content);
  fflush(stdout);
  exit(0);
}
/* $end adder */
