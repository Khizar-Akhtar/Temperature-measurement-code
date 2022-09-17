/*
 * measdev.h
 *
 *  Created on: Oct 08, 2020
 *      Author: mueller
 */

#ifndef MEASDEV_H_
#define MEASDEV_H_

// MEAScdd IO data
typedef struct {
    unsigned int rnum;
    unsigned int rvalue;
}  MeasObj_struct;

typedef struct {
    float tvalue;
    int tv_sec;
    int tv_nsec;
}  TempV_struct;

#define MEASOBJ_SIZE sizeof(MeasObj_struct)
#define REGNUM_ID 0x0FF
#define NREGS 4

#define TIMER100ms  0x009896ff
#define MBINT_ENABLE 0x80000000
#define MBINT_ACKN 0x40000000

#endif /* MEASDEV_H_ */
