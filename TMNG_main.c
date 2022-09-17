/*------------------------------------------------------------------------------
 Name        : TMNG_Main.c
 Authors     : Anees Ahmed Zuberi ,Khizar Akhtar , Nikhil Narayanan
 Version     :
 Copyright   : 
 Description : Main file
----------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>


#include "res_sensor.h"
#include "can_sensor.h"
#include "uimodule.h"
#include "data_transfer.h"


#define PORT 50012


unsigned int terminate;
pthread_mutex_t data_fifo_lock;
pthread_mutex_t zynq_lock;
unsigned int data_wr_count;
char data_fifo[100][50];
struct timespec init_time;
SensParam_struct PT100,CAN,PB,SW;
pthread_mutex_t board_lock;
char mode[7]="\0";

int connfd,fd;

void sighandler(int);

//The below function will be used to catch Ctrl+C and terminate safely

void sighandler(int signum){
   printf("\nCaught Ctrl+C signal %d, Program exit.\n", signum);
   terminate = 1;
   pthread_cond_signal(&PT100.cv);
   pthread_cond_signal(&CAN.cv);
   pthread_cond_signal(&PB.cv);
   pthread_cond_signal(&SW.cv);
   exit(1);
}

int main(){

    pthread_t res_sensor_thread_tID;
    pthread_t uimodule_thread_tID;
    pthread_t CANOpen_thread_tID;
    pthread_t data_transfer_thread_tID;
    pthread_t PB_transfer_thread_tID;
    pthread_t SW_transfer_thread_tID;

    int rc,sockfd,true;
    unsigned int len,cli;
    struct sockaddr_in servaddr;
    void *status;
    double start_time;
    terminate = 0;
    data_wr_count = 0;
    struct sched_param schedparm_main;

    memset(&schedparm_main, 0, sizeof(schedparm_main));
    schedparm_main.sched_priority = 1; // lowest rt priority
    sched_setscheduler(0, SCHED_FIFO, &schedparm_main);

    signal(SIGINT, sighandler);


    fd = open("/dev/meascdd", O_RDWR);
    if(fd < 0){
      perror("Error: Failed to open the device");
    } 

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
      printf("Error: Socket creation failed\n");
      exit(0);
    } else {
      printf("Info: Socket creation successful\n");
    }

    true = 1;
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) == -1)
    {
      printf("Error: Setting socket options failed\n");
      exit(0);
    }

    printf("Info: TcpServerF: %d\n", sockfd);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("172.20.48.31");
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
      printf("Error: Socket bind failed\n");
      exit(0);

    } else {
      printf("Info: Socket bind successful\n");
    }

    // Now server is ready to listen and verification
    if((listen(sockfd, 5)) != 0) {
      printf("Error: Listen failed\n");
      exit(0);
    } else {
      printf("Info: Server listening\n");
    }

    len = sizeof(cli);
    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);

    if(connfd < 0) {
      printf("Error: Server connection failed\n");
      exit(1);
    } else {
      printf("Info: Server connected to the client\n");
    }

    clock_gettime(CLOCK_REALTIME, &init_time);
    start_time = (double)(init_time.tv_sec * 1000) + (double)(init_time.tv_nsec/1000000);
    pthread_mutex_lock(&data_fifo_lock);
    sprintf(data_fifo[data_wr_count], "~%s: %f %s\n", "INIT", start_time, "0");
    data_wr_count++;
    pthread_mutex_unlock(&data_fifo_lock);

    /*-------------------------Creating Threads-------------------------*/
    //Launch resistive sensor data thread
    rc = pthread_create(&res_sensor_thread_tID, NULL, res_sensor,NULL);
    if(rc!= 0) {
    printf("Error: Failed to launch  resistive sensor thread\n");
    exit(-1);
    }
    
    //Launch uimodule thread 
    rc = pthread_create(&uimodule_thread_tID, NULL, uimodule, &connfd);
    if(rc != 0) {
    printf("Error: Failed to launch command sorter pthread_create(): %d\n", rc);
    exit(-1);
    }
    
    //Launch the CANOpen thread
	 rc = pthread_create(&CANOpen_thread_tID, NULL, CANOpen_thread,NULL);
	 if (rc != 0) {
	 printf("Failed to launch CANOpen pthread_create(): %d\n", rc);
	 exit(-1);
	 }

    //Launch the data transfer threads 
    rc = pthread_create(&data_transfer_thread_tID, NULL, data_transfer, NULL);
    if(rc != 0) {
      printf("ERROR; return code from data transfer is %d\n", rc);
      exit(-1);
    }

    //Launch the data transfer threads
    rc = pthread_create(&PB_transfer_thread_tID, NULL, PB_transfer, NULL);
    if(rc != 0) {
      printf("ERROR; return code from data transfer is %d\n", rc);
      exit(-1);
    }

    //Launch the data transfer threads
    rc = pthread_create(&SW_transfer_thread_tID, NULL, SW_transfer, NULL);
    if(rc != 0) {
      printf("ERROR; return code from data transfer is %d\n", rc);
      exit(-1);
    }

    /*----------Check for threads to join and terminate safely----------*/
    
    rc = pthread_join(res_sensor_thread_tID, &status);
    if(rc != 0) {
    printf("ERROR: return code from resistive pthread_join(): %d\n", rc);
    exit(-1);
    }
    
    rc = pthread_join(uimodule_thread_tID, &status);
    if(rc != 0) {
    printf("ERROR: return code from command sorter pthread_join(): %d\n", rc);
    exit(-1);
    }
    

    rc = pthread_join(CANOpen_thread_tID, &status);
    if (rc != 0) {
    printf("ERROR: return code from CANOpen pthread_join(): %d\n", rc);
    exit(-1);
    	 }


    rc = pthread_join(data_transfer_thread_tID, &status);
    if(rc != 0) {
      printf("ERROR: return code from cyclic pthread_join() is %d\n", rc);
      exit(-1);
    }
    
    rc = pthread_join(PB_transfer_thread_tID, &status);
    if(rc != 0) {
      printf("ERROR: return code from cyclic pthread_join() is %d\n", rc);
      exit(-1);
    }
    rc = pthread_join(SW_transfer_thread_tID, &status);
    if(rc != 0) {
      printf("ERROR: return code from cyclic pthread_join() is %d\n", rc);
      exit(-1);
    }

    /* Close TCP server socket */
    close(sockfd);
    printf("Info: TMNG program shutdown complete\n");
    return EXIT_SUCCESS;
}
