#include <Network.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/un.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

///////////////// Network Class implementation /////////////////
Network::Network(uint32_t port, 
	SafeFIFO<packet_t> *RFID_fifo_in, 
	SafeFIFO<packet_t> *RFID_fifo_out, 
	SafeFIFO<packet_t> *Motor_fifo_in, 
	SafeFIFO<packet_t> *Motor_fifo_out)
{
	m_tcp_port = port;
	// printf("Start Network\n");
	m_data_in = SafeFIFO<char>();
	m_data_out = SafeFIFO<char>();

	m_RFID_fifo_in = RFID_fifo_in; 
	m_RFID_fifo_out = RFID_fifo_out;
	m_Motor_fifo_in = Motor_fifo_in;
	m_Motor_fifo_out = Motor_fifo_out;
	
	m_continue = true;
}

Network::~Network()
{

}

void Network::Init()
{
	struct sockaddr_in sin;
	char buf[100];

	// char* socket_path="/tmp/socktest";

	if ( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");   
	sin.sin_family = AF_INET;
	sin.sin_port = htons(m_tcp_port);

	// strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
	// unlink(socket_path);
	int on = 1;
	setsockopt (socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

	if (bind(socket_fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
		perror("bind error");
	}

	if (listen(socket_fd, 5) == -1) 
	{
		perror("listen error");
	}

	sem_init(&m_sem_unlockWriteThread, 0, 1);
	// Lock the thread TCP write
	sem_wait(&(this->m_sem_unlockWriteThread));

	printf("create threadWaitClient\n");
	m_threadWaitClient = std::thread(&Network::threadWaitClient, this);
}

void Network::DeInit()
{
	printf("Stop Network\n");
	m_continue = false;
	m_clientConnected = false;
	// sem_post(&(this->m_sem_unlockWriteThread));
	pthread_kill(m_threadTCPReadID, 0);
	pthread_kill(m_threadTCPWriteID, 0);
	pthread_kill(m_threadWaitClientID, 0);
	// m_threadWaitClient.join();
	printf("Exiting %s\n",__FUNCTION__);
}

void Network::threadWaitClient()
{
	char buf[100];
	int read_count;
	this->client_socket_fd = -1;
	int cl;
	packet_t rfid_packet, motor_packet, in_packet;
	uint32_t magic_num = 0;

	m_threadWaitClientID = pthread_self();

	printf("Entering %s\n",__FUNCTION__);
	while(this->m_continue)
	{
		if(this->client_socket_fd == -1)
		{
			printf("Wait for new client\n");
			if ( (cl = accept(this->socket_fd, NULL, NULL)) == -1) 
			{
				perror("accept error");
				continue;
			}

			this->client_socket_fd = cl;
			printf("New Client Accepted\n");
			m_clientConnected = true;
			m_threadTCPRead = std::thread(&Network::threadTCPRead, this);
			m_threadTCPWrite = std::thread(&Network::threadTCPWrite, this);
			m_threadTCPRead.detach();
			m_threadTCPWrite.detach();
			printf("Read/Write thread created\n");
			// m_threadTCPWrite.join();
			// printf("Write thread destroyed\n");
			// m_threadTCPRead.join();
			// printf("Read thread destroyed\n");
		}

		//TODO: implement semaphore system to block the thread if there is nothing to do

		if(DataAvaillable())
		{
			printf("NETWORK (thread worker): Data received to decapsulate\n");
			decapsulate();
		}

		while(!m_RFID_fifo_out->Empty())
		{
			//TODO: use encapsulate instead of this 

			m_RFID_fifo_out->Read(rfid_packet);

			size_t size = 4 + 3 + rfid_packet.size;
			char* buf = (char*)malloc(size);
			if(NULL == buf)
			{
				printf("allocation failed\n");
				continue;
			}

			uint32_t magic = 0x41421356;
			memcpy(buf, &magic, 4);
			memcpy(buf+4, &rfid_packet, size - 4);
			m_data_out.Write(buf, size);
			sem_post(&m_sem_unlockWriteThread); // free the semaphore so the write thread can process
			free(buf);
		}

		while(!m_Motor_fifo_out->Empty())
		{
			//TODO: use encapsulate instead of this 

			m_Motor_fifo_out->Read(motor_packet);

			size_t size = sizeof(packet_t) + motor_packet.size;
			char* buf = (char*)malloc(size);
			if(NULL == buf)
			{
				printf("allocation failed\n");
				continue;
			}

			memcpy(buf, &motor_packet, size);
			m_data_out.Write(buf, size);
			free(buf);
		}
	}
}

void Network::threadTCPRead()
{
	char buf[100];
	int read_count;
	m_threadTCPReadID = pthread_self();

	while(this->m_clientConnected)
	{
		m_data_in.Vector().clear();
		while ( (read_count=read(this->client_socket_fd,buf,sizeof(buf))) > 0) 
		{
			m_data_in.Write(buf, read_count);
			printf("read %u bytes: %.*s\n", read_count, read_count, buf);
			// write(this->client_socket_fd, "echo: ", 6);
			// write(this->client_socket_fd, buf, read_count);

		}
		
		if (read_count == -1) 
		{
			perror("read");
		}
		else if (read_count == 0) 
		{
			this->m_clientConnected = false;
			close(this->client_socket_fd);
			this->client_socket_fd = -1;

			//unlock write thread for a clean exit
			sem_post(&(this->m_sem_unlockWriteThread));
		}
	}

	printf("End thread TCP Read\n");
}

void Network::threadTCPWrite()
{
	std::vector<char> data;
	m_threadTCPWriteID = pthread_self();

	while(this->m_clientConnected)
	{
		// aquire the semaphore so this thread is locked until next release
		sem_wait(&(this->m_sem_unlockWriteThread));
		m_data_out.Read(data);
		for(char c : data)
		{
			if(this->client_socket_fd != -1)
			{				
				write(this->client_socket_fd, &c, 1);
			}
		}
	}

	printf("End thread TCP Write\n");
}

void Network::Read(std::string &buffer, size_t size)
{
	std::vector<char> vec;
	m_data_in.Read(vec);
	buffer = vec.data();
}

void Network::Write(std::string &data)
{
	if(!m_clientConnected)
	{
		printf("Can't write because there is no client listening");
		return;
	}

	m_data_out.Write(data.data(), data.size());
	sem_post(&m_sem_unlockWriteThread); // free the semaphore so the write thread can process
}

bool Network::DataAvaillable()
{
	return !m_data_in.Empty();
}

void Network::RegCallBack(CBtype type, void (*cbFct)())
{

}

SafeFIFO<char>* Network::encapsulate(packet_type type, packet_t data)
{
	SafeFIFO<char> ret;
	return &ret;
}

void Network::decapsulate()
{
	uint16_t size = 0x0000;
	uint8_t type = 0x00;
	uint32_t numb_magic = 0x00000000;
	uint32_t i = 0, k = 0;
	packet_t packet_in;
	
	printf("enter %s\n",__FUNCTION__);

	for (i = 0; i < 4; ++i)
	{
		numb_magic |= m_data_in.Vector()[k + i] << (24 - i*8);
	}
	k+=4;

	/*test on numb_magic*/
	while((k < MAX_DATA_LEN) && (numb_magic == 0x41421356)) 
	{
		/*extracting header*/
		type = m_data_in.Vector()[k];
		size = (m_data_in.Vector()[k + 1] << 8| m_data_in.Vector()[k + 2]);
		k+=3;
		
		/*put the frame, the size, and the type in memory*/
		packet_in.size  = size;
		packet_in.type = type;
		
			
		/*start to 3 because 3 bytes are needed for the header. 
		 * If we want to get the payload, we must start at frame_in[k+3] 
		 * (k+0, k+1, k+2, are used: header)*/

		for (i = k; i < k + size; ++i)
		{
			packet_in.data[i - k] = m_data_in.Vector()[i];		
		}
		
		/*we forward packet to the dispatcher*/
		dispatch(packet_in);
		
		/*next step*/
		k+=size;
		
		if ( (k + 4 < MAX_DATA_LEN) && (k + 4 < m_data_in.Size()) )
		{
			/*reset and get the potential next numb_magic*/
			numb_magic = 0x00000000;
			for (i = 0; i < 4; ++i)
			{
				numb_magic |= m_data_in.Vector()[k + i] << (24 - i*8);
			}
			k+=4;
		}
		else
		{
			break;
		}		
	}
	if (k >= MAX_DATA_LEN)
	{
		printf("The buffer is overloaded, next read is skipped\n");
	}	
	
	/*empty all the data we already read from the input fifo*/
	printf("Try removing %d bytes from data read\n", k);
	m_data_in.Remove(k);
	printf("exit %s\n", __FUNCTION__);
}

void Network::dispatch(packet_t command)
{
        printf("Dispatch packet to\n");
        switch(command.type)
        {
                case CMD_RFID:
                	printf("CMD_RFID\n");
                	m_RFID_fifo_in->Write(command);
                	break;
                case CMD_MOTOR:
                	printf("CMD_MOTOR\n");
                	m_Motor_fifo_in->Write(command);
                	break;
                default:
                	printf("Wrong packet to dispatch\n");
        }

        printf("exit %s\n", __FUNCTION__);
}

///////////////// Thread safe FIFO Buffer Class Implementation //////////////////
template <typename T>
SafeFIFO<T>::SafeFIFO()
{
	m_vec.clear();
}

template <typename T>
SafeFIFO<T>::~SafeFIFO()
{

}

template <class T>
SafeFIFO<T> &SafeFIFO<T>::operator=(const SafeFIFO<T> &Init)
{
	// need to implement
	return *this;
}

template <typename T>
void SafeFIFO<T>::Read(std::vector<T> &data)
{
	data.clear();

	m_vec_mutex.lock();
	for(T block : m_vec)
		data.push_back(block);
	m_vec.clear();
	m_vec_mutex.unlock();
}

template <typename T>
void SafeFIFO<T>::Read(T &data)
{
	m_vec_mutex.lock();

	data = m_vec[0];
	m_vec.erase(m_vec.begin());
	
	m_vec_mutex.unlock();
}

template <typename T>
T& SafeFIFO<T>::Read(void)
{
	T data; 

	m_vec_mutex.lock();

	data = m_vec[0];
	m_vec.erase(m_vec.begin());
	
	m_vec_mutex.unlock();

	return data;
}

template <typename T>
void SafeFIFO<T>::Write(const std::vector<T> &data)
{ 
	m_vec_mutex.lock();

	for(T block : data)
		m_vec.push_back(block);

	m_vec_mutex.unlock();
}

template <typename T>
void SafeFIFO<T>::Write(const T* data, size_t size)
{ 
	m_vec_mutex.lock();

	for(uint32_t i=0; i<size; i++)		
		m_vec.push_back(data[i]);

	m_vec_mutex.unlock();
}

template <typename T>
void SafeFIFO<T>::Write(const T& data)
{ 
	m_vec_mutex.lock();
	
	m_vec.push_back(data);

	m_vec_mutex.unlock();
}

template <typename T>
void 	SafeFIFO<T>::Remove(size_t size)
{
	m_vec_mutex.lock();
	m_vec.erase(m_vec.begin(), m_vec.begin() + size);
	m_vec_mutex.unlock();
}

template <typename T>
size_t 	SafeFIFO<T>::Size()
{
	return m_vec.size();
}

template <typename T>
bool 	SafeFIFO<T>::Empty()
{
	return m_vec.empty(); 
}

template <typename T>
std::vector<T> &SafeFIFO<T>::Vector()
{
	return m_vec; 
}
