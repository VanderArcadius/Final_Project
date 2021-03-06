/* Set the following option to 1 to include the WEB server in the build.  By
default the WEB server is excluded to keep the compiled code size under the 32K
limit imposed by the KickStart version of the IAR compiler.  The graphics
libraries take up a lot of ROM space, hence including the graphics libraries
and the TCP/IP stack together cannot be accommodated with the 32K size limit. */
#define mainINCLUDE_WEB_SERVER		0

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Hardware library includes. */
#include "hw_sysctl.h"
#include "grlib.h"
#include "rit128x96x4.h"
#include "osram128x64x4.h"
#include "formike128x128x16.h"
#include "adc.h"

/* Demo app includes. */
#include "partest.h"
#include "lcd_message.h"
#include "bitmap.h"

/* Project 4 & 5 includes */
#include "inc/hw_types.h"
#include "computeTask.h"
#include "dataPtrs.h"
#include "displayTask.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/pwm.h"
#include "drivers/rit128x96x4.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "dataStructs.c"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "measureTask.h"
#include "serialComTask.h"
#include "systemTimeBase.h"
#include "warningAlarm.h"
#include "Flags.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "httpserver_raw/httpd.h"
#include "drivers/rit128x96x4.h"
#include "io.h"
#include "cgifuncs.h"
#include "driverlib/ethernet.h"
#include "driverlib/flash.h"
#include "inc/hw_nvic.h"
#include "lwip/opt.h"
#include "semphr.h"
#include <time.h>

#define CLOCK_RATE      300


/*-----------------------------------------------------------*/

/* The time between cycles of the 'check' functionality (defined within the
tick hook. */
#define mainCHECK_DELAY						( ( TickType_t ) 5000 / portTICK_PERIOD_MS )

/* Size of the stack allocated to the uIP task. */
#define mainBASIC_WEB_STACK_SIZE            ( configMINIMAL_STACK_SIZE * 3 )

/* The OLED task uses the sprintf function so requires a little more stack too. */
#define mainOLED_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE + 50 )

/* Task priorities. */
#define mainQUEUE_POLL_PRIORITY				( tskIDLE_PRIORITY + 2 )
#define mainCHECK_TASK_PRIORITY				( tskIDLE_PRIORITY + 3 )
#define mainSEM_TEST_PRIORITY				( tskIDLE_PRIORITY + 1 )
#define mainBLOCK_Q_PRIORITY				( tskIDLE_PRIORITY + 2 )
#define mainCREATOR_TASK_PRIORITY           ( tskIDLE_PRIORITY + 3 )
#define mainINTEGER_TASK_PRIORITY           ( tskIDLE_PRIORITY )
#define mainGEN_QUEUE_TASK_PRIORITY			( tskIDLE_PRIORITY )

/* The maximum number of message that can be waiting for display at any one
time. */
#define mainOLED_QUEUE_SIZE					( 3 )

/* Dimensions the buffer into which the jitter time is written. */
#define mainMAX_MSG_LEN						25

/* The period of the system clock in nano seconds.  This is used to calculate
the jitter time in nano seconds. */
#define mainNS_PER_CLOCK					( ( unsigned long ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )

/* Constants used when writing strings to the display. */
#define mainCHARACTER_HEIGHT				( 9 )
#define mainMAX_ROWS_128					( mainCHARACTER_HEIGHT * 14 )
#define mainMAX_ROWS_96						( mainCHARACTER_HEIGHT * 10 )
#define mainMAX_ROWS_64						( mainCHARACTER_HEIGHT * 7 )
#define mainFULL_SCALE						( 15 )
#define ulSSI_FREQUENCY						( 3500000UL )

/*-----------------------------------------------------------*/

// Global counters
unsigned volatile int globalCounter = 0;
unsigned int auralCounter = 0;
unsigned int pulseFreq=4;
unsigned int pulseCount=0;
unsigned long g_ulFlagPR=0;
unsigned int ekgCounter = 0;

//*****************************************************************************
//
// Flags that contain the current value of the interrupt indicator as displayed
// on the OLED display.
//
//*****************************************************************************
unsigned long g_ulFlags;
unsigned long auralFlag;
unsigned long ackFlag = 0;
unsigned long computeFlag;
unsigned long serialFlag;
unsigned int tempFlag = 0;
unsigned int diaFlag = 0;
unsigned int sysFlag = 0;
unsigned int pulseFlag = 0;
unsigned int iFlag = 1;
unsigned int commandErrorFlag = 0;
TaskHandle_t xComputeHandle;
TaskHandle_t xEKGHandle;
TaskHandle_t xDisplayHandle;
TaskHandle_t xTempHandle;
TaskHandle_t xCommandHandle; 
TaskHandle_t xMeasureHandle; 

//*****************************************************************************
//
// A set of flags used to track the state of the application.
//
//*****************************************************************************

//extern unsigned long g_ulFlags;
#define FLAG_CLOCK_TICK         0           // A timer interrupt has occurred
#define FLAG_CLOCK_COUNT_LOW    1           // The low bit of the clock count
#define FLAG_CLOCK_COUNT_HIGH   2           // The high bit of the clock count
#define FLAG_UPDATE             3           // The display should be updated
#define FLAG_BUTTON             4           // Debounced state of the button
#define FLAG_DEBOUNCE_LOW       5           // Low bit of the debounce clock
#define FLAG_DEBOUNCE_HIGH      6           // High bit of the debounce clock
#define FLAG_BUTTON_PRESS       7           // The button was just pressed
#define FLAG_ENET_RXPKT         8           // An Ethernet Packet received
#define FLAG_ENET_TXPKT         9           // An Ethernet Packet transmitted

//*****************************************************************************
//
// The speed of the processor.
//
//*****************************************************************************

unsigned long g_ulSystemClock;

//*****************************************************************************
//
// The debounced state of the five push buttons.  The bit positions correspond
// to:
//
//     0 - Up
//     1 - Down
//     2 - Left
//     3 - Right
//     4 - Select
//
//*****************************************************************************

unsigned char g_ucSwitches = 0x1f;

//*****************************************************************************
//
// The vertical counter used to debounce the push buttons.  The bit positions
// are the same as g_ucSwitches.
//
//*****************************************************************************

static unsigned char g_ucSwitchClockA = 0;
static unsigned char g_ucSwitchClockB = 0;

//*****************************************************************************
// Defines for setting up the system clock.
//*****************************************************************************

#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)
#define SYSTICKUS               (1000000 / SYSTICKHZ)
#define SYSTICKNS               (1000000000 / SYSTICKHZ)

//*****************************************************************************
// A set of flags.  The flag bits are defined as follows:
//     0 -> An indicator that a SysTick interrupt has occurred.
//*****************************************************************************

#define FLAG_SYSTICK            0
//static volatile unsigned long g_ulFlags;

//*****************************************************************************
// External Application references.
//*****************************************************************************

extern void httpd_init(void);

//*****************************************************************************
// SSI tag indices for each entry in the g_pcSSITags array.
//*****************************************************************************

#define SSI_INDEX_TEMPSTATE  0
#define SSI_INDEX_SYSSTATE  1
#define SSI_INDEX_DIASTATE   2
#define SSI_INDEX_PULSESTATE   3
#define SSI_INDEX_EKGSTATE      4
#define SSI_INDEX_BATTERYSTATE  5
#define SSI_INDEX_ERRORSTATE    6

//*****************************************************************************
// This array holds all the strings that are to be recognized as SSI tag
// names by the HTTPD server.  The server will call SSIHandler to request a
// replacement string whenever the pattern <!--#tagname--> (where tagname
// appears in the following array) is found in ".ssi", ".shtml" or ".shtm"
// files that it serves.
//*****************************************************************************
static const char *g_pcConfigSSITags[] =
{
    "TEMPtxt",        // SSI_INDEX_TEMPSTATE
    "SYStxt",        // SSI_INDEX_SYSSTATE
    "DIAtxt",       // SSI_INDEX_DIASTATE
    "PULSEtxt",       // SSI_INDEX_PULSESTATE
    "EKGtxt",         // SSI_INDEX_EKGSTATE
    "BATtxt",       // SSI_INDEX_BATSTATE
    "ERRtxt"         // SSI_INDEX_ERRORSTATE
};

//*****************************************************************************
//! The number of individual SSI tags that the HTTPD server can expect to
//! find in our configuration pages.
//*****************************************************************************
#define NUM_CONFIG_SSI_TAGS     (sizeof(g_pcConfigSSITags) / sizeof (char *))

//*****************************************************************************
//! Prototypes for the various CGI handler functions.
//*****************************************************************************

static char *SetTextCGIHandler(int iIndex, int iNumParams, char *pcParam[],
                              char *pcValue[]);

//*****************************************************************************
//! Prototype for the main handler used to process server-side-includes for the
//! application's web-based configuration screens.
//*****************************************************************************

static int SSIHandler(int iIndex, char *pcInsert, int iInsertLen);

//*****************************************************************************
// CGI URI indices for each entry in the g_psConfigCGIURIs array.
//*****************************************************************************
#define CGI_INDEX_CONTROL       0
#define CGI_INDEX_TEXT          1
#define CGI_INDEX_DATA          2

//*****************************************************************************
//! This array is passed to the HTTPD server to inform it of special URIs
//! that are treated as common gateway interface (CGI) scripts.  Each URI name
//! is defined along with a pointer to the function which is to be called to
//! process it.
//*****************************************************************************
static const tCGI g_psConfigCGIURIs[] =
{
    { "/settxt.cgi", SetTextCGIHandler }          // CGI_INDEX_TEXT       
};

//*****************************************************************************
//! The number of individual CGI URIs that are configured for this system.
//*****************************************************************************

#define NUM_CONFIG_CGI_URIS     (sizeof(g_psConfigCGIURIs) / sizeof(tCGI))

//*****************************************************************************
//! The file sent back to the browser by default following completion of any
//! of our CGI handlers.  Each individual handler returns the URI of the page
//! to load in response to it being called.
//*****************************************************************************
#define DEFAULT_CGI_RESPONSE    "/patient_data.ssi"

//*****************************************************************************
//! The file sent back to the browser in cases where a parameter error is
//! detected by one of the CGI handlers.  This should only happen if someone
//! tries to access the CGI directly via the broswer command line and doesn't
//! enter all the required parameters alongside the URI.
//*****************************************************************************

#define PARAM_ERROR_RESPONSE    "/perror.htm"

#define JAVASCRIPT_HEADER                                                     \
    "<script type='text/javascript' language='JavaScript'><!--\n"

#define JAVASCRIPT_FOOTER                                                     \
    "//--></script>\n"

//*****************************************************************************
// Timeout for DHCP address request (in seconds).
//*****************************************************************************

#ifndef DHCP_EXPIRE_TIMER_SECS
#define DHCP_EXPIRE_TIMER_SECS  45
#endif

//*****************************************************************************
// The error routine that is called if the driver library encounters an error.
//*****************************************************************************

#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)

{

}
#endif

//  Declare the globals
INIT_MEASUREMENT2(m2);
INIT_DISPLAY2(d2);
INIT_STATUS(s1);
INIT_ALARMS(a1);
INIT_WARNING(w1);
INIT_SCHEDULER(c1);
INIT_KEYPAD(k1);
INIT_REMOTECOMMUNICATION(r1); 
INIT_COMMAND(co); 

//Connect pointer structs to data
measureData2 mPtrs2 = 
{     
  m2.temperatureRawBuf,
  m2.bloodPressRawBuf,
  m2.pulseRateRawBuf,
  &m2.countCalls,
  &m2.sysComplete,
  &m2.diaComplete,
  &m2.tempDirection,
  &g_ulFlagPR,
  &m2.cuffPressRaw,
  &m2.cuffFlag
};

computeData2 cPtrs2=
{
  m2.temperatureRawBuf,
  m2.bloodPressRawBuf,
  m2.pulseRateRawBuf,
  d2.tempCorrectedBuf,
  d2.bloodPressCorrectedBuf,
  d2.pulseRateCorrectedBuf,
  &k1.measurementSelection,
  &m2.countCalls
};

displayData2 dPtrs2=
{
  d2.tempCorrectedBuf,
  d2.bloodPressCorrectedBuf,
  d2.pulseRateCorrectedBuf,
  &s1.batteryState,
  &m2.countCalls,
  &k1.mode,
  &a1.tempOutOfRange,
  &a1.bpOutOfRange,
  &a1.pulseOutOfRange,
  m2.EKGFreqBuf
  
};

warningAlarmData2 wPtrs2=
{
  m2.temperatureRawBuf,
  m2.bloodPressRawBuf,
  m2.pulseRateRawBuf,
  &s1.batteryState,
  &a1.bpOutOfRange,
  &a1.tempOutOfRange,
  &a1.pulseOutOfRange,
  &w1.bpHigh,
  &w1.tempHigh,
  &w1.pulseLow,
  &w1.led,
  &m2.countCalls,
  &w1.previousCount,
  &w1.pulseFlash,
  &w1.tempFlash,
  &w1.bpFlash,
  &w1.auralCount
};

keypadData kPtrs=
{
  &k1.mode,
  &k1.measurementSelection,
  &k1.scroll,
  &k1.selectChoice,
  &k1.alarmAcknowledge,
  &m2.cuffPressRaw,
  &m2.cuffFlag
};

EKGData ecPtrs=
{
  &m2.EKGRawBuf,
  &m2.EKGFreqBuf

};

statusData sPtrs=
{  
  &s1.batteryState
};

schedulerData schedPtrs=
{
  &c1.globalCounter
};

communicationsData comPtrs={
  d2.tempCorrectedBuf,
  d2.bloodPressCorrectedBuf,
  d2.pulseRateCorrectedBuf,
  &s1.batteryState,
  &m2.countCalls
};

remCommData rPtrs={ 
   d2.tempCorrectedBuf, 
   d2.bloodPressCorrectedBuf, 
   d2.pulseRateCorrectedBuf, 
   &s1.batteryState, 
   &m2.countCalls 
 }; 

commandData coPtrs = {
  &co.lStringParam,
  co.commandBuf
};

//Declare the prototypes for the tasks
void compute(void* data);
void measure(void* data);
void stat(void* data);
void alarm(void* data);
void disp(void* data);
void schedule(void* data);
void keypadfunction(void* data);
void ekgCapture(void* data);
void ekgProcess(void* data);
void remoteCommunications(void *data);
void commandFunction(void *data);
void startup();
void initializeNetwork();

/*
 * The task that handles the uIP stack.  All TCP/IP processing is performed in
 * this task.
 */
extern void vuIP_Task( void *pvParameters );

/*
 * The display is written two by more than one task so is controlled by a
 * 'gatekeeper' task.  This is the only task that is actually permitted to
 * access the display directly.  Other tasks wanting to display a message send
 * the message to the gatekeeper.
 */
static void vOLEDTask( void *pvParameters );

/*
 * Configure the hardware for the demo.
 */
static void prvSetupHardware( void );

/*
 * Configures the high frequency timers - those used to measure the timing
 * jitter while the real time kernel is executing.
 */
extern void vSetupHighFrequencyTimer( void );

/*
 * Hook functions that can get called by the kernel.
 */
void vApplicationStackOverflowHook( TaskHandle_t *pxTask, signed char *pcTaskName );
void vApplicationTickHook( void );

/* The queue used to send messages to the OLED task. */
QueueHandle_t xOLEDQueue;

/* The welcome text. */
const char * const pcWelcomeMessage = "   www.FreeRTOS.org";

/* Task Prototypes */
void vTask1(void *vParameters);

//*****************************************************************************
// This CGI handler is called whenever the web browser requests settxt.cgi.
//*****************************************************************************

static char *
SetTextCGIHandler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    long lStringParam;
    int len;
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    // Find the parameter that has the string we need to display.
    lStringParam = FindCGIParameter("DispText", pcParam, iNumParams);

    // If the parameter was not found, show the error page.
    if(lStringParam == -1)
    {
        return(PARAM_ERROR_RESPONSE);
    }

    // The parameter is present. We need to decode the text for display.
    DecodeFormString(pcValue[lStringParam], co.commandBuf, 24);//co.commandBuf

    // Get the length of the string
    len = strlen(co.commandBuf);
    if(len == 1)
    {
      //if(xSemaphore != NULL)

      //{

      //vTaskResume(xCommandHandle);

      //xTaskCreate(commandFunction, "Command Task", 200, (void*)&coPtrs, 1, &xCommandHandle);

      //xSemaphoreGive( xSemaphore, &xHigherPriorityTaskWoken );

      //portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

      //}
    }

    // Erase the previous string and overwrite it with the new one.
    //
    //RIT128x96x4StringDraw("                      ", 0, 64, 12);
    //RIT128x96x4StringDraw(co.commandBuf, 0, 64, 12);

    // Tell the HTTPD server which file to send back to the client.
    return("/send_command.ssi");
}



//*****************************************************************************

//

// This function is called by the HTTP server whenever it encounters an SSI

// tag in a web page.  The iIndex parameter provides the index of the tag in

// the g_pcConfigSSITags array. This function writes the substitution text

// into the pcInsert array, writing no more than iInsertLen characters.

//

//*****************************************************************************

static int
SSIHandler(int iIndex, char *pcInsert, int iInsertLen)
{
    // Which SSI tag have we been passed?
    switch(iIndex)
    {
        case SSI_INDEX_TEMPSTATE:
          if(*(dPtrs2.tempOutOfRangePtr) == 'Y')
          {
            if(tempFlag == 0)
            {
            usnprintf(pcInsert, iInsertLen, "Temperature: %d C",dPtrs2.tempCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
            tempFlag = 1;
            }
            else
            {
              usnprintf(pcInsert, iInsertLen, "Temperature: __ C");
              tempFlag = 0;
            }
          
          }
          else if (*(dPtrs2.tempOutOfRangePtr) == 'N')
          {
            usnprintf(pcInsert, iInsertLen, "Temperature: %d C",dPtrs2.tempCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
          }
          
            break;

        case SSI_INDEX_SYSSTATE:
          if(*(dPtrs2.bpOutOfRangePtr) == 'Y')
          {
            if(sysFlag == 0)
            {
            usnprintf(pcInsert, iInsertLen, "Systolic Pressure: %d mm Hg",dPtrs2.bloodPressCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
            sysFlag = 1;
            }
            else
            {
              usnprintf(pcInsert, iInsertLen, "Systolic Pressure: ___ mm Hg");
              sysFlag = 0;
            }
          }
          else if (*(dPtrs2.bpOutOfRangePtr) == 'N')
          {
            usnprintf(pcInsert, iInsertLen, "Systolic Pressure: %d mm Hg",dPtrs2.bloodPressCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
          }
            
            break;

        case SSI_INDEX_DIASTATE:
          if(*(dPtrs2.bpOutOfRangePtr) == 'Y')
          {
            if(diaFlag == 0)
            {
            usnprintf(pcInsert, iInsertLen, "Diastolic Pressure: %d mm Hg",dPtrs2.bloodPressCorrectedBufPtr[((*(dPtrs2.countCallsPtr)) % 8)+8]);
            diaFlag = 1;
            }
            else
            {
              usnprintf(pcInsert, iInsertLen, "Diastolic Pressure: __ mm Hg");
              diaFlag = 0;
            }
          }
          else if (*(dPtrs2.bpOutOfRangePtr) == 'N')
          {
            usnprintf(pcInsert, iInsertLen, "Diastolic Pressure: %d mm Hg",dPtrs2.bloodPressCorrectedBufPtr[((*(dPtrs2.countCallsPtr)) % 8)+8]);
          }
            
            break;

        case SSI_INDEX_PULSESTATE:
          if(*(dPtrs2.pulseOutOfRangePtr) == 'Y')
          {
            if(pulseFlag == 0)
            {
            usnprintf(pcInsert, iInsertLen, "Pulse rate: %d BPM",dPtrs2.pulseRateCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
            pulseFlag = 1;
            }
            else
            {
            usnprintf(pcInsert, iInsertLen, "Pulse rate: __ BPM");
              pulseFlag = 0;
            }
          }
          else if (*(dPtrs2.pulseOutOfRangePtr) == 'N')
          {
            usnprintf(pcInsert, iInsertLen, "Pulse rate: %d BPM",dPtrs2.pulseRateCorrectedBufPtr[(*(dPtrs2.countCallsPtr)) % 8]);
          }

            break;
            
        case SSI_INDEX_EKGSTATE:
            usnprintf(pcInsert, iInsertLen, "EKG: %d Hz",dPtrs2.EKGFreqBufPtr[((ekgCounter - 1) % 16)]);
            break;
        
        case SSI_INDEX_BATTERYSTATE:
            usnprintf(pcInsert, iInsertLen, "Battery: %d",*dPtrs2.batteryStatePtr);
        break;
        
        case SSI_INDEX_ERRORSTATE:
            if(commandErrorFlag == 1)
            {
              usnprintf(pcInsert, iInsertLen, "Invalid Command");
              commandErrorFlag = 0;
            }
            else
              usnprintf(pcInsert, iInsertLen, "");
        break;

        default:
            usnprintf(pcInsert, iInsertLen, "??");
        break;
    }

    // Tell the server how many characters our insert string contains.
    return(strlen(pcInsert));
}

//*****************************************************************************
// Display an lwIP type IP Address.
//*****************************************************************************
void
DisplayIPAddress(unsigned long ipaddr, unsigned long ulCol,
                 unsigned long ulRow)
{
    char pucBuf[16];
    unsigned char *pucTemp = (unsigned char *)&ipaddr;

    // Convert the IP Address into a string.
    usprintf(pucBuf, "%d.%d.%d.%d", pucTemp[0], pucTemp[1], pucTemp[2],
             pucTemp[3]);

    // Display the string.
    RIT128x96x4StringDraw(pucBuf, ulCol, ulRow, 15);

}

//*****************************************************************************
// Required by lwIP library to support any host-related timer functions.
//*****************************************************************************

void
lwIPHostTimerHandler(void)
{
    static unsigned long ulLastIPAddress = 0;
    unsigned long ulIPAddress;
    ulIPAddress = lwIPLocalIPAddrGet();

    // Check if IP address has changed, and display if it has.
    if(ulLastIPAddress != ulIPAddress)
    {
        ulLastIPAddress = ulIPAddress;
        RIT128x96x4StringDraw("                       ", 0, 16, 15);
        RIT128x96x4StringDraw("                       ", 0, 24, 15);
        RIT128x96x4StringDraw("IP:   ", 0, 16, 15);
        RIT128x96x4StringDraw("MASK: ", 0, 24, 15);
        RIT128x96x4StringDraw("GW:   ", 0, 32, 15);
        DisplayIPAddress(ulIPAddress, 36, 16);
        ulIPAddress = lwIPLocalNetMaskGet();
        DisplayIPAddress(ulIPAddress, 36, 24);
        ulIPAddress = lwIPLocalGWAddrGet();
        DisplayIPAddress(ulIPAddress, 36, 32);
    }
}
/*-----------------------------------------------------------*/

//*****************************************************************************
//
// Handles the SysTick timeout interrupt.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
  // Used for checking if button presses have occurred
  unsigned long ulData, ulDelta;

  // Indicate that a timer interrupt has occurred.
  HWREGBITW(&g_ulFlags, FLAG_CLOCK_TICK) = 1;
  
  // Call the lwIP timer handler
  lwIPTimer(SYSTICKMS);
  
  // Disable the port interrupts
  portDISABLE_INTERRUPTS();
	{
		/* Increment the RTOS tick. */
		if( xTaskIncrementTick() != pdFALSE )
		{
			/* A context switch is required.  Context switching is performed in
			the PendSV interrupt.  Pend the PendSV interrupt. */
			portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
		}
	}
        // Enable the port interrupts
	portENABLE_INTERRUPTS();
        
  // only check buttons if there is not a button pressed
  if(!HWREGBITW(&g_ulFlags, FLAG_BUTTON_PRESS)){
    // Read the state of the push buttons.
    ulData = (GPIOPinRead(GPIO_PORTE_BASE, (GPIO_PIN_0 | GPIO_PIN_1 |
                                            GPIO_PIN_2 | GPIO_PIN_3)) |
              (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1) << 3));

    // Determine the switches that are at a different state than the debounced state.
    //debug line to imitate up click
    ulDelta = ulData ^ g_ucSwitches;

    // Increment the clocks by one.
    // Exclusive or of clock B If a bit is different in A and B then 1 if the bits have the same value = 0
    g_ucSwitchClockA ^= g_ucSwitchClockB;
    
    // Compliment of clock B. This changes 1 to 0 and 0 to 1 bitwise
    g_ucSwitchClockB = ~g_ucSwitchClockB;

    // Reset the clocks corresponding to switches that have not changed state.
    g_ucSwitchClockA &= ulDelta;
    g_ucSwitchClockB &= ulDelta;

    // Get the new debounced switch state.
    g_ucSwitches &= g_ucSwitchClockA | g_ucSwitchClockB;
    g_ucSwitches |= (~(g_ucSwitchClockA | g_ucSwitchClockB)) & ulData;

    // Determine the switches that just changed debounced state.
    ulDelta ^= (g_ucSwitchClockA | g_ucSwitchClockB);

    // See if the select button was  pressed during an alarm.
    if(g_ucSwitches==15 && auralFlag==1)
    {
        // Set a flag to indicate that the select button was just pressed.
        PWMGenDisable(PWM_BASE, PWM_GEN_0);
        auralFlag = 0;
        auralCounter = globalCounter;
        ackFlag = 1;
    }
    // See if any switches just changed debounced state.
    if(ulDelta && (g_ucSwitches != 0x1F))
    {
        // You can watch the variable for ulDelta
        // Up = 1 Right = 8 down =2 left =4  select = 16 Bit values
        HWREGBITW(&g_ulFlags, FLAG_BUTTON_PRESS) = 1;
        
    }
  }
}

//*****************************************************************************
//
// The interrupt handler for the first timer interrupt.
//
//*****************************************************************************
void
Timer0IntHandler(void)
{
    // Clear the timer interrupt.
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    // Update the global counter.
    IntMasterDisable();
    increment();
    IntMasterEnable();
}

//*****************************************************************************
//
// The interrupt handler for the pulse rate transducer interrupt.
//
//*****************************************************************************
void
Timer1IntHandler(void)
{
    // Clear the timer interrupt.
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    
    pulseCount++;
        
    if(pulseCount>20){
      pulseFreq=3;
    }
    else if(pulseCount >40){
      pulseFreq = 2;
    }
    else if(pulseCount >60){
      pulseFreq=1;
    }
    else if (pulseCount>80){
      pulseFreq =4;
      pulseCount =0;
    }
    
    TimerLoadSet(TIMER1_BASE, TIMER_A, (SysCtlClockGet()/pulseFreq)/2-1);

    // Update PR Flag
    if(g_ulFlagPR==0){
      g_ulFlagPR=1;
    }
    else
      g_ulFlagPR=0;
}


/*************************************************************************
 * Please ensure to read http://www.freertos.org/portlm3sx965.html
 * which provides information on configuring and running this demo for the
 * various Luminary Micro EKs.
 *************************************************************************/
int main( void )
{      
    // Configure the hardware
    prvSetupHardware();

    /* Create the queue used by the OLED task.  Messages for display on the OLED
    are received via this queue. */
    xOLEDQueue = xQueueCreate( mainOLED_QUEUE_SIZE, sizeof( xOLEDMessage ) );
    
    // Create tasks
    xTaskCreate(measure, "Measure Task", 1024, (void*)&mPtrs2, 3, &xMeasureHandle);
    xTaskCreate(alarm, "Warning Task", 500, (void*)&wPtrs2, 4, NULL);
    xTaskCreate(stat, "Status Task", 100, (void*)&sPtrs, 3, NULL);
    xTaskCreate(ekgCapture, "EKG Caputre Task", 500, (void*)&ecPtrs, 2, NULL);
    xTaskCreate(compute, "Compute Task", 100, (void*)&cPtrs2, 2, &xComputeHandle);
    xTaskCreate(disp, "Display Task", 500, (void*)&dPtrs2, 2, &xDisplayHandle);
    xTaskCreate(keypadfunction, "Keypad Task", 500, (void*)&kPtrs, 1, NULL);
    xTaskCreate(ekgProcess, "EKG Process Task", 1024, (void*)&ecPtrs, 1, &xEKGHandle);
    xTaskCreate(remoteCommunications, "RemComm Task", 100, (void*)&rPtrs, 2, NULL); 
    xTaskCreate(commandFunction, "Command Task", 200, (void*)&coPtrs, 2, &xCommandHandle);

    /* Start the tasks defined within this file/specific to this demo. */
    xTaskCreate( vOLEDTask, "OLED", mainOLED_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* Will only get here if there was insufficient memory to create the idle
    task. */
	return 0;
}
/*-----------------------------------------------------------*/
  
//*****************************************************************************
// Configure the system hardware
//*****************************************************************************
void prvSetupHardware( void )
{
  // Variables used for configurations
  unsigned long ulPeriod;
  unsigned long ulPeriodPR;
  
  /* If running on Rev A2 silicon, turn the LDO voltage up to 2.75V.  This is
  a workaround to allow the PLL to operate reliably. */
  if( DEVICE_IS_REVA2 )
  {
      SysCtlLDOSet( SYSCTL_LDO_2_75V );
  }

  /* Set the clocking to run from the PLL at 50 MHz */
  SysCtlClockSet( SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ );

  /* 	Enable Port F for Ethernet LEDs
          LED0        Bit 3   Output
          LED1        Bit 2   Output 
  */
  
  // Assign the system clock
  g_ulSystemClock = SysCtlClockGet();
  
  // Enable the peripherals used by this example.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1); 
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM);
  
  // Configure the GPIO used to output the state of the led
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0);
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);//GPIO_PF2_LED1
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);

  //**INITIALIZE BUTTONS**//
  //Configure the GPIOs used to read the state of the on-board push buttons.
  GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
  GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
                   GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
  GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
  GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
  GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0);
  
  /* ADC BEGIN*/
  // The ADC0 peripheral must be enabled for use.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

  /* Enable sample sequence 3 with a processor signal trigger.  Sequence 3
   will do a single sample when the processor sends a singal to start the
   conversion.  Each ADC module has 4 programmable sequences, sequence 0
   to sequence 3.  This example is arbitrarily using sequence 3. */
  ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);

  /* Configure step 0 on sequence 3.  Sample the temperature sensor
  (ADC_CTL_TS) and configure the interrupt flag (ADC_CTL_IE) to be set
   when the sample is done.  Tell the ADC logic that this is the last
   conversion on sequence 3 (ADC_CTL_END).  Sequence 3 has only one
   programmable step.  Sequence 1 and 2 have 4 steps, and sequence 0 has
   8 programmable steps.  Since we are only doing a single conversion using
   sequence 3 we will only configure step 0.  For more information on the
   ADC sequences and steps, reference the datasheet.*/
  ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_TS | ADC_CTL_IE |
                           ADC_CTL_END);

  // Since sample sequence 3 is now configured, it must be enabled.
  ADCSequenceEnable(ADC0_BASE, 3);

  /* Clear the interrupt status flag.  This is done to make sure the
   interrupt flag is cleared before we sample.*/
  ADCIntClear(ADC0_BASE, 3);
  
  /*ADC END*/
  
  //**INITIALIZE UART**//
  // Configure the GPIO for the UART
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
     
  // Set the configuration of the UART
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 460800,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));

  //**INITIALIZE TIMER INTERRUPT**//
  // Configure the 32-bit periodic timer.
  TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_PER);
  TimerConfigure(TIMER1_BASE, TIMER_CFG_32_BIT_PER);
  
  // Determine the period
  ulPeriodPR =(SysCtlClockGet()/400)/200 ;
  
  // Load the timers
  TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/1000);
  TimerLoadSet(TIMER1_BASE, TIMER_A, ulPeriodPR-1);

  // Setup the interrupt for the timer timeout.
  IntEnable(INT_TIMER0A);
  IntEnable(INT_TIMER1A);

  TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
  TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

  // Enable the timer.
  TimerEnable(TIMER0_BASE, TIMER_A);
  TimerEnable(TIMER1_BASE, TIMER_A);
  
  //**INITIAL SOUND WARNING**//
  // Set GPIO G1 as PWM pin.  They are used to output the PWM1 signal.
  GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
  
  // Compute the PWM period based on the system clock.
  ulPeriod = SysCtlClockGet() / 440;
  
  // Set the PWM period to 440 (A) Hz.
  PWMGenConfigure(PWM_BASE, PWM_GEN_0,
                    PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC);
  PWMGenPeriodSet(PWM_BASE, PWM_GEN_0, ulPeriod);

  // PWM1 to a duty cycle of 75%.
  PWMPulseWidthSet(PWM_BASE, PWM_OUT_1, ulPeriod * 3 / 4);

  // Enable the PWM1 output signal.
  PWMOutputState(PWM_BASE,PWM_OUT_1_BIT, true);
  
  // Enable and Reset the Ethernet Controller
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH); 
  SysCtlPeripheralReset(SYSCTL_PERIPH_ETH); 
  
  // Configure SysTick to periodically interrupt.
  SysTickPeriodSet(g_ulSystemClock / CLOCK_RATE);
  SysTickIntEnable();
  SysTickEnable();

  // Enable processor interrupts.
  IntMasterEnable();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{

  static xOLEDMessage xMessage = { "PASS" };
  static unsigned long ulTicksSinceLastDisplay = 0;
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

  /* Called from every tick interrupt.  Have enough ticks passed to make it
  time to perform our health status check again? */

  ulTicksSinceLastDisplay++;
  if( ulTicksSinceLastDisplay >= mainCHECK_DELAY )
  {
          ulTicksSinceLastDisplay = 0;

  }
}
/*-----------------------------------------------------------*/

void vOLEDTask( void *pvParameters )
{
  xOLEDMessage xMessage;
  unsigned long ulY, ulMaxY;
  static char cMessage[ mainMAX_MSG_LEN ];
  extern volatile unsigned long ulMaxJitter;
  const unsigned char *pucImage;

  /* Functions to access the OLED.  The one used depends on the dev kit
  being used. */
  void ( *vOLEDInit )( unsigned long ) = NULL;
  void ( *vOLEDStringDraw )( const char *, unsigned long, unsigned long, unsigned char ) = NULL;
  void ( *vOLEDImageDraw )( const unsigned char *, unsigned long, unsigned long, unsigned long, unsigned long ) = NULL;
  void ( *vOLEDClear )( void ) = NULL;

  /* Map the OLED access functions to the driver functions that are appropriate
  for the evaluation kit being used. */
  vOLEDInit = RIT128x96x4Init;
  vOLEDStringDraw = RIT128x96x4StringDraw;
  vOLEDImageDraw = RIT128x96x4ImageDraw;
  vOLEDClear = RIT128x96x4Clear;
  ulMaxY = mainMAX_ROWS_96;
  pucImage = pucBasicBitmap;


  ulY = ulMaxY;

  /* Initialise the OLED and display a startup message. */
  vOLEDInit( ulSSI_FREQUENCY );
  //vOLEDStringDraw( "POWERED BY FreeRTOS", 0, 0, mainFULL_SCALE );
  //vOLEDImageDraw( pucImage, 0, mainCHARACTER_HEIGHT + 1, bmpBITMAP_WIDTH, bmpBITMAP_HEIGHT );

  for( ;; )
  {
          /* Wait for a message to arrive that requires displaying. */
          xQueueReceive( xOLEDQueue, &xMessage, portMAX_DELAY );

          /* Write the message on the next available row. */
          ulY += mainCHARACTER_HEIGHT;
          if( ulY >= ulMaxY )
          {
                  ulY = mainCHARACTER_HEIGHT;
                  vOLEDClear();
                  vOLEDStringDraw( pcWelcomeMessage, 0, 0, mainFULL_SCALE );
          }

          /* Display the message along with the maximum jitter time from the
          high priority time test. */
          sprintf( cMessage, "%s [%uns]", xMessage.pcMessage, ulMaxJitter * mainNS_PER_CLOCK );
          vOLEDStringDraw( cMessage, 0, ulY, mainFULL_SCALE );
	}
}
/*-----------------------------------------------------------*/

//******************************************************************************
// The command function receives and controls commands sent from the web browser
//******************************************************************************
void commandFunction(void* data)
{
  // Dereference the data struct
  commandData * coData = (commandData*)data;

  // Create a flag for the 'd' command.
  static int displayFlag = 1;
   
  // Loop forever
  for( ;;)
  {
    // Assign the char buf to a char.
    char command = *coData->commandBufPtr;

    // Do nothing if no command is present
    if(command != NULL)
    {
      // Enable or disable display with the 'd' command
      if(command == 'd' || command == 'D')
      {
        // Disable display if flag is set to 1
        if (displayFlag == 1)
        {
          // Disable the display
          RIT128x96x4Disable();
          // Set flag to 0 for future 'd'
          displayFlag = 0;
        }
        // Enable display if flag is 0
        else
        {
          // Enable display
          RIT128x96x4Enable(1000000);
          // Set flag to 1 for future 'd'
          displayFlag = 1;
        }
      }
      
      //Start Measurements
      else if(command == 's' || command == 'S')
      {
        
          vTaskResume(xMeasureHandle);
        
      }
      
      // Stop Measurements
      else if(command == 'p' || command == 'P')
      {
        
          vTaskSuspend(xMeasureHandle);
        
      }
      
      // Retrieve latest Measurement Data
      else if(command == 'm' || command == 'M')
      {
        
          //Set a flag to obtain latest mesasurement data
        
      }
      
      // Retrieve latest warning/alarm data
      else if(command == 'w' || command == 'W')
      {
        
          //Set a flag to get latest warning data
        
      }
      
      // Enable or disable display with the 'd' command
      else if(command == 'i' || command == 'I')
      {
        iFlag = 1;
      }
      else{
        // no valid command value was sent.
        commandErrorFlag = 1;
      }

      // Clear the command buffer
      memset(coData->commandBufPtr, 0, sizeof(coData->commandBufPtr));
    }

    // Suspend task indefinitely
    vTaskSuspend(NULL);
  }
}

//*****************************************************************************
// The remote communications task initializes the network interface, connects to 
// and configures a local area network (LAN). Sets up a web server and handler 
// to communicate with a remote browser.  Formats the data to be displayed and 
// sends the formatted data over the network for display on the browser. 
// Continually updates the displayed data at a 5 second rate.
//******************************************************************************
void remoteCommunications(void* data)
{  
  // Dereference the data struct
  remCommData * rData = (remCommData*)data;
  
  // Used to store mac address
  unsigned long ulUser0, ulUser1;
  unsigned char pucMACArray[8];

  // Configure the hardware MAC address for Ethernet Controller filtering of
  // incoming packets.
  //
  // For the LM3S6965 Evaluation Kit, the MAC address will be stored in the
  // non-volatile USER0 and USER1 registers.  These registers can be read
  // using the FlashUserGet function, as illustrated below.
  FlashUserGet(&ulUser0, &ulUser1);

  // Mac address not programmed
  if((ulUser0 == 0xffffffff) || (ulUser1 == 0xffffffff))
  {
      // We should never get here.  This is an error if the MAC address
      // has not been programmed into the device.  Exit the program.
      RIT128x96x4StringDraw("MAC Address", 0, 16, 15);
      RIT128x96x4StringDraw("Not Programmed!", 0, 24, 15);
      while(1);
  }

  // Convert the 24/24 split MAC address from NV ram into a 32/16 split
  // MAC address needed to program the hardware registers, then program
  // the MAC address into the Ethernet Controller registers.
  pucMACArray[0] = ((ulUser0 >>  0) & 0xff);
  pucMACArray[1] = ((ulUser0 >>  8) & 0xff);
  pucMACArray[2] = ((ulUser0 >> 16) & 0xff);
  pucMACArray[3] = ((ulUser1 >>  0) & 0xff);
  pucMACArray[4] = ((ulUser1 >>  8) & 0xff);
  pucMACArray[5] = ((ulUser1 >> 16) & 0xff);

  // Initialze the lwIP library, using DHCP.
  lwIPInit(pucMACArray, 0, 0, 0, IPADDR_USE_DHCP);

  // Setup the device locator service.
  LocatorInit();
  LocatorMACAddrSet(pucMACArray);
  LocatorAppTitleSet("EK-LM3S8962 medical device");

  // Loop Forever
  for( ;; )
  {
    if(iFlag == 1)
    {
      initializeNetwork();
      iFlag = 0;
    }
    
    // Resume the command
    vTaskResume(xCommandHandle);

    // Delay for 5 seconds
    vTaskDelay(5000);
  }
}

void initializeNetwork()
{
    // Initialize a sample httpd server.
    httpd_init();

    // Pass our tag information to the HTTP server.
    http_set_ssi_handler(SSIHandler, g_pcConfigSSITags,
                         NUM_CONFIG_SSI_TAGS);

    // Pass our CGI handlers to the HTTP server.
    http_set_cgi_handlers(g_psConfigCGIURIs, NUM_CONFIG_CGI_URIS);
}

/*-----------------------------------------------------------*/
void vApplicationStackOverflowHook( TaskHandle_t *pxTask, signed char *pcTaskName )
{
	( void ) pxTask;
	( void ) pcTaskName;

	for( ;; );
}
/*-----------------------------------------------------------*/

void vAssertCalled( const char *pcFile, unsigned long ulLine )
{
volatile unsigned long ulSetTo1InDebuggerToExit = 0;

	taskENTER_CRITICAL();
	{
		while( ulSetTo1InDebuggerToExit == 0 )
		{
			/* Nothing do do here.  Set the loop variable to a non zero value in
			the debugger to step out of this function to the point that caused
			the assertion. */
			( void ) pcFile;
			( void ) ulLine;
		}
	}
	taskEXIT_CRITICAL();
}
