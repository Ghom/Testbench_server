#ifndef NETWORK_H
#define NETWORK_H

#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <atomic>
#include <semaphore.h>
#include <pthread.h>

#define MAX_DATA_LEN 100

typedef enum {CMD_RFID=0, CMD_MOTOR, INFO_RFID, INFO_MOTOR} packet_type;

typedef struct __attribute__ ((packed)) packet_t
{
	uint8_t type;
	uint16_t size;
	uint8_t data[MAX_DATA_LEN]; 
}packet;

typedef struct CBtype
{

}CBtype_t;

template <typename T>
class SafeFIFO
{
public:
	SafeFIFO();
	~SafeFIFO();
	SafeFIFO<T> &operator=(const SafeFIFO<T> &);

	void 	Read(std::vector<T> &data);
	void 	Read(T &data);
	T&	Read(void);
	void 	Write(const std::vector<T> &data);
	void 	Write(const T* data, size_t size);
	void 	Write(const T& data);
	void 	Remove(size_t size);
	size_t 	Size();
	bool 	Empty();
	std::vector<T>& Vector();

private:
	std::mutex 	  	m_vec_mutex;
	std::vector<T> 		m_vec; 
};

template class SafeFIFO<packet_t>;
template class SafeFIFO<char>;

class Network
{
public:
	Network(uint32_t port, 
		SafeFIFO<packet_t> *RFID_fifo_in, 
		SafeFIFO<packet_t> *RFID_fifo_out, 
		SafeFIFO<packet_t> *Motor_fifo_in, 
		SafeFIFO<packet_t> *Motor_fifo_out);

	~Network();
	void Init();
	void DeInit();

	void Read(std::string &buffer, size_t size=0);
	void Write(std::string &data);
	void RegCallBack(CBtype type, void (*cbFct)());
	bool DataAvaillable();

private:
	void 		threadWaitClient();
	void 		threadTCPWrite();
	void 		threadTCPRead();
	void 		(*callBackNewPacket)();
	SafeFIFO<char>*	encapsulate(packet_type type, packet_t data);
	void 		decapsulate();
	void 		dispatch(packet_t command);

	std::thread 	m_threadWaitClient;
	pthread_t	m_threadWaitClientID;
	std::thread 	m_threadTCPRead;
	pthread_t	m_threadTCPReadID;
	std::thread 	m_threadTCPWrite;
	pthread_t	m_threadTCPWriteID;

	SafeFIFO<char> 		m_data_in;
	SafeFIFO<char> 		m_data_out;
	SafeFIFO<packet_t> 	*m_RFID_fifo_in;
	SafeFIFO<packet_t> 	*m_RFID_fifo_out;
	SafeFIFO<packet_t> 	*m_Motor_fifo_in;
	SafeFIFO<packet_t> 	*m_Motor_fifo_out;

	std::atomic<bool>	m_continue;
	std::atomic<bool>	m_clientConnected;
	sem_t			m_sem_unlockWriteThread;

	uint32_t		m_tcp_port;
	// this is used for test purposes
	int socket_fd;
	int client_socket_fd;
};

#endif