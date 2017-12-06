#include "commandTask.h"
#include "dataStructs.c"
#include "dataPtrs.c"
#include "systemTimeBase.h"
#include "FreeRTOS.h"
#include "Flags.h"
#include "task.h"

/*accepted commands

I - Initialize network communications
S - Start mode, enable measurements and interrupts
P - Stop mode, terminate any measurement task
D - Enable or disable local display
M - Returns the most recent of all  measurement data
W - Returns most recent Warning or Alarm Data

E - error response
*/
 void command(void *data) {
  EKGData * EKGDataPtr = (EKGData*) data;
  signed int* EKGRawBuf = (*EKGDataPtr).EKGRawBufPtr;
  for( ;; )
  {
    //Generate the sine wave

      vTaskDelay(5000);
  }
  //return;

 }
	


 