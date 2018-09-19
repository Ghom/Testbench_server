#include <motor.h>
#include <assert.h>
#include <wiringPi.h>
#include <vector>

#define CLOCKWISE true
#define ANTICLOCKWISE false

uint32_t	g_speed_meas = 0;
uint32_t	g_speed_cmd = 0;
uint32_t	g_speed_pwm = 0;
uint32_t	g_counter = 0;

Motor::Motor(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out)
{
	assert(fifo_in != NULL);
	assert(fifo_out != NULL);

	wiringPiSetup();
	// pinMode (0, OUTPUT) ;
	// for (;;)
	// {
	// 	digitalWrite (0, HIGH) ; delay (500) ;
	// 	digitalWrite (0,  LOW) ; delay (500) ;
	// }
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
	// counter++;

	// printf("%d\n",counter);

	// if(counter == 40)
	// {
	// 	newTime = millis();
	// 	elapsedms = newTime - prevTime;
	// 	// printf("prev:%d new:%d elaps:%d ",prevTime, newTime, elapsedms);
	// 	prevTime = newTime;
	// 	counter = 0;
	// 	if(elapsedms != 0)
	// 		g_speed_meas = 60000/elapsedms;
	// 	// printf("speed:%d ",g_speed_meas);
	// }
}

// void Motor::threadSpeedMonitor(void)
// {
// 	int32_t diff_cmd_meas = 0;
// 	uint32_t old_pwm = 0;
// 	double average_speed = 0;
// 	std::vector<uint32_t> speeds;
// 	speeds.clear();
// 	uint32_t i=0;

// 	while(this->m_continue)
// 	{
// 		if(g_movment_counter == 0)
// 			g_speed_meas = 0;

// 		g_movment_counter = 0;


// 		if(speeds.size() < 100)
// 			speeds.push_back(g_speed_meas);
// 		else
// 		{
// 			if(i==100)
// 				i = 0;
// 			speeds[i++] = g_speed_meas;
// 		}

// 		uint32_t k=0;
// 		for(uint32_t val : speeds)
// 		{
// 			k++;
// 			average_speed += val;
// 		}
// 		average_speed /= k;

// 		if(i == 100)
// 		{

// 			diff_cmd_meas =  g_speed_cmd - average_speed;//g_speed_meas;

// 			old_pwm = g_speed_pwm;
// 			if(diff_cmd_meas > 100)
// 				g_speed_pwm += ((16000-g_speed_pwm)/10)+1;
// 			else if(diff_cmd_meas < -100)
// 				g_speed_pwm -= ((g_speed_pwm-8200)/10)+1;
// 			else if(diff_cmd_meas > 10)
// 				g_speed_pwm += 5;
// 			else if(diff_cmd_meas < -10)
// 				g_speed_pwm -= 5;
// 			else if(diff_cmd_meas > 0)
// 				g_speed_pwm += 1;
// 			else if(diff_cmd_meas < 0)
// 				g_speed_pwm -= 1;

// 			if(g_speed_meas)
// 				printf("speed:%d(%lf) diff:%d pwm:%d\n",g_speed_meas, average_speed, diff_cmd_meas, g_speed_pwm);

// 			if(old_pwm != g_speed_pwm)
// 				this->speed(g_speed_pwm);
// 		}

// 		delay(10);
// 	}
// }

// Cadence d'échantillonnage en ms
#define CADENCE_MS 100
volatile double dt = CADENCE_MS/1000.;
volatile double temps = -CADENCE_MS/1000.;
 
volatile double commande = 8000.;
volatile double vref = 0;
 
// PID
// volatile double Kp = 0.29;
volatile double Kp = 0.49;
// volatile double Ki = 8.93;
volatile double Ki = 2.93;
volatile double P_x = 0.;
volatile double I_x = 8000.;
volatile double ecart = 0.;

void Motor::threadSpeedMonitor(void)
{
	double rpm = 0;
	while(this->m_continue)
	{
		rpm = ((double)g_counter*1.5)/dt; // simplification of rpm = ((g_counter / 40(nb tick per full rotation)) * 60(seconds))/ dt (sec) 
		g_counter = 0;

		/******* Régulation PID ********/
		// Ecart entre la consigne et la mesure
		ecart = vref - rpm;

		// Terme proportionnel
		P_x = Kp * ecart;

		// Calcul de la commande
		commande = P_x + I_x;

		printf("rpm:%lf vref:%lf ecart:%lf P_x:%lf I_x:%lf commande:%lf\n",rpm, vref, ecart, P_x, I_x, commande);

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
			vref = g_speed_cmd;
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