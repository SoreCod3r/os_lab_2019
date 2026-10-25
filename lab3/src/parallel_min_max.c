#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) 
{
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed < 0) 
            {
              printf("Seed cannot be negative");
              return -1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) 
            {
              printf("Array_size should be more than 0");
              return -1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) 
            {
              printf("pnum cannot be negative");
              return -1;
            }
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  // Массив для pipe: pipes[i][0] — чтение, pipes[i][1] — запись
  int pipes[pnum][2];

  // Если работаем через pipe, создаём их ДО fork
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        free(array);
        return 1;
      }
    }
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // ================= ДОЧЕРНИЙ ПРОЦЕСС =================

        // 1. Вычисляем диапазон [start, end) для этого ребёнка
        unsigned int start = i * (array_size / pnum);
        unsigned int end = (i == pnum - 1) ? array_size : (i + 1) * (array_size / pnum);

        // 2. Ищем локальные min и max в своём диапазоне
        struct MinMax local = GetMinMax(array, start, end);

        if (with_files) {
          // --- Вариант с файлами ---
          char filename[64];
          snprintf(filename, sizeof(filename), "min_max_%d.tmp", i);

          FILE *f = fopen(filename, "w");
          if (!f) {
            perror("fopen for write");
            _exit(1);
          }
          fprintf(f, "%d %d\n", local.min, local.max);
          fclose(f);
        } else {
          // --- Вариант с pipe ---
          // Закрываем все чужие pipe, чтобы не держать лишние дескрипторы
          for (int j = 0; j < pnum; j++) {
            if (j != i) {
              close(pipes[j][0]);
              close(pipes[j][1]);
            }
          }

          // Закрываем читающий конец своего pipe — он не нужен
          close(pipes[i][0]);

          // Пишем min и max в pipe
          if (write(pipes[i][1], &local.min, sizeof(int)) != sizeof(int) ||
              write(pipes[i][1], &local.max, sizeof(int)) != sizeof(int)) {
            perror("write to pipe");
            _exit(1);
          }

          close(pipes[i][1]);
        }

        // Важно: _exit, а не return, чтобы не дублировать буферы stdio родителя
        _exit(0);
      }

    } else {
      printf("Fork failed!\n");
      // Аварийная очистка pipe в родителе
      if (!with_files) {
        for (int j = 0; j < pnum; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
      }
      free(array);
      return 1;
    }
  }

  // ================= РОДИТЕЛЬСКИЙ ПРОЦЕСС =================

  // Родитель не пишет в pipe — закрываем все пишущие концы
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      close(pipes[i][1]);
    }
  }

  // Ждём завершения всех детей
  while (active_child_processes > 0) {
    wait(NULL);
    active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  // Собираем локальные результаты
  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      // --- Чтение из файла ---
      char filename[64];
      snprintf(filename, sizeof(filename), "min_max_%d.tmp", i);

      FILE *f = fopen(filename, "r");
      if (!f) {
        perror("fopen for read");
        continue;
      }
      if (fscanf(f, "%d %d", &min, &max) != 2) {
        perror("fscanf");
      }
      fclose(f);
      remove(filename); // удаляем временный файл
    } else {
      // --- Чтение из pipe ---
      if (read(pipes[i][0], &min, sizeof(int)) != sizeof(int) ||
          read(pipes[i][0], &max, sizeof(int)) != sizeof(int)) {
        perror("read from pipe");
        continue;
      }
      close(pipes[i][0]);
    }

    // Редукция: обновляем глобальные min и max
    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}