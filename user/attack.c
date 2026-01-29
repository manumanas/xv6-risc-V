#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // Your code here.
  char *p = sbrk(4096 * 4);   // ask for 4 pages

  int best_i = -1;
  int best_len = 0;

  for(int i = 0; i < 4096 * 4; i++) {

    if((p[i] >= 'A' && p[i] <= 'Z') ||
       (p[i] >= 'a' && p[i] <= 'z') ||
       (p[i] >= '0' && p[i] <= '9')) {

      int j = i;

      while(j < 4096 * 4 &&
            ((p[j] >= 'A' && p[j] <= 'Z') ||
             (p[j] >= 'a' && p[j] <= 'z') ||
             (p[j] >= '0' && p[j] <= '9'))) {
        j++;
      }

      int len = j - i;

      if(len >= 4 && len <= 32 && len > best_len) {
        best_i = i;
        best_len = len;
      }
    }
  }

  if(best_i >= 0) {
    write(1, &p[best_i], best_len);
    write(1, "\n", 1);
  }

  // exit(1);
  exit(0);

}
