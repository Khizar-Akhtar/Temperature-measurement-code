/*----------------------------------------------------------------------------
Name        :  res_sensor.c
Authors     :  Nikhil Narayanan
Version     : 
Copyright   : 
Description :  Module for resistive sensor measurement 
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>

#include "uimodule.h"
#include "res_sensor.h"

//Debug prints enables(Only added for function calls)
#define DBG_PRINT_SUB 0
#define MAX31685_DELAY 63 //In milliseconds
#define SPI_DELAY 100 //In micro seconds

//Function to write the slave registers in IP
int reg_write(int reg,int value)
{
    int retc;
    static MeasObj_struct TF_Obj_Snd;
    TF_Obj_Snd.rnum = reg;
    TF_Obj_Snd.rvalue = value;
    retc = write(fd,&TF_Obj_Snd, MEASOBJ_SIZE);
    if(retc < 0){
      perror("Error: Failed to write message to device.\n");
      return errno;
    }
    return 0;
}

//Function to read the slave registers in IP
int reg_read(int reg)
{
    int retc;
    static MeasObj_struct TF_Obj_Snd, TF_Obj_Rcv;
    TF_Obj_Snd.rnum = REGNUM_ID;
    TF_Obj_Snd.rvalue = reg;
    retc = write(fd,&TF_Obj_Snd, MEASOBJ_SIZE);
    if(retc < 0){
      perror("Error: Failed to write message to device.\n");
      return errno;
    }
    retc = read(fd,&TF_Obj_Rcv, MEASOBJ_SIZE);
    if(retc < 0){
      perror("Error: Failed to read message from device.\n");
      return errno;
    }
    return TF_Obj_Rcv.rvalue;
}

//Function to write the registers in MAX device

int max_write(int reg,int value)
{
    unsigned int spi_data,status;
    struct itimerspec itval;
    unsigned long long missed;
    int tim_fd;

    spi_data = (0x80 | reg)<<8 | (value & 0xff);
    pthread_mutex_lock(&zynq_lock);
    reg_write(1,spi_data);
    pthread_mutex_unlock(&zynq_lock);
    //Create the timer
    tim_fd = timerfd_create(CLOCK_MONOTONIC,0);
    if(tim_fd == -1){
      perror("Error: creating timer.");
    }
    //Make the timer To execute after SPI_DELAY
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = SPI_DELAY * 1000;
    timerfd_settime (tim_fd, 0, &itval, NULL);
    read(tim_fd, &missed, sizeof (missed));
    pthread_mutex_lock(&zynq_lock);
    status = (reg_read(1)) & 0x01;
    pthread_mutex_unlock(&zynq_lock);
    close(tim_fd);
    if(status == 0){
      perror("Error: SPI not completed");
    }
    return 0;
}

int max_read(int reg)
{
    unsigned int spi_data,status;
    struct itimerspec itval;
    unsigned long long missed;
    int tim_fd;

    spi_data = reg<<8;
    pthread_mutex_lock(&zynq_lock);
    reg_write(1,spi_data);
    pthread_mutex_unlock(&zynq_lock);
    tim_fd = timerfd_create(CLOCK_MONOTONIC,0);
    if(tim_fd == -1){
      perror("Error: creating timer.");
    }
    //Make the timer To execute after SPI_DELAY
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = SPI_DELAY * 1000;
    timerfd_settime (tim_fd, 0, &itval, NULL);
    read(tim_fd, &missed, sizeof (missed));
    pthread_mutex_lock(&zynq_lock);
    status = (reg_read(1)) & 0x01;
    pthread_mutex_unlock(&zynq_lock);
    close(tim_fd);
    if(status == 0){
      perror("SPI not completed");
      return 0xffff;
    }else{
      pthread_mutex_lock(&zynq_lock);
      status = reg_read(2) & 0xff;
      pthread_mutex_unlock(&zynq_lock);
      return(status);
    }
}

// Function to read the registers in MAX device and calculate the temperature

void* res_sensor()
{
  int i,r_lsb,r_msb,r_reg_bit0,ADC_code,status,tim_fd;
  struct timespec meas_time;
  struct itimerspec itval;
  unsigned long long missed;
  TempV_struct res_meas;
  struct sched_param schedparm_resist;

  memset(&schedparm_resist, 0, sizeof(schedparm_resist));
  schedparm_resist.sched_priority = 1; // lowest rt priority
  sched_setscheduler(0, SCHED_FIFO, &schedparm_resist);

  max_write(0,0x081); //Power up the MAX31685
  while(terminate == 0){
    pthread_mutex_lock(&PT100.lock);
    pthread_cond_wait(&PT100.cv, &PT100.lock);
    pthread_mutex_unlock(&PT100.lock);
    if(terminate == 1){
      break;
    }
    for(i=1;i <= PT100.s_count;i++){
      if(strcmp(mode,"auto") == 0){
    	 i--;
      }
      clock_gettime(CLOCK_REALTIME, &meas_time);
      max_write(0,0x0a1);//Do a one shot conversion in MAX31685
      //Create the timer
      tim_fd = timerfd_create(CLOCK_MONOTONIC,0);
      if(tim_fd == -1){
    	perror("Error: creating timer.");
      }
      //Make the timer To execute after 63ms
      itval.it_interval.tv_sec = 0;
      itval.it_interval.tv_nsec = 0;

      if(strcmp(PT100.ts_unit,"s") == 0){
        itval.it_value.tv_sec = PT100.ts_sample;
        itval.it_value.tv_nsec = 0;
      }else if(strcmp(PT100.ts_unit,"ms") == 0){
        itval.it_value.tv_sec = 0;
        if(PT100.ts_sample < (MAX31685_DELAY)){
          itval.it_value.tv_nsec = (long int)(MAX31685_DELAY) * 1000000;
        }else{
    	  itval.it_value.tv_nsec = (long int)(PT100.ts_sample) * 1000000;
        }
      }
      timerfd_settime (tim_fd, 0, &itval, NULL);
      read(tim_fd, &missed, sizeof (missed));
      pthread_mutex_lock(&zynq_lock);
      status = (reg_read(1)) & 0x03;
      pthread_mutex_unlock(&zynq_lock);
      if(status != 1){
    	perror("Error: Conversion is not finished");
      }
      r_msb = max_read(1);
      if(r_msb == 0xffff){
    	perror("Error: Wait for MAX31865 timed out\n");
        continue;
      }
      r_msb &= 0xff;
      if(DBG_PRINT_SUB == 1){
    	printf("Info: Value read from register 0x01 of MAX device (MSB) = %x\n",r_msb);
      }
      r_lsb = max_read(2);
      if(r_msb == 0xffff){
    	perror("Error: Wait for MAX31865 timed out\n");
        continue;
      }
      r_lsb &= 0xff;
      if(DBG_PRINT_SUB == 1){
    	printf("Info: Value read from register 0x02 of MAX device (LSB) = %x\n",r_lsb);
      }
      r_reg_bit0 = r_lsb & 0x01;
      if(r_reg_bit0 == 1){
    	perror("Error: Faulty value read.\n");
      }else{
        ADC_code =  r_msb << 7 | r_lsb >> 1;
        pthread_mutex_lock(&data_fifo_lock);
        strcpy(res_meas.sname,"S3");
        //res_meas.tvalue = (ADC_code / 32.0) - 256.0;
        res_meas.tvalue = (ADC_code * 0.01220703125);
        res_meas.tv_msec = (double)(meas_time.tv_sec - init_time.tv_sec) * 1000000 + (double)(meas_time.tv_nsec - init_time.tv_nsec)/1000;
        sprintf(data_fifo[data_wr_count], "~%s: %.3f %.3f\n", res_meas.sname,res_meas.tv_msec,res_meas.tvalue);
        data_wr_count++;
        pthread_mutex_unlock(&data_fifo_lock);
      }
      close(tim_fd);
    }

  }
  max_write(0, 0x001);
  printf("Info: MAX31865 Power down\n");
  printf("Info: Resistive Sensor thread dead\n");
  pthread_exit(NULL);
  return NULL;

}

void *PB_transfer (void *arg)
{
    int i,tim_PB_fd;
    struct itimerspec itval;
    unsigned long long missed;
	unsigned int PB_value ;
	struct timespec meas_time;
	TempV_struct PB_meas;
	struct sched_param schedparm_PB;

	memset(&schedparm_PB, 0, sizeof(schedparm_PB));
	schedparm_PB.sched_priority = 1; // lowest rt priority
	sched_setscheduler(0, SCHED_FIFO, &schedparm_PB);

    while (terminate == 0)
    {  
      pthread_mutex_lock(&PB.lock);
      pthread_cond_wait(&PB.cv, &PB.lock);
      pthread_mutex_unlock(&PB.lock);
      if(terminate == 1){
        break;
      }
      /* Create the timer */
      tim_PB_fd = timerfd_create (CLOCK_MONOTONIC, 0);
      if(tim_PB_fd == -1){
    	perror("Error: creating timer.");
      }
      /* Make the timer periodic */
      if(strcmp(PB.ts_unit,"s") == 0){
        itval.it_interval.tv_sec = PB.ts_sample;
        itval.it_interval.tv_nsec = 0;
      }else if(strcmp(PB.ts_unit,"ms") == 0){
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = (long int)PB.ts_sample * 1000000;
      }else if(strcmp(PB.ts_unit,"us") == 0){
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = (long int)PB.ts_sample * 1000;
      }
      itval.it_value.tv_sec = 1;
      itval.it_value.tv_nsec = 0;
      timerfd_settime (tim_PB_fd, 0, &itval, NULL);

      for(i=1;i<=PB.s_count;i++){
        if(strcmp(mode,"auto") == 0){
           i--;
        }
        read(tim_PB_fd, &missed, sizeof (missed));
		clock_gettime(CLOCK_REALTIME, &meas_time);
	    pthread_mutex_lock(&zynq_lock);
		PB_value = (reg_read(0) & 0x1f00) >> 8;
	    pthread_mutex_unlock(&zynq_lock);
        //Lock the data fifo here
        pthread_mutex_lock(&data_fifo_lock);
        strcpy(PB_meas.sname,"S1");
        PB_meas.tvalue = PB_value;
        PB_meas.tv_msec = (double)(meas_time.tv_sec - init_time.tv_sec) * 1000000 + (double)(meas_time.tv_nsec - init_time.tv_nsec)/1000;
        sprintf(data_fifo[data_wr_count], "~%s: %.3f %.3f\n", PB_meas.sname, PB_meas.tv_msec,PB_meas.tvalue);
		data_wr_count++;
        //Unlock the data fifo here
        pthread_mutex_unlock(&data_fifo_lock);
      }
      close(tim_PB_fd);
    }
    printf("Info: Push button thread dead.\n");
    pthread_exit(NULL);
    return NULL;
}

void *SW_transfer (void *arg)
{
    int i,tim_SW_fd;
    struct itimerspec itval;
    unsigned long long missed;
	unsigned int SW_value ;
	struct timespec meas_time;
	TempV_struct SW_meas;
	struct sched_param schedparm_SW;

	memset(&schedparm_SW, 0, sizeof(schedparm_SW));
	schedparm_SW.sched_priority = 1; // lowest rt priority
	sched_setscheduler(0, SCHED_FIFO, &schedparm_SW);

	while (terminate == 0)
    {  
      pthread_mutex_lock(&SW.lock);
      pthread_cond_wait(&SW.cv, &SW.lock);
      pthread_mutex_unlock(&SW.lock);
      if(terminate == 1){
        break;
      }
      /* Create the timer */
      tim_SW_fd = timerfd_create (CLOCK_MONOTONIC, 0);
      if(tim_SW_fd == -1){
    	perror("Error: Creating timer failed.");
      }
      /* Make the timer periodic */
      if(strcmp(SW.ts_unit,"s") == 0){
        itval.it_interval.tv_sec = SW.ts_sample;
        itval.it_interval.tv_nsec = 0;
      }else if(strcmp(SW.ts_unit,"ms") == 0){
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = (long int)SW.ts_sample * 1000000;
      }else if(strcmp(SW.ts_unit,"us") == 0){
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = (long int)SW.ts_sample * 1000;
      }
      itval.it_value.tv_sec = 1;
      itval.it_value.tv_nsec = 0;
      timerfd_settime (tim_SW_fd, 0, &itval, NULL);

      for(i = 1;i <= SW.s_count;i++){
        if(strcmp(mode,"auto") == 0){
           i--;
        }
        read(tim_SW_fd, &missed, sizeof (missed));
		clock_gettime(CLOCK_REALTIME, &meas_time);
	    pthread_mutex_lock(&zynq_lock);
		SW_value = reg_read(0) & 0xff;
	    pthread_mutex_unlock(&zynq_lock);
        //Lock the data fifo here
        pthread_mutex_lock(&data_fifo_lock);
        strcpy(SW_meas.sname,"S0");
        SW_meas.tvalue = SW_value;
        SW_meas.tv_msec = (double)(meas_time.tv_sec - init_time.tv_sec) * 1000000 + (double)(meas_time.tv_nsec - init_time.tv_nsec)/1000;
        sprintf(data_fifo[data_wr_count], "~%s: %.3f %.3f\n", SW_meas.sname,SW_meas.tv_msec,SW_meas.tvalue);
		data_wr_count++;
        //Unlock the data fifo here
        pthread_mutex_unlock(&data_fifo_lock);
      }
      close(tim_SW_fd);
    }
    printf("Info: Switches thread dead.\n");
    pthread_exit(NULL);
    return NULL;
}

