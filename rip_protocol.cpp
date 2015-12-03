#include "sysinclude.h"

extern void rip_sendIpPkt(unsigned char *pData, UINT16 len,unsigned short dstPort,UINT8 iNo);

extern struct stud_rip_route_node *g_rip_route_table;

#define DSTPORT 520

struct RIPEntry {
    short afi;
    short rt;
    unsigned int dest;
    unsigned int mask;
    unsigned int nexthop;
    unsigned int metric;
};

struct RIPpac {
    char command;
    char version;
    short must_be_zero;
    RIPEntry ripentry[25];
    RIPpac() {
        command = 0;
        version = 2;
        must_be_zero = 0;
        memset(ripentry, 0, sizeof(RIPEntry) * 25);
    }
};
//������iNo��Ӧ��rip��,ע�����в�������iNo��ͬ��entry(ˮƽ�����㷨)
void make_res_rip_pac(RIPpac* resrippac, UINT16* len, UINT8 iNo) {
    resrippac->command = 2;
    int entrytail = 0;
    stud_rip_route_node* pointer = g_rip_route_table;
    while (pointer != NULL) {
        if (pointer->if_no != iNo) {
            resrippac->ripentry[entrytail].afi = htons(2);
            resrippac->ripentry[entrytail].rt = 0;
            resrippac->ripentry[entrytail].dest = htonl(pointer->dest);
            resrippac->ripentry[entrytail].mask = htonl(pointer->mask);
            resrippac->ripentry[entrytail].nexthop = htonl(pointer->nexthop);
            resrippac->ripentry[entrytail].metric = htonl(pointer->metric);
            ++entrytail;
        }
        pointer = pointer->next;
    }
    (*len) = 1 + 1 + 2 + entrytail * sizeof(RIPEntry);
}
//����IP��Mask�ҵ�����
stud_rip_route_node* find_entry(unsigned int dest, unsigned int mask) {
    stud_rip_route_node* pointer = g_rip_route_table;
    while (pointer != NULL) {
        if (pointer->dest == dest && pointer->mask == mask) {
            break;
        }
        pointer = pointer->next;
    }
    return pointer;
}

int stud_rip_packet_recv(char *pBuffer,int bufferSize,UINT8 iNo,UINT32 srcAdd)
{   
    //�Ϸ��Լ��
    char command = pBuffer[0];
    char version = pBuffer[1];
    if (command != 1 && command != 2) {
        ip_DiscardPkt(pBuffer, STUD_RIP_TEST_COMMAND_ERROR);
        return -1;
    }
    else if (version != 2) {
        ip_DiscardPkt(pBuffer, STUD_RIP_TEST_VERSION_ERROR);
        return -1;
    }

    if (command == 1) { //request,���ͻص�ǰ����
        RIPpac resrippac;
        UINT16 len = 0;
        make_res_rip_pac(&resrippac, &len, iNo);
        rip_sendIpPkt((unsigned char*)(&resrippac), len, DSTPORT, iNo);
    }
    else if (command == 2) { //response,�޸ı���
        RIPpac* formatted = (RIPpac*)pBuffer;
        int entrynum = (bufferSize - 4) / (sizeof(RIPEntry));
        for (int i = 0; i < entrynum; ++i) {
            formatted->ripentry[i].dest = ntohl(formatted->ripentry[i].dest);
            formatted->ripentry[i].mask = ntohl(formatted->ripentry[i].mask);
            formatted->ripentry[i].nexthop = ntohl(formatted->ripentry[i].nexthop);
            formatted->ripentry[i].metric = ntohl(formatted->ripentry[i].metric);
            stud_rip_route_node* pointer = find_entry(formatted->ripentry[i].dest, formatted->ripentry[i].mask);
            if (pointer == NULL) { //��������ֱ�����,nexthopΪsrcAdd(���յ��ķ����ԴIP��ַ)
                if (formatted->ripentry[i].metric + 1 < 16) {
                    stud_rip_route_node* newnode = new stud_rip_route_node;
                    newnode->dest = formatted->ripentry[i].dest;
                    newnode->mask = formatted->ripentry[i].mask;
                    newnode->nexthop = srcAdd;
                    newnode->metric = formatted->ripentry[i].metric + 1;
                    newnode->if_no = iNo;
                    newnode->next = g_rip_route_table;
                    g_rip_route_table = newnode;
                }
            }
            else if (pointer->nexthop == srcAdd) { //������nexthop��srcAdd���,ǿ�Ƹ���
                (formatted->ripentry[i].metric + 1 > 16) ? pointer->metric = 16 : pointer->metric = formatted->ripentry[i].metric + 1;
                pointer->if_no = iNo;
            }
            else {
                if (pointer->metric > formatted->ripentry[i].metric) { //������nexthop��srcAdd����,�ȽϺ����
                    pointer->metric = (formatted->ripentry[i].metric + 1 > 16) ? 16 : formatted->ripentry[i].metric + 1;
                    pointer->nexthop = srcAdd;
                    pointer->if_no = iNo;
                }
            }
        }
    }
    return 0;
}

void stud_rip_route_timeout(UINT32 destAdd, UINT32 mask, unsigned char msgType)
{
    if (msgType == RIP_MSG_SEND_ROUTE) {
        RIPpac rippac;
        UINT16 len = 0;
        make_res_rip_pac(&rippac, &len, 1);
        rip_sendIpPkt((unsigned char*)(&rippac), len, DSTPORT, 1);
        make_res_rip_pac(&rippac, &len, 2);
        rip_sendIpPkt((unsigned char*)(&rippac), len, DSTPORT, 2);
    }
    else if (msgType == RIP_MSG_DELE_ROUTE) {
        stud_rip_route_node* pointer = find_entry(destAdd, mask);
        if (pointer != NULL) {
            pointer->metric = 16;
        }
    }
}
