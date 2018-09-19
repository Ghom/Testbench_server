// ST25RU3993_Reader_cmdline.cpp : définit le point d'entrée pour l'application console.
//
#include <stdlib.h>
#include <stdio.h>
#include <cstdlib>

#include "SerialCommHelper.h"
#include "st_stream.h"
#include "commands_application.h"
#include "ST25RU3993_driver_api.h"

enum actionFlag { ACTION_IDLE = 0, ACTION_SCANNING, ACTION_NONE };
enum stateFlag { STATE_OFFLINE = 0, STATE_ONLINE, STATE_NONE };

#define READER_SCAN_ON		1
#define READER_SCAN_OFF		0

typedef struct readerSettings
{
	boardSettings	board;
	Gen2Settings	gen2;
	scanSettings	scan;
	rxSettings		rx;
	char			paLevel;
	char			rxGain;
}readerSettings_t;

typedef struct appContext
{
	char infoBar[100];
	char hwInfo[20];
	char swInfo[20];
	char swVerMajor;
	char swVerMinor;
	char swVerRev;
	actionFlag action;
	stateFlag state;
	char serialPort[10];
	CSerialCommHelper *serialHdl;
	unsigned int readCount;
	unsigned int tagDetected;
	readerSettings settings;
}appContext_t;

const char * GetAction(appContext *ctx)
{
	switch (ctx->action)
	{
	case ACTION_IDLE:
		return "Idle\t";
		break;
	case ACTION_SCANNING:
		return "Scanning";
		break;
	default:
		ERROR("Action flag not supported");
		return "N/a";
	}
}

const char * GetState(appContext *ctx)
{
	switch (ctx->state)
	{
	case STATE_OFFLINE:
		return "Offline";
		break;
	case STATE_ONLINE:
		return "Online";
		break;
	default:
		ERROR("State flag not supported");
		return "N/a";
	}
}

void PrintInfos(appContext *ctx)
{
	static bool readerAvailable = false;
	if (ctx->state == STATE_ONLINE)
		readerAvailable = true;

	printf("Reader \t\t\t|Hardware \t\t\t|Software \t\t\t|Action \t\t|State\n");
	if(readerAvailable)
		printf("ST25RU3993 Reader \t|%s \t\t|%s %d.%d.%d \t|%s \t\t|%s\n", ctx->hwInfo, ctx->swInfo, ctx->swVerMajor, ctx->swVerMinor, ctx->swVerRev, GetAction(ctx), GetState(ctx));

	if (ctx->readCount != 0)
	{
		printf("Tag detected: %d\n", ctx->tagDetected);
		printf("Read count: %d\n", ctx->readCount);
	}
}

void PrintMenu(appContext *ctx)
{
	printf("------------------------------------------------------ Info -----------------------------------------------------------\n");
	printf("%s\n", ctx->infoBar);
	printf("------------------------------------------------------ Menu -----------------------------------------------------------\n");
	printf("Esc:Quit | F1:Connect/Disconect Reader | F2:Start/Stop Scanning | F3:Reader Settings\n");
	printf("-----------------------------------------------------------------------------------------------------------------------\n");

}

void PrintSettings(appContext *ctx)
{
	system("cls");
	printf("------------------------------------------------------ Info -----------------------------------------------------------\n");
	printf("%s\n", ctx->infoBar);
	printf("------------------------------------------------------ Settings Menu --------------------------------------------------\n");
	printf("q:Back to main screen | modify value: [option code]=[value] (E.g. t1=ext) \n");
	printf("-----------------------------------------------------------------------------------------------------------------------\n");
	printf("\n          ------------                                          \n");
	printf("----------|Tx Options|------------------------------------------\n");
	printf("          ------------                                          \n");
	printf("t1: PA = %s [ext|int]\n", (ctx->settings.board.PA == 0?"int":"ext"));
	printf("t2: Output Level = %ddB [-19;0]\n", ctx->settings.paLevel);
	printf("----------------------------------------------------------------\n");
	printf("\n          ------------                                          \n");
	printf("----------|Rx Options|------------------------------------------\n");
	printf("          ------------                                          \n");
	printf("r1: Reiceive Gain = %ddB [-17;19]\n", ctx->settings.rxGain);
	printf("r2: Adaptive sensitivity = %s [on|off]\n", ctx->settings.rx.adaptiveSensitivityEnable==1?"on":"off");
	printf("----------------------------------------------------------------\n");

}

bool IsKeyPressed(unsigned timeout_ms = 0)
{
	return WaitForSingleObject(
		GetStdHandle(STD_INPUT_HANDLE),
		timeout_ms
	) == WAIT_OBJECT_0;
}

void SettingT1(appContext *ctx, char * value)
{
	if (!strcmp(value, "ext"))
	{
		ctx->settings.board.PA = 1;
		CmdConfigPA(ctx->serialHdl, 1, &ctx->settings.board.PA);
	}
	else if (!strcmp(value, "int"))
	{
		ctx->settings.board.PA = 0;
		CmdConfigPA(ctx->serialHdl, 1, &ctx->settings.board.PA);
	}
	else
	{
		sprintf(ctx->infoBar, "Wrong value chosen for PA configuration");
		return;
	}

	sprintf(ctx->infoBar, "PA configuration changed to %s", value);
}

void SettingT2(appContext *ctx, char * value)
{
	char level = strtol(value, NULL, 10);
	if (!(level == 0 && strcmp(value, "0")))
	{
		if (!(level >= -19 && level <= 0))
		{
			sprintf(ctx->infoBar, "The Output level value is not in the allowed range [-19;0]");
			return;
		}

		ctx->settings.paLevel = level;
		CmdWriteReg(ctx->serialHdl, 0x15, ctx->settings.paLevel);
	}
	else
	{
		sprintf(ctx->infoBar, "The Output level value is not a number");
		return;
	}
	
	sprintf(ctx->infoBar, "Output Level changed to %sdB", value);
}

void SettingR1(appContext *ctx, char * value)
{
	char level = strtol(value, NULL, 10);
	if (!(level == 0 && strcmp(value, "0")))
	{
		if (!(level >= -17 && level <= 19))
		{
			sprintf(ctx->infoBar, "The Reiceive Gain value is not in the allowed range [-17;+19]");
			return;
		}

		ctx->settings.rxGain = level;
		CmdConfigTxRx(ctx->serialHdl, 1, &ctx->settings.rxGain, 0, NULL, 0);
	}
	else
	{
		sprintf(ctx->infoBar, "The Reiceive Gain value is not a number");
		return;
	}
	
	sprintf(ctx->infoBar, "Reiceive Gain changed to %sdB", value);
}

void SettingR2(appContext *ctx, char * value)
{
	if (!strcmp(value, "on"))
	{
		ctx->settings.rx.adaptiveSensitivityEnable = 1;
	}
	else if (!strcmp(value, "off"))
	{
		ctx->settings.rx.adaptiveSensitivityEnable = 0;
	}
	else
	{
		sprintf(ctx->infoBar, "Wrong value chosen for Adaptive sensibility");
		return;
	}

// modify adaptive sensi
rxSettings params;
memset(&params, 0, sizeof(rxSettings));
CmdInvParams(ctx->serialHdl, &params, &params); // read
params.wAdaptiveSensitivityMode = 1;
params.adaptiveSensitivityEnable = ctx->settings.rx.adaptiveSensitivityEnable;
CmdInvParams(ctx->serialHdl, &params, &params); // write

sprintf(ctx->infoBar, "Adaptive sensitivity is now %s", value);
}

void getSettings(appContext *ctx)
{
	CmdReaderConfig(ctx->serialHdl, 0, 0, &ctx->settings.board);
	CmdGen2Settings(ctx->serialHdl, NULL, &ctx->settings.gen2);
	CmdInvParams(ctx->serialHdl, NULL, &ctx->settings.rx);

	CmdReadReg(ctx->serialHdl, 0x15, &ctx->settings.paLevel);
	CmdConfigTxRx(ctx->serialHdl, 0, &ctx->settings.rxGain, 0, NULL, 0);
}

// #include <conio.h>
void MenuSettings(appContext *ctx)
{
	char command[20];
	char option[10];
	char value[10];
	char * ch;
	bool quit = false;
	while (_getch()); // flush input

	getSettings(ctx);

	do
	{
		PrintSettings(ctx);
		printf("\nEnter your command: ");
		scanf("%s", command);

		ch = strtok(command, "=");
		strcpy(option, ch);
		ch = strtok(NULL, "=");
		if (ch != NULL)
			strcpy(value, ch);

		if (!strcmp(option, "t1"))
		{
			SettingT1(ctx, value);
		}
		else if (!strcmp(option, "t2"))
		{
			SettingT2(ctx, value);
		}
		else if (!strcmp(option, "r1"))
		{
			SettingR1(ctx, value);
		}
		else if (!strcmp(option, "r2"))
		{
			SettingR2(ctx, value);
		}
		else if (!strcmp(option, "q"))
		{
			quit = true;
		}

	} while (!quit);
}

void PrintTagScanInfos(appContext *ctx)
{
	invGen2DataHeader Gen2Header;
	invGen2Tag *tagsInfo;
	unsigned short tagDetected = 0;
	memset(&Gen2Header, 0, sizeof(invGen2DataHeader));

	if (!CmdTagDetected(ctx->serialHdl, &tagDetected))
	{
		ctx->readCount += tagDetected;
		ctx->tagDetected = tagDetected;
	}
}

int Connect(appContext *ctx)
{
	int status = -1;
	int count = 0;
	char comName[7] = "COM0";
	while (ctx->state != STATE_ONLINE && count <= 255)
	{
		sprintf(comName, "COM%d", count);
		DEBUG("Trying %s\n", comName);

		if (!ctx->serialHdl->Init(comName, 115200, 0, 1, 8))
		{
			ctx->serialHdl->SetRts(true);
			Sleep(100);
			ctx->serialHdl->SetRts(false);
			Sleep(100);
			ctx->serialHdl->Start();

			// If the serial is open we now need to check this is the RFID reader device
			if(!CheckConnection(ctx->serialHdl))
			{
				status = 0;
				ctx->state = STATE_ONLINE;
				sprintf(ctx->infoBar, "Connected to %s", comName);
			}
			else
			{
				ctx->serialHdl->Stop();
				ctx->serialHdl->UnInit();
			}
		}
		count++;
	}

	return status;
}

int main()
{
	appContext ctx;
	memset(&ctx, 0, sizeof(appContext));

	ctx.action = ACTION_IDLE;
	ctx.state = STATE_OFFLINE;
	sprintf(ctx.infoBar, "");

	CSerialCommHelper serialHandle = CSerialCommHelper();
	ctx.serialHdl = &serialHandle;


	do
	{
		system("cls");
		PrintMenu(&ctx);
		PrintInfos(&ctx);

		if (GetAsyncKeyState(VK_F1))
		{
			if (ctx.action != ACTION_IDLE)
			{
				sprintf(ctx.infoBar,"Stop scanning before disconecting");
				continue;
			}

			if (ctx.state == STATE_ONLINE)
			{
				ctx.state = STATE_OFFLINE;
				ctx.serialHdl->Stop();
				ctx.serialHdl->UnInit();
			}
			else if (ctx.state == STATE_OFFLINE)
			{
				if (!Connect(&ctx))
				{
					CmdGetFwVersion(ctx.serialHdl, &ctx.swVerMajor, &ctx.swVerMinor, &ctx.swVerRev);
					CmdGetFwInfo(ctx.serialHdl, ctx.hwInfo, ctx.swInfo);
					//ReaderSetConfig(ctx.serialHdl);
				}
				else
				{
					sprintf(ctx.infoBar, "Connection error, reader not found");
				}
			}
		}
		else if (GetAsyncKeyState(VK_F2))
		{
			if (ctx.state != STATE_ONLINE)
			{
				sprintf(ctx.infoBar, "Can't start scan, please connect to the reader");
				continue;
			}

			if (ctx.action == ACTION_SCANNING)
			{
				ctx.action = ACTION_IDLE;
				CmdReaderOnOff(ctx.serialHdl, READER_SCAN_OFF);
				//ReaderStopScan(ctx.serialHdl);
			}
			else if (ctx.action == ACTION_IDLE)
			{
				ctx.action = ACTION_SCANNING;
				ctx.readCount = 0;
				CmdReaderOnOff(ctx.serialHdl, READER_SCAN_ON);
				//ReaderStartScan(ctx.serialHdl);
			}
		}
		else if (GetAsyncKeyState(VK_F3))
		{
			if (ctx.action != ACTION_IDLE)
			{
				sprintf(ctx.infoBar, "Stop scanning before changing settings");
				continue;
			}

			if (ctx.state != STATE_ONLINE)
			{
				sprintf(ctx.infoBar, "You need to connect to the reader first");
				continue;
			}

			MenuSettings(&ctx);
		}

		if (ctx.action == ACTION_SCANNING)
		{
			PrintTagScanInfos(&ctx);
		}

		usleep(50M)
	}
	while(1);

	ctx.serialHdl->Stop();
	ctx.serialHdl->UnInit();
	return 0;
}