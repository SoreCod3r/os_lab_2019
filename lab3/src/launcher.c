#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s seed arraysize\n", argv[0]);
    return 1;
  }

  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    /* Этот код выполняется в дочернем процессе. */
    execl("./sequential_min_max",
          "sequential_min_max",
          argv[1],
          argv[2],
          (char *)NULL);

    /* Сюда попадём только при ошибке execl. */
    perror("execl");
    _exit(127);
  }

  /* Этот код выполняется в родительском процессе. */
  int status;

  while (waitpid(pid, &status, 0) == -1) {
    if (errno == EINTR) {
      continue;
    }

    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    fprintf(stderr, "Child terminated by signal %d\n",
            WTERMSIG(status));
  }

  return 1;
}