#define _XOPEN_SOURCE 600 /* Or higher */
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pzip.h"

struct chunk_data {
  int chunk_size;
  char* chunk;
  int char_freq[26];

  struct zipped_char* local_result;
  int local_count;

  pthread_barrier_t* barrier;

  int thread_index;
  int num_threads;
  struct chunk_data** all_data;
  struct zipped_char* zipped_chars;
};

void* chunk_handler(void* args) {
  struct chunk_data* params = (struct chunk_data*) args;
  char* substr = params->chunk;
  int chunk_size = params->chunk_size;
  int char_freq[26] = {0};

  int local_index = 0;
  struct zipped_char* local_result = malloc(sizeof(struct zipped_char) * chunk_size);

  int last_char = substr[0];
  int char_counter = 1;

  int index = (substr[0] - 'a');
  char_freq[index] += 1;
  for(int i = 1; i < chunk_size; i++) {
    index = (substr[i] - 'a');
    char_freq[index] += 1;

    if(last_char != substr[i]) {
      local_result[local_index].character = substr[i - 1];
      local_result[local_index].occurence = char_counter;
      local_index += 1;

      last_char = substr[i];
      char_counter = 1;

    } else {
      char_counter += 1;
    }

  }

  local_result[local_index].character = last_char;
  local_result[local_index].occurence = char_counter;

  params->local_result = local_result;
  params->local_count = local_index + 1;
  memcpy(params->char_freq, char_freq, 26 * sizeof(int));

  pthread_barrier_wait(params->barrier);

  int offset = 0;
  for(int i = 0; i < params->thread_index; i++) {
    offset += params->all_data[i]->local_count;
  }

  for (int i = 0; i < params->local_count; i++) {
    params->zipped_chars[offset + i] = params->local_result[i];
  }

  return NULL;
}

/**
 * pzip() - zip an array of characters in parallel
 *
 * Inputs:
 * @n_threads:		   The number of threads to use in pzip
 * @input_chars:		   The input characters (a-z) to be zipped
 * @input_chars_size:	   The number of characaters in the input file
 *
 * Outputs:
 * @zipped_chars:       The array of zipped_char structs
 * @zipped_chars_count:   The total count of inserted elements into the zipped_chars array
 * @char_frequency: Total number of occurences
 *
 * NOTE: All outputs are already allocated. DO NOT MALLOC or REASSIGN THEM !!!
 *
 */
void pzip(int n_threads, char *input_chars, int input_chars_size,
	  struct zipped_char *zipped_chars, int *zipped_chars_count,
	  int *char_frequency)
{
  int chunk_size = input_chars_size / n_threads;

  pthread_t tid[n_threads];
  struct chunk_data* param_list[n_threads];
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, n_threads);

  for(int i = 0; i < n_threads; i++) {
    int start_index = i * chunk_size;

    struct chunk_data *params = malloc(sizeof(struct chunk_data));
    params->chunk_size = chunk_size;
    params->chunk = input_chars + start_index;
    params->barrier = &barrier;
    params->num_threads = n_threads;
    params->all_data = param_list;
    params->zipped_chars = zipped_chars;
    params->thread_index = i;

    param_list[i] = params;

    pthread_create(&tid[i], NULL, chunk_handler, (void*) params);
  }

  for (int i = 0; i < n_threads; i++) {
    pthread_join(tid[i], NULL);
  }

  for(int i = 0; i < n_threads; i++) {
    *zipped_chars_count += param_list[i]->local_count;
  }

  for (int i = 0; i < n_threads; i++) {
    for (int j = 0; j < 26; j++) {
        char_frequency[j] += param_list[i]->char_freq[j];
    }
  }

  for (int i = 0; i < n_threads; i++) {
    free(param_list[i]->chunk);
    free(param_list[i]->local_result);
    free(param_list[i]);
  }

  pthread_barrier_destroy(&barrier);
}
