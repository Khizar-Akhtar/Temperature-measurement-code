/*----------------------------------------------------------------------------
Name        :  data_transfer.c
Authors     :  Nikhil Narayanan
Version     : 
Copyright   : 
Description :  Cyclic thread for ethernet transmission, PB and SW
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <time.h>

#include "res_sensor.h"
#include "data_transfer.h"


#define BUFFER_SIZE 80
#define DATA_INTERVAL 1 //In milliseconds

void *data_transfer (void *arg)
{
    int rd_count;
    int tim_fd;
    struct itimerspec itval;
    unsigned long long missed;
    struct sched_param schedparm_data;

    memset(&schedparm_data, 0, sizeof(schedparm_data));
    schedparm_data.sched_priority = 1; // lowest rt priority
    sched_setscheduler(0, SCHED_FIFO, &schedparm_data);

    /* Create the timer */
    tim_fd = timerfd_create (CLOCK_MONOTONIC, 0);
    if(tim_fd == -1){
      perror("Error: creating timer.");
    }
    /* Make the timer periodic */
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = DATA_INTERVAL * 1000000;
    itval.it_value.tv_sec = 1;
    itval.it_value.tv_nsec = 0;
    timerfd_settime (tim_fd, 0, &itval, NULL);

    while (terminate == 0)
    {  
      read (tim_fd, &missed, sizeof (missed));
      if(data_wr_count > 0){
        //Lock the data fifo here
        pthread_mutex_lock(&data_fifo_lock);
        for(rd_count=0;rd_count < data_wr_count;rd_count++){
          write(connfd, data_fifo[rd_count], strlen(data_fifo[rd_count]));
        }
        data_wr_count = 0;
        //Unlock the data fifo here
        pthread_mutex_unlock(&data_fifo_lock);
      }
    }
    close(tim_fd);
    printf("Info: Data transfer thread dead.\n");
    pthread_exit(NULL);
    return NULL;
}





