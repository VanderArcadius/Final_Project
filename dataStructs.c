#include <stdio.h>
#include "bool.h"

//Change the structs to use arrays. bloodpress has both systolic and diastolic
typedef struct
{
  unsigned int temperatureRawBuf[8];
  unsigned int bloodPressRawBuf[16];
  unsigned int pulseRateRawBuf[8];
  unsigned int countCalls;
  unsigned int sysComplete;
  unsigned int diaComplete;
  int tempDirection;
  int prDirection;
  unsigned int cuffPressRaw;
  unsigned int* cuffFlag;
  signed int EKGRawBuf[256];
  unsigned int EKGFreqBuf[16];

} measurement2;
#define INIT_MEASUREMENT2(X) measurement2 X ={{36,NULL,NULL,NULL,NULL,NULL,NULL,NULL},{55,NULL,NULL,NULL,NULL,NULL,NULL,NULL,50,NULL,NULL,NULL,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL,NULL,NULL,NULL,NULL},0,0,0,1,1,50,0};

// Display struct
typedef struct
{
  unsigned char tempCorrectedBuf[8];
  unsigned char bloodPressCorrectedBuf[16];
  unsigned char pulseRateCorrectedBuf[8];
}display2;
#define INIT_DISPLAY2(X) display2 X ={NULL,NULL,NULL};

// Struct holding status task data
typedef struct
{
  unsigned short batteryState;
}status;
#define INIT_STATUS(X) status X ={200};

// Struct holding alarm data
typedef struct
{
  unsigned char bpOutOfRange;
  unsigned char tempOutOfRange;
  unsigned char pulseOutOfRange;
}alarms;
#define INIT_ALARMS(X) alarms X ={'\0','\0','\0'};

// Datastruct for warnings/alarms
typedef struct 
{
  Bool bpHigh;
  Bool tempHigh;
  Bool pulseLow;
  unsigned int led;
  unsigned long previousCount;
  const long pulseFlash;
  const long tempFlash;
  const long bpFlash;
  unsigned long auralCount;
}warning;
#define INIT_WARNING(X) warning X={FALSE,FALSE,FALSE,0,0,2000,1000,500,0};

// Datastruct for scheduler
typedef struct{
  unsigned int globalCounter;
}scheduler;
#define INIT_SCHEDULER(X) scheduler X={0};

// Datastruct for keypad data
typedef struct{
  unsigned short mode;
  unsigned short measurementSelection;
  unsigned short scroll;
  unsigned short selectChoice;
  unsigned short alarmAcknowledge;
}keypad;
#define INIT_KEYPAD(X) keypad X={0,0,0,1,0};

// Datastruct for remote communications data 
typedef struct{ 
  unsigned long ulIPAddress; 
  long lStringParam; 
  unsigned char pcDecodedString[24]; 
}remotecommunication; 
#define INIT_REMOTECOMMUNICATION(X) remotecommunication X={0,0,NULL}; 

// Datastruct for command data 
typedef struct{ 
  unsigned long lStringParam; 
  char commandBuf[24]; 
}command; 
#define INIT_COMMAND(X) command X={0,NULL}; 

// Struct for TCB's
typedef struct MyStruct 
{
    void (*taskPtr)(void*);
    void* taskDataPtr;
    struct MyStruct* next;
    struct MyStruct* prev;
}
TCB;

