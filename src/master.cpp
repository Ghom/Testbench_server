/*refer to 
* http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
* http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "network.h"
#include "rfid.h"
#include "motor.h"

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
        // Create the RFID communication fifo
        SafeFIFO<packet_t> RFID_fifo_in;
        SafeFIFO<packet_t> RFID_fifo_out;

        // Create the Motor communication fifo
        SafeFIFO<packet_t> Motor_fifo_in;
        SafeFIFO<packet_t> Motor_fifo_out;

        printf("Start Network\n");
        Network network(12345, &RFID_fifo_in, &RFID_fifo_out, &Motor_fifo_in, &Motor_fifo_out);
        network.Init();

        printf("Start Motor\n");
        Motor motor(&Motor_fifo_in, &Motor_fifo_out);

        printf("Start  RFID\n");
        RFID rfid(&RFID_fifo_in, &RFID_fifo_out);
}

#ifdef NOTDEF
int mainbis(void)
{
        char action[20] = "";
        std::string packet;
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

        printf("Try network\n");
        Network net(12345);
        net.Init();

        do
        {
                // printf("Select action:");
                // scanf("%s",&action);
                if(net.DataAvaillable())
                {
                        net.Read(packet);
                        // printf("packet %s\n", packet.data());
                
                

                        if(packet == "start\n")
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
                        else if(packet == "stop\n")
                        {
                                printf("Stoping\n");
                                CmdReaderOnOff(&serialHandle, READER_SCAN_OFF);
                        }
                        else
                        {
                                net.Write(packet);
                        }
                }

        }while(packet != "quit\n");

        net.DeInit();
        return 0;
}/*main*/
#endif