//  #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/fs.h"

// void
// find(char *path, char *filename)
// {
//   char buf[512];
//   int fd;
//   struct dirent de;
//   struct stat st;

//   // Open the current path
//   fd = open(path, 0);
//   if (fd < 0) {
//     fprintf(2, "find: cannot open %s\n", path);
//     return;
//   }

//   // Get file status
//   if (fstat(fd, &st) < 0) {
//     fprintf(2, "find: cannot stat %s\n", path);
//     close(fd);
//     return;
//   }

//   // If path is a file
//   if (st.type == T_FILE) {
//     char *p = path + strlen(path);

//     // Move backwards to get file name
//     while (p >= path && *p != '/')
//       p--;
//     p++;

//     // Compare file name
//     if (strcmp(p, filename) == 0) {
//       printf("%s\n", path);
//     }

//     close(fd);
//     return;
//   }

//   // If path is a directory
//   if (st.type == T_DIR) {
//     while (read(fd, &de, sizeof(de)) == sizeof(de)) {

//       // Skip empty entries
//       if (de.inum == 0)
//         continue;

//       // Skip "." and ".."
//       if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
//         continue;

//       // Build new path
//       int len = strlen(path);
//     memmove(buf, path, len);
//     buf[len] = '/';
//     memmove(buf + len + 1, de.name, strlen(de.name) + 1);


//       // Recursive call
//       find(buf, filename);
//     }
//   }

//   close(fd);
// }

// int
// main(int argc, char *argv[])
// {
//   if (argc != 3) {
//     fprintf(2, "Usage: find path filename\n");
//     exit(1);
//   }

//   find(argv[1], argv[2]);
//   exit(0);
// } 


#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"

int use_exec = 0;
char *exec_argv[MAXARG];
int exec_argc = 0;

void
run_exec(char *file)
{
  int pid = fork();
  if (pid == 0) {
    // child
    exec_argv[exec_argc] = file;
    exec_argv[exec_argc + 1] = 0;
    exec(exec_argv[0], exec_argv);
    fprintf(2, "find: exec failed\n");
    exit(1);
  } else {
    // parent
    wait(0);
  }
}

void
find(char *path, char *filename)
{
  char buf[512];
  int fd;
  struct dirent de;
  struct stat st;

  fd = open(path, 0);
  if (fd < 0)
    return;

  if (fstat(fd, &st) < 0) {
    close(fd);
    return;
  }

  // FILE
  if (st.type == T_FILE) {
    char *p = path + strlen(path);
    while (p >= path && *p != '/')
      p--;
    p++;

    if (strcmp(p, filename) == 0) {
      if (use_exec)
        run_exec(path);
      else
        printf("%s\n", path);
    }

    close(fd);
    return;
  }

  // DIRECTORY
  if (st.type == T_DIR) {
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;

      int len = strlen(path);
      memmove(buf, path, len);
      buf[len] = '/';
      memmove(buf + len + 1, de.name, strlen(de.name) + 1);

      find(buf, filename);
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "Usage: find path filename [-exec cmd ...]\n");
    exit(1);
  }

  // Check for -exec
  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-exec") == 0) {
      use_exec = 1;
      exec_argc = argc - i - 1;
      for (int j = 0; j < exec_argc; j++)
        exec_argv[j] = argv[i + 1 + j];
      break;
    }
  }

  find(argv[1], argv[2]);
  exit(0);
}
