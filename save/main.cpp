/*refer to 
* http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
* http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ST25RU3993_driver_api.h"
#include "SerialCommHelper.h"
#include "serial.h"

#define SEC *1000*1000
#define MILLI_SEC *1000
#define MICRO_SEC *1

#define READER_SCAN_ON          1
#define READER_SCAN_OFF         0

std::string rxBuffer;
// char txBuffer[100] = "fromPI";

int main(void)
{
        char action[20] = "";
        int time = 0;

        CSerialCommHelper serialHandle = CSerialCommHelper();
        if (serialHandle.Init("/dev/serial0", 115200, 0, 1, 8))
        {
                return -1;
        }

        printf("serial opened\n");
        serialHandle.Start();
        printf("serial started\n");
        if(CheckConnection(&serialHandle))
        {
                serialHandle.Stop();
                serialHandle.UnInit();
                return -1;
        }

        printf("connection checked\n");
        do
        {
                printf("Select action:");
                scanf("%s",&action);

                if(!strcmp(action,"start"))
                {
                        time = 0;
                        unsigned char reader_state = 0;
                        int count =0;
                        while(1)//time < 120)
                        {
                                if(time%3 == 0)
                                {
                                        reader_state = ~reader_state;
                                        if(reader_state)
                                        {
                                                count++;
                                                CmdReaderOnOff(&serialHandle, READER_SCAN_ON);
                                                printf("\n\nStart (%d)\n\n",count);
                                        }
                                        else
                                        {
                                                CmdReaderOnOff(&serialHandle, READER_SCAN_OFF);
                                                printf("\n\nStop (%d)\n\n",count);
                                        }
                                }


                                if(reader_state)
                                {
                                        // serialHandle.Write(txBuffer, 100);
                                        if (serialHandle.ReadAvailable(rxBuffer) != S_OK)
                                        {
                                                DEBUG("Wait for packet timeout");
                                                return -1;
                                        }

                                        unsigned int pos=0;
                                        unsigned int start=0;
                                        unsigned int end = 0;
                                        printf("\nread %d bytes:",rxBuffer.size());
                                        while(pos<rxBuffer.size())
                                        {
                                                end = (rxBuffer.size() - pos) >= 11 ? (pos + 11) : rxBuffer.size();
                                                start = pos; 
                                                for(pos; pos<end; pos++)
                                                {
                                                        // printf("0x%02x ",rxBuffer[pos]);
                                                }
                                                if(rxBuffer[pos-1] == 1)
                                                        printf("|");
                                                // printf("\n");
                                        }
                                }
                                usleep(1 SEC);
                                time++;
                        }
                }
                else if(!strcmp(action,"stop"))
                {
                        printf("Stoping\n");
                        CmdReaderOnOff(&serialHandle, READER_SCAN_OFF);
                }

        }while(strcmp(action,"quit"));

        return 0;
}/*main*/