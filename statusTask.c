#include "statusTask.h"
#include "dataStructs.c"
#include "dataPtrs.c"
#include "systemTimeBase.h"
#include "FreeRTOS.h"
#include "task.h"

 void stat(void *data) {

  for( ;; )
  {
     //printf("\n CHECKING STATUS! \n");
    // Recast task argument pointer to task’s data structure type
    statusData*word=(statusData*)data;
    unsigned short*temp3=(unsigned short*)(*word).batteryStatePtr;
   
    //Check if battery state is at 0, to prevent unwanted values being displayed   
    if (*temp3 !=0 )	 
    --*temp3;//= *temp3 - 1;
    //(*word).batteryStatePtr = (unsigned char *)(*temp3);
    vTaskDelay(5000);
  }
  //return;

 }
	
 
