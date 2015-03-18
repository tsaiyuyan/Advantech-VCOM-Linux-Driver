#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include "advioctl.h"
#include "advtype.h"
#include "vcom_proto.h"
#include "vcom_proto_cmd.h"
#include "vcom_proto_ioctl.h"
#include "vcom.h"

extern struct vc_ops vc_netdown_ops;
static struct vc_ops vc_sync_ops;

#define ADV_THIS	(&vc_sync_ops)

struct vc_ops * vc_sync_jmp(struct vc_attr *attr, struct vc_ops * current)
{
	if(attr->pre_ops != 0){
		printf("loop call %s from unkown state\n", __func__);
	}
	
	attr->pre_ops = current;
	return ADV_THIS->init(attr);
	
}

struct vc_ops * vc_sync_jmp_recv(struct vc_attr * attr, struct vc_ops *current,
		char *buf, int len)
{
	if(attr->pre_ops != 0){
		printf("loop call %s from unkown state\n", __func__);
		exit(0);
	}
	
	attr->pre_ops = current;
	return ADV_THIS->recv(attr, buf, len);
}

static struct vc_ops * vc_sync_open(struct vc_attr * attr)
{
	printf("%s(%d)\n", __func__, __LINE__);
	exit(0); //this function was designed never to be called
	return ADV_THIS;
}

static struct vc_ops * vc_sync_close(struct vc_attr * attr)
{
	printf("%s(%d)\n", __func__, __LINE__);
	exit(0); //this function was designed never to be called
	vc_buf_clear(attr, ADV_CLR_RX|ADV_CLR_TX);

	return vc_netdown_ops.init(attr);
}

static struct vc_ops * vc_sync_error(struct vc_attr * attr, char * str, int num)
{
	printf("%s: %s(%d)\n", __func__, str, num);
	return vc_netdown_ops.init(attr);
}

static int _sync_event(struct vc_attr * attr)
{
	char pbuf[1024];
	char * dbg_msg;
	int plen;
	int init_step;
	struct vc_proto_packet * packet;
	
	packet = (struct vc_proto_packet *)pbuf;

	init_step = 0;
	dbg_msg = "(NULL)";

	while(1){

		switch(init_step){
		case 0:
			plen = vc_pack_setwmask(packet, attr->tid, 0x38010000, sizeof(pbuf));
			dbg_msg = "SET_WAIT_MASK";
			break;
		case 1:
			plen = vc_pack_getmodem(packet, attr->tid, sizeof(pbuf));
			dbg_msg = "GET_MODEMSTATUS";
			break;
		default:
			dbg_msg = "UNKNOWN";
			return 0;
		}	

		if(plen <= 0){
			printf("failed to create %s\n", dbg_msg);
			break;
		}

		if(fdcheck(attr->sk, FD_WR_RDY, 0) == 0){
			printf("cannot send %s\n", dbg_msg);
			break;
		}

		if(send(attr->sk, packet, plen, MSG_NOSIGNAL) != plen){
			printf("failed to  send %s\n", dbg_msg);
			break;
		}
		attr->tid++;
		init_step++;
	}

	return -1;

}

static int _reg_sync_event(struct vc_attr * attr)
{
	char pbuf[1024];
	int plen;
	struct vc_proto_packet * packet;
	
	packet = (struct vc_proto_packet *)pbuf;

	plen = vc_pack_womask(packet, attr->tid, sizeof(pbuf));

	if(plen <= 0){
		printf("failed to create WAIT_ON_MASK\n");
		return -1;
	}

	if(fdcheck(attr->sk, FD_WR_RDY, 0) == 0){
		printf("cannot send WAIT_ON_MASK\n");
		return -1;
	}

	if(send(attr->sk, packet, plen, MSG_NOSIGNAL) != plen){
		printf("failed to  send WAIT_ON_MASK\n");
		return -1;
	}
	attr->tid++;

	return 0;
}

static int _sync_queue(struct vc_attr * attr)
{
	char pbuf[1024];
	int plen;
	struct vc_proto_packet * packet;
	int qfsize;
	
	packet = (struct vc_proto_packet *)pbuf;

	if(is_rb_empty(attr->tx)){
		qfsize = 1024;
	}else{
		qfsize = 0;
	}

	plen = vc_pack_qfsize(packet, attr->tid, qfsize, sizeof(pbuf));

	if(plen <= 0){
		printf("failed to create QUEUE_FREE\n");
		return -1;
	}

	if(fdcheck(attr->sk, FD_WR_RDY, 0) == 0){
		printf("cannot send QUEUE_FREE\n");
		return -1;
	}

	if(send(attr->sk, packet, plen, MSG_NOSIGNAL) != plen){
		printf("failed to  send QUEUE_FREE\n");
		return -1;
	}
	attr->tid++;

	return 0;
}


static struct vc_ops * vc_sync_init(struct vc_attr * attr)
{
	struct vc_ops * nxt;

	if(_sync_event(attr)){
		printf("failed to sync\n");
		return vc_netdown_ops.init(attr);
	}

	if(_sync_queue(attr)){
		printf("failed to sync queue\n");
		return vc_netdown_ops.init(attr);
	}

	nxt = attr->pre_ops;
	attr->pre_ops = 0;

	return nxt->init(attr);
}

static struct vc_ops * vc_sync_recv(struct vc_attr * attr, char *buf, int len)
{
	struct vc_proto_packet * pbuf;
	struct vc_ops * nxt;
	unsigned int subcmd;
	unsigned int tid;
	unsigned int status;
	
	pbuf = (struct vc_proto_packet *)buf;

	tid = ntohl(pbuf->hdr.tid);
	subcmd = ntohl(pbuf->attach.param.p2);
	status = ntohl(pbuf->attach.param.p1);

	switch(subcmd){
		case VCOM_IOCTL_WAIT_ON_MASK:
			if(_sync_event(attr)){
				return vc_netdown_ops.init(attr);
			}
			break;
		case VCOM_IOCTL_SET_WAIT_MASK:
			if(_reg_sync_event(attr)){
				return vc_netdown_ops.init(attr);
			}
			break;
		case VCOM_IOCTL_GET_MODEMSTATUS:
		{
			unsigned int modem;

			modem = ntohl(pbuf->attach.uint32.uint);
			if(ioctl(attr->fd, ADVVCOM_IOCSMCTRL, &modem)){
				printf("ioctl(mctrl) failed\n");
				return vc_netdown_ops.init(attr);
			}
			break;	
		}
		default:
			printf("recv ioctl tid %u cmd %x status %x\n", tid, subcmd, status);

	}
	
//	return attr->pre_ops->init(attr);
	nxt = attr->pre_ops;
	attr->pre_ops = 0;

	return nxt->init(attr);

}


#undef ADV_THIS

static struct vc_ops vc_sync_ops = {
	.init = vc_sync_init,
	.err = vc_sync_error,
	.recv = vc_sync_recv,
	.open = vc_sync_open,
	.close = vc_sync_close,
};