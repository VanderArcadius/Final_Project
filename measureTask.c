// Main and measureTask
#include <stdio.h>
#include "measureTask.h"
#include "dataPtrs.h"
#include "bool.h"
#include "systemTimeBase.h"
#include "Flags.h"
#include "FreeRTOS.h"
#include "task.h"
#include "adc.h"
#include "inc/hw_memmap.h"


void measure(void* data)
{
  for( ;; )
  {
    measureData2 * measureDataPtr = (measureData2*) data;
 
 
    measureTempArray(data);
    measurePRArray(data);
    
    //if sys/dia pressure interupt flagged
    if(*(*measureDataPtr).cuffPressRawPtr==1){
    //cuff pointer in range sys 50 -80
    if(*(*measureDataPtr).cuffPressRawPtr >=50 || *(*measureDataPtr).cuffPressRawPtr <=80){
      measureSysBPArray(data);
      ++(*(*measureDataPtr).sysCompletePtr);
      //remove flag
    }
    
   
    //if cuff raw in range110 -150
    if(*(*measureDataPtr).cuffPressRawPtr >=110 || *(*measureDataPtr).cuffPressRawPtr <=150){
      measureDiaBPArray(data);
      ++(*(*measureDataPtr).diaCompletePtr);
        
    }
        *(*measureDataPtr).cuffPressRawPtr=0;
    }
    /*Moved this to after the measurements so we start at index 0
    increment the count entry */
    ++(*(*measureDataPtr).countCallsPtr);
    
    vTaskResume(xComputeHandle);
    
    // Delay for 5 seconds
    vTaskDelay(5000);  
  }
}

/*
Function measureTempArray
Input pointer to measureData
Output Null
Do: updates the tempRaw based on algorithm
*/
void measureTempArray(void* data){
  measureData2* measureDataPtr = (measureData2*) data;
  
  //Creates a local pointer to the countCalls Variable
  unsigned int* countCalls = (*measureDataPtr).countCallsPtr;
  
  //Creates a local pointer to the start of the array
  unsigned int* tempRawBuf = (*measureDataPtr).temperatureRawBufPtr;
  
  //find the current index of the array based on call count. 
  unsigned int next = (*countCalls +1) %8;

  /* This array is used for storing the data read from the ADC FIFO. It
   must be as large as the FIFO for the sequencer in use.  This example
   uses sequence 3 which has a FIFO depth of 1.  If another sequence
   was used with a deeper FIFO, then the array size must be changed. */
  unsigned long ulADC0_Value[1];

  // Trigger the ADC conversion.
  ADCProcessorTrigger(ADC0_BASE, 3);

  // Wait for conversion to be completed.
  while(!ADCIntStatus(ADC0_BASE, 3, false))
  {
  }

  /* Clear the interrupt status flag.  This is done to make sure the
  interrupt flag is cleared before we sample. */
  ADCIntClear(ADC0_BASE, 3);

  // Read ADC Value.
  ADCSequenceDataGet(ADC0_BASE, 3, ulADC0_Value);

  // Use non-calibrated conversion provided in the data sheet.
  tempRawBuf[next] = (int)(147.5 - ((225 * ulADC0_Value[0]) / 1023));
};

/*
Function measureSysBp
Input pointer to measureData
Output Null
Do: Places Systolic into array indexes 0-7
*/
void measureSysBPArray(void* data){
    measureData2* measureDataPtr = (measureData2*) data;
   
    unsigned int* countCalls = (*measureDataPtr).sysCompletePtr;
    
    unsigned int* bloodPressRawBuf  = (*measureDataPtr).bloodPressRawBufPtr;
   
    unsigned int sysNext = (*countCalls +1) %8;
    
   bloodPressRawBuf[sysNext] = *(*measureDataPtr).cuffPressRawPtr;

};

/*
Function measureDiaBp
Input pointer to measureData
Output Null
Do: Places Systolic into array indexes 8-15
*/
void measureDiaBPArray(void* data){
  
    measureData2* measureDataPtr = (measureData2*) data;
    //unsigned int* countCalls = (*measureDataPtr).countCallsPtr;
    unsigned int* bloodPressRawBuf = (*measureDataPtr).bloodPressRawBufPtr;
      unsigned int* countCalls = (*measureDataPtr).diaCompletePtr;

  //unsigned int* sysComplete = (*measureDataPtr).sysCompletePtr;
    //unsigned int* diaComplete = (*measureDataPtr).diaCompletePtr;
    //unsigned int diaLast = ((*countCalls) %8) + 8;
    unsigned int diaNext = ((*countCalls +1) %8) + 8;

  bloodPressRawBuf[diaNext] = *(*measureDataPtr).cuffPressRawPtr;

};


/*
Function measurePrArray
Input pointer to measureData
Output Null
Do: Needs to be updated with the model transducer handling.
*/
void measurePRArray(void* data){
 int check=0;
    int beatCount=0;
    int bpm=0;
    int change =0;
    measureData2* measureDataPtr = (measureData2*) data;
    unsigned int clock = globalCounter;//(*measureDataPtr).globalCounterPtr;
    unsigned int temp=clock;
    while(clock<(temp+100)){
        
        clock=globalCounter;//(*measureDataPtr).globalCounterPtr;
        unsigned long* beat=(*measureDataPtr).prPtr;
        
        if(*beat==1 && check==0){
            beatCount++;
            check=1;
        }
        else if(*beat==0){
           check=0;
        }
    }
    
    bpm=(beatCount*60/3);
    
    unsigned int* countCalls = (*measureDataPtr).countCallsPtr;
    unsigned int* pulseRateRawBuf = (*measureDataPtr).pulseRateRawBufPtr;
    unsigned int prLast = (*countCalls) %8;
    unsigned int prNext = (*countCalls+1) %8;
    //Check to see if we prLast would cause divide by 0
    if(pulseRateRawBuf[prLast]!=0){
      change = ((pulseRateRawBuf[prLast]-bpm)*100)/(pulseRateRawBuf[prLast]);
    }
    //If the last value was 0, we shouuld have change be 0
    //Each measurement does not currently have its own counter 
    //so the display task only looks at an index across three
    else{
      change = 15;
    }
    if(change>=15||change<=-15){
        pulseRateRawBuf[prNext]=bpm;
    }

}
