//Using SDL2 and standard IO

/*********************************************************

* Filename: part1a-mandelbrot.c

* Student name: Ish Handa

* Student no.: 3035238565

* Date: Oct 30, 2017

* version: 5.7

* Development platform: Windows 10 with linux bash

* Compilation: gcc part1b-Mandelbrot.c -l SDL2 -lm

* Time Spent: 24 hours

*********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>

typedef struct message
{
int row_index;
float rowdata[IMAGE_WIDTH];
pid_t child_pid;

} msg;

typedef struct task
{
int start_row;
int num_of_rows;
} TASK;

int sigreply = 0;
int total = 0;

void my_handler(int signum)  // creating a signal handler for detecting signals for task from parent
{
sigreply = 1;
}

void my_handler2(int signum)  // creating exit status handler, to check if all childs terminate
{
printf("Process %d is interrupted by ^c. Bye Bye\n",getpid());
exit(total);
}

int main(int argc, char* args[])
{

//data structure to store the start and end times of the whole program
struct timespec start_time, end_time;
//get the start time
clock_gettime(CLOCK_MONOTONIC, &start_time);

//data structure to store the start and end times of the computation
struct timespec start_compute, end_compute;

//generate mandelbrot image and store each pixel for later display
//each pixel is represented as a value in the range of [0,1]

//store the 2D image as a linear array of pixels (in row-major format)
float * pixels;

//allocate memory to store the pixels
pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
if (pixels == NULL) {
    printf("Out of memory!!\n");
    exit(1);
}


printf("Start collection the image lines\n");
// Part 1b solution


int task[2];  // creating task pipe - boss
int pdf[2];  // creating child pipe - worker
pipe(pdf);
pipe(task);

int process_cnt = atoi(args[1]); // getting number of childs to be made
int task_cnt = atoi(args[2]); // getting the task count

int i;
pid_t pid;

int child_pids[process_cnt]; // store all chill processes in an array

// creating the child processes

 signal(SIGUSR1,my_handler);

for(i = 0; i < process_cnt; i++) // run a loop to create all child processes
{
    pid = fork();
    if(pid < 0)
        {
        printf("Error");
        exit(1);
        }
    else if(pid > 0) // store the children ID, useful for debugging and continue with loop
        {
        child_pids[i]=pid;
        }
    else if(pid == 0)  // if child is being created, break so that grand child is not created. We want to create direct children of parent only
        {
        break;
        }
}


if(pid==0)
{
    //child enters

    close(task[1]);
    close(pdf[0]);
    printf("Child (%d) Start up. Wait for task!\n", getpid()); // show which child process is running
    signal(SIGINT,my_handler2);  // calling the handler to check for signal

    while(1) // infinite while loop
        {
          while(!sigreply)
          {  //check for signal and continue if received else wait
         pause();
          }

    total++;  // add number of tasks, to be used to calculate how many tasks each child performs

    printf("Child (%d):Start the computation ...\n", getpid());

    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    int x, y;
    float difftime;
    struct timespec start_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &start_compute);

    float layer[IMAGE_WIDTH];
    TASK task_read;
    msg MSG; // creating an instance of the struct
    read(task[0],&task_read, sizeof(task_read)); // read from tasks pipe

    for (y=task_read.start_row; y<(task_read.start_row+task_read.num_of_rows)&&(y<IMAGE_HEIGHT); y++)  // run from start row to end row
    {
        for (x=0; x<IMAGE_WIDTH; x++)
        {
            //compute a value for each point c (x, y) in the complex plane
            layer[x] = Mandelbrot(x, y );  //run N/m rows

    	}

    	//write to struct and then pipe

        memcpy(MSG.rowdata,layer,sizeof(layer));  // copy the created array in the struct
        MSG.row_index = y;  // get the row index into the struct
        int z = task_read.start_row+task_read.num_of_rows-1;
        if((z == y) || (y == IMAGE_HEIGHT-1))  // check if it is the last row of the task or the last row of the file
          { MSG.child_pid = (int)getpid();}  // if yes then make child_pid equal to the childs pid to send back to the struct
        else
           {MSG.child_pid = 0;}  // else make it zero
    	write(pdf[1], &MSG, sizeof(MSG));  // write to the pipe
    	memset(layer,0,sizeof(layer));  //empty the layer array
    }

    clock_gettime(CLOCK_MONOTONIC, &end_compute);
	difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;  // calculate the time each child takes
	printf("Child (%d): ... completed. Elapse time = %.3f ms\n",getpid(), difftime);
    } // end while

}

if(pid>0)
{
    // parent enters

    float difftime = 0;
    close(task[0]);
    close(pdf[1]);
    printf("Start collecting image lines\n");

    TASK taskread;
    int k,startrow;
    for( k =0; k< process_cnt; k++)  // we go through all the child process to distribute tasks to them
        {
        taskread.num_of_rows = task_cnt;  // make each row equal to the task size
        taskread.start_row = taskread.num_of_rows*k;   // get the start row
        write(task[1],&taskread,sizeof(taskread));  // write to task pipe
        kill(child_pids[k],SIGUSR1); // send signal to child
        // waitpid(child_pids[k],NULL,0);
        startrow = taskread.start_row + taskread.num_of_rows; // update start row for next loop so that same row is not send twice
        }

    //  taskread.num_of_rows = task_cnt;
    //  taskread.start_row = taskread.num_of_rows*(k+1);
    int cnt1 = 0;
    msg red; // creating another instance for reading

    while(read(pdf[0],&red,sizeof(red))>0)  // read till pipe is empty
    {
    cnt1++;  // count rows
    for(int j=0;j<IMAGE_HEIGHT;j++)
        {
            int temp = j+ red.row_index*IMAGE_WIDTH;  // get the element of pixel, we are going through the array pixel by pixel
            pixels[temp]=red.rowdata[j]; // assign the value to each pixel from the struct
        }



        int cpid = red.child_pid; // storing the child id for use
     //print("hedgtHBDTHfth");

        if(cpid>0)  // if msg includes worker id and child finishes its task
        {
            taskread.num_of_rows=task_cnt;   // do the same as before to distribute tasks to the child
            taskread.start_row = startrow;
            startrow = taskread.num_of_rows + taskread.start_row;
            write(task[1],&taskread,sizeof(taskread));  // write to task pipe
            kill(cpid,SIGUSR1);
  		    //if(startrow==IMAGE_HEIGHT)
            if(cnt1==IMAGE_HEIGHT)  // if rows cross image height, stop reading from pipe and exit from while loop
                break;
        }

    }

    int terminate[process_cnt];
    for(int k =0; k< process_cnt; k++)  // we go through all the child process to stop them
    {
    kill(child_pids[k],SIGINT);  // send signal to child
    waitpid(child_pids[k],&terminate[k],0); // wait for it to finish
    }

    for(int k =0; k< process_cnt; k++)  // we go through all the child process to check for termination
    {
    int done = WEXITSTATUS(terminate[k]);
    printf("Child process %d terminated and completed %d tasks\n",child_pids[k],done);
    }

    printf("All Child processes have completed\n");

        //Report timing

    struct rusage useda,usedb;  // used to calculate the time spent by parent and child in user and systems mode

    getrusage(RUSAGE_CHILDREN, &useda);

    printf("Total time spent by all child processes in user mode = %ld ms\nTotal time spent by all child processes in system mode = %ld ms\n",
     ((useda.ru_utime).tv_sec*1000+(useda.ru_utime).tv_usec/1000),(useda.ru_stime).tv_sec*1000+(useda.ru_stime).tv_usec/1000);

    getrusage(RUSAGE_SELF, &usedb);

    printf("Total time spent by parent processes in user mode = %ld ms\nTotal time spent by parent processes in system mode = %ld ms\n",
      (usedb.ru_utime).tv_sec*1000+((usedb.ru_utime).tv_usec/1000),(usedb.ru_stime).tv_sec*1000+(usedb.ru_stime).tv_usec/1000);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
  	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
  	printf("Total elapse time measured by the process = %.3f ms\n", difftime);  // print total time

    printf("Draw the image\n");
    //Draw the image by using the SDL2 library
    DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
    }

return 0;
}
