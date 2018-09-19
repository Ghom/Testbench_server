#ifndef RFID_H
#define RFID_H

#include <thread>
#include <atomic>
#include <pthread.h>

#include "network.h"
#include "SerialCommHelper.h"

typedef struct rfid_cmd_t
{
	uint8_t type;
	uint8_t argc;
	uint32_t args[20];
}rfid_cmd;

typedef enum  {RFID_DISCONECTED=0, RFID_IDLE, RFID_SCANNING, RFID_ERROR} rfid_state_t;

class RFID
{
public:
	RFID(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out);
	~RFID();
	void Init();
	void DeInit();

private:
	void threadWorker(void);
	void threadScan(void);
	void decapsulate(packet_t &command, rfid_cmd_t &rfid_cmd);
	void execute(rfid_cmd_t &cmd);

	std::thread 		m_threadWorker;
	std::thread 		m_threadScan;

	SafeFIFO<packet_t> 	*m_fifo_in;
	SafeFIFO<packet_t> 	*m_fifo_out;

	CSerialCommHelper 	m_serial;
	std::atomic<bool>	m_continue;

	rfid_state_t		m_status;
};

#endif