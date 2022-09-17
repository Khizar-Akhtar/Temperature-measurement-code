/*----------------------------------------------------------------------------
Name        :  uimodule.c
Authors     :  Khizar Akhtar,Nikhil Narayanan
Version     : 
Copyright   : 
Description :  User command parsing thread 
----------------------------------------------------------------------------*/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "res_sensor.h"
#include "uimodule.h"

#define BUFFER_SIZE 80

void* uimodule()

{
    char buff[BUFFER_SIZE],text_inp[BUFFER_SIZE],arg3[2];
    int arg1,arg2;
    struct sched_param schedparm_ui;

    memset(&schedparm_ui, 0, sizeof(schedparm_ui));
    schedparm_ui.sched_priority = 1; // lowest rt priority
    sched_setscheduler(0, SCHED_FIFO, &schedparm_ui);

    while (terminate == 0) 
    {
      strcpy(text_inp,"\0"); // copy parameter from client to the text_inp
      arg1 = 0;
      arg2 = 0;
      strcpy(arg3,"\0");

      terminate = 0;
      bzero(buff, BUFFER_SIZE); // empty buffer
      read(connfd, buff, sizeof(buff));
      //printf("\n %s",buff);
      sscanf(buff, "%s %d %d %s", text_inp, &arg1, &arg2, arg3);
      if(strcmp(text_inp,"exit") == 0){
        terminate = 1;
        pthread_cond_signal(&PT100.cv); // The pthread_cond_signal() wake up threads waiting for the condition variable. on success 0 is returned
        pthread_cond_signal(&CAN.cv); //why waking threads here??
        pthread_cond_signal(&PB.cv);
        pthread_cond_signal(&SW.cv);
        break;
      }else if(strcmp(text_inp,"help") == 0){
        printf("--- Please sent one of the following options ---\n");
        printf("--- Sensor ID - iterations - sampling period  ---\n");
        printf("--- To exit - exit ---\n");
      }
      else if(strcmp(text_inp,"auto")==0){
    	printf("Auto mode enabled.\n");
    	strcpy(mode,"auto");  
    	PT100.s_count = 1;
    	CAN.s_count = 1;
    	PB.s_count = 1;
    	SW.s_count = 1;
    	pthread_cond_signal(&PT100.cv);
    	pthread_cond_signal(&CAN.cv);
    	pthread_cond_signal(&SW.cv);
    	pthread_cond_signal(&PB.cv);
      }
      else if(strcmp(text_inp,"manual")==0){
    	printf("Auto mode stopped.\n");
    	strcpy(mode,"manual");
    	PT100.s_count = 0;
    	CAN.s_count = 0;
    	PB.s_count = 0;
    	SW.s_count = 0;
      }
      else if(strcmp(text_inp,"S3") == 0){
        if(sscanf(buff, "%s %d %d %s", text_inp, &arg1, &arg2, arg3) != 4){
        	printf("Error: Please enter a valid count for iterations or sampling period and unit and try again.\n");
          printf("Info: Values %s %d %d %s\n" ,text_inp, arg1,arg2,arg3 );
        }else if((arg1 == 0)||(arg2 == 0)){
          printf("Error: Iterations and sampling rate should be non zero.\n");
        }else{
          PT100.s_count = arg1;
          PT100.ts_sample = arg2;
          strcpy(PT100.ts_unit,arg3);
          pthread_cond_signal(&PT100.cv);
          printf("Info: S3 Sample Count: %d Sampling Rate: %d\n",PT100.s_count,PT100.ts_sample);
        }
      }else if(strcmp(text_inp,"S2") == 0){
        if(sscanf(buff, "%s %d %d %s", text_inp, &arg1, &arg2, arg3) != 4){
        	printf("Error: Please enter a valid count for iterations or sampling period and unit and try again.\n");
          printf("Info: Values %s %d %d %s\n" ,text_inp, arg1,arg2,arg3 );

        }else if((arg1 == 0)||(arg2 == 0)){
            printf("Error: Iterations and sampling rate should be non zero.\n");
        }else{
          CAN.s_count = arg1;
          CAN.ts_sample = arg2;
          strcpy(CAN.ts_unit,arg3);
          pthread_cond_signal(&CAN.cv);
          printf("Info: S2 Sample Count: %d Sampling Rate: %d\n" ,CAN.s_count, CAN.ts_sample );
        }  
      }else if(strcmp(text_inp,"S1") == 0){
        if(sscanf(buff, "%s %d %d %s", text_inp, &arg1, &arg2, arg3) != 4){
        	printf("Error: Please enter a valid count for iterations or sampling period and unit and try again.\n");
          printf("Info: Values %s %d %d %s\n" ,text_inp, arg1,arg2,arg3 );

        }else if((arg1 == 0)||(arg2 == 0)){
            printf("Error: Iterations and sampling rate should be non zero.\n");
        }else{
          PB.s_count = arg1;
          PB.ts_sample = arg2;
          strcpy(PB.ts_unit,arg3);
          pthread_cond_signal(&PB.cv);
          printf("Info: S1 Sample Count: %d Sampling Rate: %d \n" ,PB.s_count, PB.ts_sample);
        } 
      }else if(strcmp(text_inp,"S0") == 0){
        if(sscanf(buff, "%s %d %d %s", text_inp, &arg1, &arg2, arg3) != 4){
          printf("Error: Please enter a valid count for iterations or sampling period and unit and try again.\n");
          printf("Info: Values %s %d %d %s\n" ,text_inp, arg1,arg2,arg3 );

        }else if((arg1 == 0)||(arg2 == 0)){
            printf("Error: Iterations and sampling rate should be non zero.\n");
        }else{
          SW.s_count = arg1;
          SW.ts_sample = arg2;
          strcpy(SW.ts_untit,arg3);
          pthread_cond_signal(&SW.cv);
          printf("Info: S0 Sample Count: %d Sampling Rate: %d\n" ,SW.s_count, SW.ts_sample );
        } 
      }else{
        printf("Error: Invalid command.\n");
      }
    }
    printf("Info: UImodule thread dead\n");
    pthread_exit(NULL);
    return NULL;
}
