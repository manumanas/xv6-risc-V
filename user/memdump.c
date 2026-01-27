#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}
void
memdump(char *fmt, char *data)
{
  while (*fmt) {
    switch (*fmt) {
      case 'i': {  // 32-bit integer, decimal
        int val = *(int *)data;           // Type pun: treat next 4 bytes as int
        printf("%d\n", val);
        data += sizeof(int);              // Advance pointer by 4 bytes
        break;
      }
      case 'p': {  // 64-bit pointer/value, hex
        uint64 val = *(uint64 *)data;     // Treat next 8 bytes as 64-bit unsigned
        printf("%lx\n", val);             // %lx for long hex (or %llx if needed)
        data += sizeof(uint64);           // Advance by 8 bytes
        break;
      }
      case 'h': {  // 16-bit short, decimal
        short val = *(short *)data;
        printf("%d\n", val);              // %d is fine (sign extension ok here)
        data += sizeof(short);            // Advance by 2 bytes
        break;
      }
      case 'c': {  // Single char
        char val = *data;
        printf("%c\n", val);
        data += sizeof(char);             // Advance by 1 byte
        break;
      }
      case 's': {  // 64-bit pointer to null-terminated string
        char *str = *(char **)data;       // Dereference the 8-byte pointer
        printf("%s\n", str);
        data += sizeof(char *);           // Advance by 8 bytes (pointer size)
        break;
      }
      case 'S': {  // Rest of data is null-terminated string
        printf("%s\n", data);             // Print from current position
        return;                           // Done — rest is consumed
      }
      default:
        // Unknown format → ignore or error (for safety, skip)
        break;
    }
    fmt++;  // Move to next format character
  }
}
