#include "sysinclude.h"

extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

#define FRAMESIZE (4*3+4*100+4)
#define MAXFRAME 10

//1���ػ�������
int low = 1;//�����½�
int high = 1;//�����Ͻ�
unsigned char sendbuffer[FRAMESIZE*MAXFRAME];//����֡����
unsigned int packetlen[MAXFRAME];//����֡����
int tail = 0;//����֡��β

/*
* ͣ��Э����Ժ���
*/
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType)
{
	if (MSG_TYPE_SEND == messageType) {
		memcpy(sendbuffer + tail * FRAMESIZE, pBuffer, bufferSize);
		packetlen[tail] = bufferSize;
		tail++;
		if (high != low) {
			return 0;
		}
		else {
			SendFRAMEPacket(sendbuffer + (high - 1) * FRAMESIZE, bufferSize);
			high = 1 + high;
		}
	}
	else if (MSG_TYPE_RECEIVE == messageType) {
		unsigned int* acknum = (unsigned int*) malloc(sizeof(unsigned int));
		memcpy(acknum, pBuffer+8, 4);
		*acknum = ntohl(*acknum);
		if (*acknum == low) {
			low = 1 + low;
			free(acknum);
			if (tail >= high) {
				SendFRAMEPacket(sendbuffer + (high - 1) * FRAMESIZE, packetlen[high - 1]);
				high = 1 + high;
			}
		}
		else {
			free(acknum);
			return 0;
		}
	}
	else if (MSG_TYPE_TIMEOUT == messageType) {
		SendFRAMEPacket(sendbuffer + (low - 1) * FRAMESIZE, packetlen[low - 1]);
	}
	return 0;
}

int nlow = 1;//�����½�
int nhigh = 1;//�����Ͻ�
unsigned char nsendbuffer[FRAMESIZE*MAXFRAME];
unsigned int npacketlen[MAXFRAME];
int ntail = 0;

/*
* ����n֡���Ժ���
*/
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType)
{
	if (MSG_TYPE_SEND == messageType) {
		memcpy(nsendbuffer + ntail * FRAMESIZE, pBuffer, bufferSize);
		npacketlen[ntail] = bufferSize;
		ntail++;
		if ((nhigh - nlow) == WINDOW_SIZE_BACK_N_FRAME) {
			return 0;
		}
		else {
			SendFRAMEPacket(nsendbuffer + (nhigh - 1) * FRAMESIZE, bufferSize);
			nhigh++;
		}
	}
	else if (MSG_TYPE_RECEIVE == messageType) {
		unsigned int* acknum = (unsigned int*) malloc(sizeof(unsigned int));
		memcpy(acknum, pBuffer+8, 4);
		*acknum = ntohl(*acknum);
		if (*acknum >= nlow) {
			nlow = (*acknum) + 1;
			free(acknum);
			while (ntail >= nhigh && (nhigh - nlow) < WINDOW_SIZE_BACK_N_FRAME) {
				SendFRAMEPacket(nsendbuffer + (nhigh - 1) * FRAMESIZE, npacketlen[nhigh - 1]);
				nhigh++;
			}
		}
		else {
			free(acknum);
			return 0;
		}
	}
	else if (MSG_TYPE_TIMEOUT == messageType) {
		unsigned int* timeoutnum = (unsigned int*) malloc(sizeof(unsigned int));
		memcpy(timeoutnum, pBuffer, 4);
		unsigned int resendstart = nlow;
		for (; resendstart <= (nhigh - 1); ++resendstart) {
			SendFRAMEPacket(nsendbuffer + (resendstart - 1) * FRAMESIZE, npacketlen[resendstart - 1]);
		}
		free(timeoutnum);
	}
	return 0;
}

/*
* ѡ�����ش����Ժ���
*/
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType)
{
	return 0;
}
