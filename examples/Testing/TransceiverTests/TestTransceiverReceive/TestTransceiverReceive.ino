/* 
* 
*/


#define WAIT_FOR_LOGGER
#define SERIAL_BAUD_RATE 115200

#include <ILoLaInclude.h>

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
#ifdef WAIT_FOR_LOGGER
	while (!Serial)
		;
	delay(1000);
#endif


	Serial.println(F(" Start!"));
}

void loop()
{
	
}
