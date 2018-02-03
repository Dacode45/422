#include <stdlib.h>
#include <stdio.h>
#include <regex.h>

regex_t regex;
char msgbuf[100];

int main(int argc, char **argv) {
  if(regcomp(&regex, "link:([a-z|0-9|\\/|:|\\.]+)", REG_EXTENDED | REG_ICASE)){
    perror("Could not complie regex\n");
    exit(1);
  }
  const char * to_match = "01234567link:http://google3.com link:http://apple.com JOhn galt ";
  const char * p = to_match;
  const int groups = 2;
  regmatch_t m[groups];

  while (1) {
    int i = 0;
    int nomatch = regexec (&regex, p, groups, m, 0);
    if (nomatch) {
      printf ("No matches.\n");
      return nomatch;
    }
    for (i = 0; i < groups; i++) {
      int start;
      int finish;
      if (m[i].rm_so == -1) {
        break;
      }
      start = m[i].rm_so + (p - to_match);
      finish = m[i].rm_eo + (p - to_match);
      if (i == 0) {
        printf ("$& is ");
      }
      else {
        printf ("$%d is ", i);
      }
      printf ("'%.*s (bytes %d:%d)\n", (finish-start), to_match + start, start, finish);
    }
    p += m[0].rm_eo;
  }
  return 0;
}
