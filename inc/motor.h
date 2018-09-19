#ifndef MOTOR_H
#define MOTOR_H
#include <thread>
#include <atomic>
#include <pthread.h>

#include "network.h"

typedef enum {CMD_CHANGE_SPEED=0, CMD_GET_SPEED} motor_cmd_type_t;

typedef struct motor_cmd_t
{
	uint32_t type;
	uint32_t argc;
	uint32_t args[20];
}motor_cmd;

void IsrSpeedMeas(void);

class Motor
{
public:
	Motor(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out);
	~Motor();
	void Init();
	void DeInit();

private:
	void threadWorker(void);
	void threadSpeedMonitor(void);
	void decapsulate(packet_t &command, motor_cmd_t &motor_cmd);
	void execute(motor_cmd_t &cmd);
	void direction(bool clockwise);
	void speed(uint32_t speed);
	
	std::thread 		m_threadWorker;
	std::thread 		m_threadSpeedMonitor;

	SafeFIFO<packet_t> 	*m_fifo_in;
	SafeFIFO<packet_t> 	*m_fifo_out;

	std::atomic<bool>	m_continue;
};

#endif