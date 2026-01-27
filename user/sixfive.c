#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SEPARATORS " -\r\t\n./,"   // The separator characters

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "Usage: sixfive file1 [file2 ...]\n");
    exit(1);
  }

  // Loop over each input file
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], 0);   // Open file read-only
    if (fd < 0) {
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      continue;
    }

    char buf[1];                 // Read one char at a time
    char numbuf[32];             // Buffer to build number (max ~10 digits + safety)
    int numidx = 0;              // Index in numbuf
    int in_number = 0;           // Flag: are we currently inside a number?

    // Treat start of file as separator → reset state
    while (read(fd, buf, 1) == 1) {
      char c = buf[0];

      if (strchr(SEPARATORS, c)) {
        // Separator → end current number if any
        if (in_number) {
          numbuf[numidx] = '\0';  // Null-terminate
          int n = atoi(numbuf);

          // Check if multiple of 5 or 6
          if (n % 5 == 0 || n % 6 == 0) {
            printf("%d\n", n);
          }

          numidx = 0;
          in_number = 0;
        }
      } else if (c >= '0' && c <= '9') {
        // Digit → add to current number
        if (numidx < sizeof(numbuf) - 1) {
          numbuf[numidx++] = c;
          in_number = 1;
        }
      } else {
        // Non-separator, non-digit → end current number if any
        if (in_number) {
          numbuf[numidx] = '\0';
          int n = atoi(numbuf);

          if (n % 5 == 0 || n % 6 == 0) {
            printf("%d\n", n);
          }

          numidx = 0;
          in_number = 0;
        }
      }
    }

    // End of file → treat as separator, finish any pending number
    if (in_number) {
      numbuf[numidx] = '\0';
      int n = atoi(numbuf);

      if (n % 5 == 0 || n % 6 == 0) {
        printf("%d\n", n);
      }
    }

    close(fd);
  }

  exit(0);
}