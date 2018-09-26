#include <motor.h>
#include <assert.h>
#include <wiringPi.h>
#include <vector>

#define CLOCKWISE true
#define ANTICLOCKWISE false
#define PWM_MIN 8000
#define PWM_MAX 20000

uint16_t	g_speed_meas = 0;
uint32_t	g_speed_cmd = 0;
uint32_t	g_speed_pwm = 0;
uint32_t	g_counter = 0;

Motor::Motor(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out)
{
	assert(fifo_in != NULL);
	assert(fifo_out != NULL);

	pinMode(0, INPUT);
	pullUpDnControl(0, PUD_OFF);
	// its trigering on both anyway because the HC-020K is shit (even with resistor added)
	pinMode(1, PWM_OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT);
	direction(CLOCKWISE);
	pwmSetRange(32768); // default is 1024
	pwmSetMode(PWM_MODE_BAL);
	speed(0);
	// pwmSetMode(PWM_MODE_MS);
	// pwmSetClock(??);
	delay(1000);
	wiringPiISR(0, INT_EDGE_FALLING, &IsrSpeedMeas);

	m_fifo_in = fifo_in;
	m_fifo_out = fifo_out;

        m_continue = true;
        g_speed_meas = 0;
        m_threadWorker = std::thread(&Motor::threadWorker, this);
        m_threadSpeedMonitor = std::thread(&Motor::threadSpeedMonitor, this);
}

Motor::~Motor()
{
	pwmWrite(1, 0);
	m_continue = false;
	m_threadWorker.join();
	m_threadSpeedMonitor.join();
}

void Motor::Init()
{

}

void Motor::DeInit()
{
	m_continue = false;
	speed(0);
	printf("Stop motor\n");
}

void Motor::threadWorker(void)
{
	packet_t cmd;
	motor_cmd_t motor_cmd;

	while(this->m_continue)
	{
		//check if there is any command to dequeu
		while(!m_fifo_in->Empty())
		{
			// get the command execute the approriate action
			m_fifo_in->Read(cmd);
			decapsulate(cmd, motor_cmd);
			execute(motor_cmd);
		}
	}
}

void IsrSpeedMeas(void)
{
	static uint32_t elapsedms = 0;
	static uint32_t newTime=0, prevTime=0;
	static uint32_t counter=0;

	g_counter++;
}

// Cadence d'échantillonnage en ms
#define CADENCE_MS 100
volatile double dt = CADENCE_MS/1000.;
volatile double temps = -CADENCE_MS/1000.;
 
volatile double commande = PWM_MIN;
volatile double vref = 0;
 
// PID
// volatile double Kp = 0.29;
volatile double Kp = 0.49;
// volatile double Ki = 8.93;
volatile double Ki = 2.93;
volatile double P_x = 0.;
volatile double I_x = PWM_MIN;
volatile double ecart = 0.;
uint32_t stable_count=5;

void Motor::stop(void)
{
	dt = CADENCE_MS/1000.;
	temps = -CADENCE_MS/1000.;
	 
	commande = PWM_MIN;
	vref = 0;
	 
	Kp = 0.49;
	Ki = 2.93;
	P_x = 0.;
	I_x = PWM_MIN;
	ecart = 0.;
}

#define MEAN_SAMPLE_SIZE 10
#define TOLERANCE_PERCENTAGE 3//%

void Motor::threadSpeedMonitor(void)
{
	double rpm = 0;
	double average_speed = 0;
	std::vector<uint32_t> speeds(MEAN_SAMPLE_SIZE, 0);
	uint32_t i=0;
	double vref_tolerance;

	while(this->m_continue)
	{
		rpm = ((double)g_counter*1.5)/dt; // simplification of rpm = ((g_counter / 40(nb tick per full rotation)) * 60(seconds))/ dt (sec) 
		g_counter = 0;

		speeds[(i++)%MEAN_SAMPLE_SIZE-1] = rpm;

		for(uint32_t val : speeds)
			average_speed += val;
		average_speed /= 10;

		g_speed_meas = (uint16_t)average_speed;

		vref_tolerance = (vref*TOLERANCE_PERCENTAGE)/100;

		// printf("rpm:%lf mean rpm:%d ecart:%d vref_tolerance:%lf\n",rpm, (int)average_speed, abs(average_speed-vref), vref_tolerance);

		if((abs(average_speed-vref) <= vref_tolerance) && (stable_count < 5))
		{
			stable_count++;
			if(stable_count >= 5)
			{
				printf("STABLE SPEED\n");
				signalStable();
			}
		}

		/******* Régulation PID ********/
		// Ecart entre la consigne et la mesure
		ecart = vref - rpm;

		// Terme proportionnel
		P_x = Kp * ecart;

		// Calcul de la commande
		commande = P_x + I_x;

		// printf("rpm:%lf vref:%lf ecart:%lf P_x:%lf I_x:%lf commande:%lf\n",rpm, vref, ecart, P_x, I_x, commande);

		// Terme intégral (sera utilisé lors du pas d'échantillonnage suivant)
		I_x = I_x + Ki * dt * ecart;
		/******* Fin régulation PID ********/


		this->speed(commande);
		delay(CADENCE_MS);
	}
}

void Motor::decapsulate(packet_t &command, motor_cmd_t &motor_cmd)
{
	motor_cmd.type = command.data[0];
	motor_cmd.argc = command.data[1];
	for(uint32_t i=0; i<motor_cmd.argc; i++)
		motor_cmd.args[i] = command.data[i+2];
}

void Motor::execute(motor_cmd_t &cmd)
{
	switch(cmd.type)
	{
		case CMD_CHANGE_SPEED:
			g_speed_cmd = cmd.args[0]<<8 | cmd.args[1];
			stable_count=0;
			vref = g_speed_cmd;
			if(vref == 0)
			{
				stop();
			}
			printf("Change engine speed to %d %s\n",g_speed_cmd, cmd.args[2]==1?"clockwise":"anti-clockwise");
			direction(cmd.args[2]);
		break;
		case CMD_GET_SPEED:
			printf("Current engine speed is %d\n",g_speed_meas);
		break;
		
		default:
		printf("default\n");

	}
}

void Motor::signalStable(void)
{
	packet_t info;

	info.type = INFO_MOTOR;
	info.size = 3;

	info.data[0] = CMD_SIGNAL_STABLE_SPEED; // type
	info.data[1] = (g_speed_meas >> 8) & 0xFF; // argv[0]
	info.data[2] = g_speed_meas & 0xFF; // argv[1]
	m_fifo_out->Write(info);
}

void Motor::direction(bool clockwise)
{
	if(clockwise)
	{
		digitalWrite(4, LOW);
		digitalWrite(5, HIGH);
	}
	else
	{
		digitalWrite(4, HIGH);
	 	digitalWrite(5, LOW);
	}
}

void Motor::speed(uint32_t speed)
{
	pwmWrite(1, speed);
}
