/*----------------------------------------------------------------------------
Name        :  res_sensor.c
Authors     :  Nikhil Narayanan
Version     : 
Copyright   : 
Description :  Header file for res_sensor.c module
----------------------------------------------------------------------------*/

#ifndef res_sensor_H_
#define res_sensor_H_

// IO data structure for board
typedef struct {
    unsigned int rnum;
    unsigned int rvalue;
}  MeasObj_struct;
// Data structure for temperature measurement
typedef struct {
    char sname[3];
    double tvalue;
    double tv_msec;
}  TempV_struct;

typedef struct {
    int s_count;
    int ts_sample;
    pthread_cond_t cv;
    pthread_mutex_t lock;
    char ts_unit[2];
}  SensParam_struct;



#define MEASOBJ_SIZE sizeof(MeasObj_struct)
#define REGNUM_ID 0x0FF
#define NREGS 4


extern unsigned int terminate;
extern int fd;
extern unsigned int data_wr_count;
extern char data_fifo[100][50];
extern pthread_mutex_t data_fifo_lock;
extern struct timespec init_time;
extern SensParam_struct PT100,CAN,PB,SW;
extern char mode[7];
extern pthread_mutex_t zynq_lock;


int reg_write(int reg,int value);
int reg_read(int reg);
int max_write(int reg,int value);
int max_read(int reg);

void* res_sensor();
void* PB_transfer();
void* SW_transfer();
 
#endif 
