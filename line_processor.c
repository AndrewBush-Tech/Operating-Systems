#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>

/*
- A program with a pipeline of 4 threads that interact with each other as producers and consumers.
- Input thread is the first thread in the pipeline. It gets input from the user as a file or standard in and puts it in a buffer it shares with the next thread in the pipeline.
- Line separator thread is the second thread in the pipeline. It consumes items from the buffer it shares with the input thread. It replaces every line separator in the input by a space.
- It puts the space in a buffer it shares with the next thread in the pipeline. Thus this thread implements both consumer and producer functionalities.
- The plus sign thread is the third thread in the pipeline. It consumes items from the buffer it shares with the line separator thread and replaces "++" with a "^".
- Output thread is the fourth thread in the pipeline. It consumes items from the buffer it shares with the plus sign thread and prints the items as standard out.
*/

// Size of the buffers
#define SIZE 1000

// Number of items that will be produced. This number is less than the size of the buffer. Hence, we can model the buffer as being unbounded.
#define NUM_ITEMS 50

bool stop_processing = false;

// Buffer 1, shared resource between input thread and line separator thread
char buffer_1[SIZE];
// Number of items in the buffer
int count_1 = 0;
// Index where the input thread will put the next item
int prod_idx_1 = 0;
// Index where the square-root thread will pick up the next item
int con_idx_1 = 0;
// Initialize the mutex for buffer 1
pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER;
// Initialize the condition variable for buffer 1
pthread_cond_t full_1 = PTHREAD_COND_INITIALIZER;


// Buffer 2, shared resource between line separator thread and plus sign thread
char buffer_2[SIZE];
// Number of items in the buffer
int count_2 = 0;
// Index where the thread will put the next item
int prod_idx_2 = 0;
// Index where the output thread will pick up the next item
int con_idx_2 = 0;
// Initialize the mutex for buffer 2
pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
// Initialize the condition variable for buffer 2
pthread_cond_t full_2 = PTHREAD_COND_INITIALIZER;


// Buffer 3, shared resource between plus sign thread and output thread
char buffer_3[SIZE];
// Number of items in the buffer
int count_3 = 0;
// Index where the square-root thread will put the next item
int prod_idx_3 = 0;
// Index where the output thread will pick up the next item
int con_idx_3 = 0;
// Initialize the mutex for buffer 3
pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;
// Initialize the condition variable for buffer 3
pthread_cond_t full_3 = PTHREAD_COND_INITIALIZER;

/*
Get the next item from buffer 1
*/
char get_buff_1(){
  // Lock the mutex before checking if the buffer has data
  pthread_mutex_lock(&mutex_1);
  while (count_1 == 0)
    // Buffer is empty. Wait for the producer to signal that the buffer has data
    pthread_cond_wait(&full_1, &mutex_1);
  char item = buffer_1[con_idx_1];
  // Increment the index from which the item will be picked up from wrap
  con_idx_1 = (con_idx_1 + 1) % SIZE;
  count_1--;
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_1);
  // Return the item
  return item;
}

/*
 Put an item in buff_1
*/
void put_buff_1(char item){
  // Lock the mutex before putting the item in the buffer
  pthread_mutex_lock(&mutex_1);
  // Put the item in the buffer
  buffer_1[prod_idx_1] = item;
  // Increment the index where the next item will be put and wrap if necessary
  prod_idx_1 = (prod_idx_1 + 1) % SIZE;
  count_1++;
  // Signal to the consumer that the buffer is no longer empty
  pthread_cond_signal(&full_1);
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_1);
}

/*
Get the next item from buffer 2
*/
char get_buff_2(){
   // Lock the mutex before checking if the buffer has data
  pthread_mutex_lock(&mutex_2);
  while (count_2 == 0)
    // Buffer is empty. Wait for the producer to signal that the buffer has data
    pthread_cond_wait(&full_2, &mutex_2);
  char item = buffer_2[con_idx_2];
  // Increment the index from which the item will be picked up from wrap
  con_idx_2 = (con_idx_2 + 1) % SIZE;
  count_2--;
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_2);
  // Return the item
  return item;
}
/*
 Put an item in buff_2
*/
void put_buff_2(char item){
  // Lock the mutex before putting the item in the buffer
  pthread_mutex_lock(&mutex_2);
  // Put the item in the buffer
  buffer_2[prod_idx_2] = item;
  // Increment the index where the next item will be put and wrap if necessary
  prod_idx_2 = (prod_idx_2 + 1) % SIZE;
  count_2++;
  // Signal to the consumer that the buffer is no longer empty
  pthread_cond_signal(&full_2);
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_2);
}
/*
Get the next item from buffer 3
*/
char get_buff_3(){
  // Lock the mutex before checking if the buffer has data
  pthread_mutex_lock(&mutex_3);
  while (count_3 == 0)
    // Buffer is empty. Wait for the producer to signal that the buffer has data
    pthread_cond_wait(&full_3, &mutex_3);
  char item = buffer_3[con_idx_3];
  // Increment the index from which the item will be picked up from wrap
  con_idx_3 = (con_idx_3 + 1) % SIZE;
  count_3--;
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_3);
  // Return the item
  return item;
}
/*
 Put an item in buff_3
*/
void put_buff_3(char item){
  // Lock the mutex before putting the item in the buffer
  pthread_mutex_lock(&mutex_3);
  // Put the item in the buffer
  buffer_3[prod_idx_3] = item;
  // Increment the index where the next item will be put and wrap if necessary
  prod_idx_3 = (prod_idx_3 + 1) % SIZE;
  count_3++;
  // Signal to the consumer that the buffer is no longer empty
  pthread_cond_signal(&full_3);
  // Unlock the mutex
  pthread_mutex_unlock(&mutex_3);
}

/*
  Function that the input thread will run.
  Get input from file.
*/
void *get_input(void *args) {
  char line[SIZE];
  while (!stop_processing) {
    if (fgets(line, SIZE, stdin) == NULL) {
      break;
    }
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = ' ';
      len--;
    }
    if (strcmp(line, "STOP\n") == 0) {
      stop_processing = true;
    } else {
      for (size_t i = 0; i < len; i++) {
        put_buff_1(line[i]);
      }
      put_buff_1(' ');
    }
  }
  put_buff_1('\0');
  return NULL;
}

/*
 Function that the line separator thread will run. It replaces every line separator in the input by a space.
 Consume an item from the buffer shared with the input thread.
 Produce an item in the buffer shared with the plus sign thread.
*/
void *line_separator(void *args) {
  for (;;) {
    char item = get_buff_1();
    if (item == '\0') {
      put_buff_2('\0');
      break;
    } else if (item == '\n') {
      put_buff_2(' ');
    } else {
      put_buff_2(item);
    }
  }
  return NULL;
}
/*
 Function that the plus sign thread will run. It replaces two plus signs with a caret symbol.
 Consume an item from the buffer shared with the input thread.
 Produce an item in the buffer shared with the output thread.
*/
void *plus_sign(void *args) {
  bool prev_plus = false;
  for (;;) {
    char item = get_buff_2();
    if (item == '\0') {
      put_buff_3('\0');
      break;
    }
    if (item == '+') {
      if (prev_plus) {
        put_buff_3('^');
        prev_plus = false;
      } else {
        prev_plus = true;
      }
    } else {
      if (prev_plus) {
        put_buff_3('+');
        prev_plus = false;
      }
      put_buff_3(item);
    }
  }
  return NULL;
}

/*
 Function that the output thread will run.
 Prints the items.
*/
void *write_output(void *args) {
  char line[81] = {0};
  int idx = 0;
  for (;;) {
    char item = get_buff_3();
    if (item == '\0') {
      if (idx > 0) {
        line[idx] = '\n';
        write(STDOUT_FILENO, line, idx + 1);
        idx = 0;
      }
      break;
    }
    line[idx++] = item;
    if (idx == 80) {
      line[idx] = '\n';
      write(STDOUT_FILENO, line, 81);
      idx = 0;
    }
  }
  return NULL;
}

int main()
{
    srand(time(0));
    pthread_t input_t, line_separator_t, plus_sign_t, output_t;
    // Create the threads
    pthread_create(&input_t, NULL, get_input, NULL);
    pthread_create(&line_separator_t, NULL, line_separator, NULL);
    pthread_create(&plus_sign_t, NULL, plus_sign, NULL);
    pthread_create(&output_t, NULL, write_output, NULL);
    // Wait for the threads to terminate
    pthread_join(input_t, NULL);
    pthread_join(line_separator_t, NULL);
    pthread_join(plus_sign_t, NULL);
    pthread_join(output_t, NULL);
    //printf("All threads have completed.\n");
    return EXIT_SUCCESS;
}