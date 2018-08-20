//Using SDL2 and standard IO

/*********************************************************

* Filename: part2-mandelbrot.c

* Student name: Ish Handa

* Student no.: 3035238565

* Date: Nov 17, 2017

* version: 1.4

* Development platform: Windows 10 with linux bash

* Compilation: gcc part2-Mandelbrot.c -l SDL2 -lm

*********************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include "Mandel.h"
#include "draw.h"
#include <signal.h>


typedef struct task
{
int start_row;
int num_of_rows;
}TASK;

TASK* task_pool;
pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
int task_pool_cnt=0;
pthread_cond_t notFULL = PTHREAD_COND_INITIALIZER;
pthread_cond_t notEMPTY = PTHREAD_COND_INITIALIZER;
int pool_index_in=0;
int pool_index_out=0;
int start_row =0;

float * pixels;

int buffer_size;

int thread_cnt;

int total_tasks=0;

int task_cnt;


void *consumer(void *idp)
{
    int *my_id = (int *)idp;
    struct timespec start_compute, end_compute;
    printf("Worker(%d): Start up. Wait for task!\n",*my_id);

    int *cnt = (int*) malloc(sizeof (int));
    *cnt =0;

    while(total_tasks<=(IMAGE_HEIGHT/task_cnt))  // run till total task is less that the distributed tasks
    {
        pthread_mutex_lock(&pool_lock);  // acquire lock
        while(task_pool_cnt == 0) // enter if task pool count is 0
        {
            pthread_cond_wait(&notEMPTY,&pool_lock);  // wait for not empty signal to read

        }
        printf("Worker(%d): Start the computation\n",*my_id);

        float difftime = 0;
        clock_gettime(CLOCK_MONOTONIC, &start_compute); // start clock

        *cnt = *cnt +1;  // increase the task count
        TASK task_read;

        task_read = task_pool[pool_index_out]; // read from the task pool
        task_pool_cnt--;  // decrement the task pool cnt to open up space in the buffer
        pool_index_out = (pool_index_out+1)%buffer_size; // increment the pool index out
        total_tasks++;  // increment task count
        pthread_cond_signal(&notFULL);  // send not full signal
        pthread_mutex_unlock(&pool_lock);  // unlock the mutex

        int y,x;

        for (y=task_read.start_row; y<(task_read.num_of_rows+task_read.start_row)&& y < IMAGE_HEIGHT; y++)  // run from start row to end row
        {
            for (x=0; x<IMAGE_WIDTH; x++)
            {
                pixels[x+(y*IMAGE_WIDTH)]=Mandelbrot(x,y); // add computation directly to pixel
            }

        }

        // report timings

        clock_gettime(CLOCK_MONOTONIC, &end_compute);
        difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;  // calculate the time each child takes
        printf("Worker(%d): ... completed. Elapse time = %.3f ms\n",*my_id, difftime);


    }
    pthread_exit((void *) cnt);  // return the task count

}

int main( int argc, char* args[])
{

    // Initialize the pixel array to write on. Since threads share the same open file, we can directly write on pixels from any thread

    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    if (pixels == NULL) {
        printf("Out of memory!!\n");
        exit(1);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);  // start the clock

    thread_cnt = atoi(args[1]);  // input the thread count
    task_cnt = atoi(args[2]);  // input the size of task
    buffer_size = atoi(args[3]);  // input the buffer size for the problem

    pthread_t threads[thread_cnt];   // initialize the threads
    task_pool = (TASK*) malloc(sizeof(TASK) * buffer_size); // and create the task pool

    int i;

    int thread_ids[thread_cnt];  // creating an array to store thread ID


    for( i=0; i<thread_cnt; i++)
    {
        thread_ids[i]=i; // save thread ID
        pthread_create(&threads[i],NULL,consumer, (void *)&thread_ids[i]); // create all the threads with the thread ID as an argument
    }

    float difftime = 0;

    TASK taskread;
    taskread.num_of_rows = task_cnt;  // input the start row and task size into the struct
    taskread.start_row = start_row;
    task_pool[pool_index_in]= taskread;  // store the task into the task pool
    task_pool_cnt++;  // increment the task pool cnt
    start_row = taskread.start_row + task_cnt; // update start row for next loop so that same row is not send twice
    pool_index_in = (pool_index_in+1)%buffer_size;
    total_tasks++;

    while(total_tasks<=(IMAGE_HEIGHT/task_cnt))  // run till total task is less that the distributed tasks
    {
        pthread_mutex_lock(&pool_lock);  // put mutex lock

        while(task_pool_cnt == buffer_size)  // as long as task pool count is same as buffer size then wait for signal
            {
                pthread_cond_wait(&notFULL, &pool_lock);  // wait for a not full signal to write to buffer
            }

        taskread.num_of_rows = task_cnt;
        taskread.start_row = start_row;
        task_pool[pool_index_in]= taskread; // store the task into the task pool
        task_pool_cnt++;  // increment the task pool cnt
        start_row = taskread.start_row + task_cnt;  // update start row for next loop so that same row is not send twice
        pool_index_in = (pool_index_in+1)%buffer_size;   // add to buffer index
        pthread_cond_signal(&notEMPTY);  // send a not empty signal to threads
        pthread_mutex_unlock(&pool_lock);// open mutex

        if(total_tasks==(IMAGE_HEIGHT/task_cnt))   // when total task is equal to the total tasks to be performed, break from the loop
            break;

    }

    int * total;

    int total_array[thread_cnt];

    for( i=0; i<thread_cnt; i++)
    {
        pthread_join(threads[i], (void **) &total);  // join all the threads to get the task count
	total_array[i]=*total;

    }
 for( i=0; i<thread_cnt; i++)
    {
        printf("Worker thread %d has terminated and completed %d tasks\n",thread_ids[i],total_array[i]); // print termination status and task count by that thread
    }

    printf("All worker threads have terminated\n");

        //Report timing

    struct rusage useda,usedb;  // used to calculate the time spent by parent and child in user and systems mode

    getrusage(RUSAGE_SELF, &usedb);

    printf("Total time spent by process and its threads in user mode = %ld ms\nTotal time spent by process and its threads in system mode = %ld ms\n",
      (usedb.ru_utime).tv_sec*1000+((usedb.ru_utime).tv_usec/1000),(usedb.ru_stime).tv_sec*1000+(usedb.ru_stime).tv_usec/1000);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
  	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
  	printf("Total elapse time measured by the process = %.3f ms\n", difftime);  // print total time

    printf("Draw the image\n");
    //Draw the image by using the SDL2 library
    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);

    return 0;
}
