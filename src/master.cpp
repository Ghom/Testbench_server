/*refer to 
* http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
* http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <wiringPi.h>

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

std::atomic<bool> quit(false);    // signal flag

void got_signal(int)
{
    quit.store(true);
}

int main(void)
{
        struct sigaction sa;
        memset( &sa, 0, sizeof(sa) );
        sa.sa_handler = got_signal;
        sigfillset(&sa.sa_mask);
        sigaction(SIGINT,&sa,NULL);

        // since RFID and Motor need GPIO access the init should be done here
        wiringPiSetup();

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

        while (true)
        {
                sleep(1);
                if( quit.load() ) 
                        break;    // exit normally after SIGINT
        }

        printf("Stopping server\n");
        rfid.DeInit();
        motor.DeInit();
        network.DeInit();

        return 0;
}
