/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * 	Modification Log:
 * 	-----------------
 *	.01 joh 060691	removed 4 byte count from the beginning of
 *			of each message
 *	.02 joh 060791	moved send_msg stuff into caserverio.c
 *	.03 joh 071291	moved time stamp from client to the
 *			channel in use block
 *	.04 joh 071591	added ticks at last io to the client structure
 *	.05 joh	103191	moved lock from msg buf to client structure
 *	.06 joh	050692	added declaration for cac_send_heartbeat()
 *	.07 joh 022492	added get flag to the event ext block
 *	.08 joh 090893	added sid field to channel in use block 
 *
 */

#ifndef INCLserverh
#define INCLserverh

static char *serverhSccsId = "@(#) $Id$";

#if defined(CAS_VERSION_GLOBAL) && 0
#       define HDRVERSIONID(NAME,VERS) VERSIONID(NAME,VERS)
#else /*CAS_VERSION_GLOBAL*/
#       define HDRVERSIONID(NAME,VERS)
#endif /*CAS_VERSION_GLOBAL*/

#define APIENTRY

#include <epicsAssert.h>

#include <vxLib.h>
#include <ellLib.h>
#include <fast_lock.h>

#include <dbDefs.h>
#include <db_access.h>
#include <dbEvent.h>
#include <caProto.h>
#include <bucketLib.h>
#include <taskwd.h>

#include <asLib.h>
#include <asDbLib.h>

#include <socket.h>
#include <addrList.h>


#include <net_convert.h>

/*
 * !! buf must be the first item in this structure !!
 * This guarantees that buf will have 8 byte natural
 * alignement
 *
 * Conversions:
 * The contents of message_buffer has to be converted
 * from network to host format and vice versa.
 * For efficiency reasons, the caHdr structure that's common
 * to all messages is converted only once:
 * 1) from net to host just after receiving it in camessage()
 * 2) from host to net in cas_send_msg()
 * 
 * The remaining message_buffer content, however, is always
 * in net format!
 *
 */
struct message_buffer{
  char 				buf[MAX_MSG_SIZE];
  unsigned long 		stk;
  unsigned long			maxstk;
  unsigned long			cnt;			
};

struct client{
  ELLNODE			node;
  FAST_LOCK			lock;
  FAST_LOCK			putNotifyLock;
  FAST_LOCK			addrqLock;
  FAST_LOCK			eventqLock;
  ELLLIST			addrq;
  ELLLIST			putNotifyQue;
  struct sockaddr_in		addr;
  struct message_buffer		send;
  struct message_buffer		recv;
  unsigned long			ticks_at_last_send;
  unsigned long			ticks_at_last_recv;
  void				*evuser;
  SEM_ID			blockSem; /* used whenever the client blocks */
  int				sock;
  int				proto;
  int				tid;
  unsigned			minor_version_number;
  char				eventsoff;
  char				disconnect;	/* disconnect detected */
  char				*pUserName;
  char				*pHostName;
};


/*
 * for tracking db put notifies
 */
typedef struct rsrv_put_notify{
	ELLNODE		node;
	PUTNOTIFY	dbPutNotify;
	caHdr		msg;
	unsigned long	valueSize; /* size of block pointed to by dbPutNotify */
	int		busy; /* put notify in progress */
}RSRVPUTNOTIFY;


/*
 * per channel structure 
 * (stored in addrq off of a client block)
 */
struct channel_in_use{
  	ELLNODE		node;
  	ELLLIST		eventq;
  	struct client	*client;
  	RSRVPUTNOTIFY	*pPutNotify; /* potential active put notify */
  	const unsigned 	cid;	/* client id */
  	const unsigned 	sid;	/* server id */
  	unsigned long	ticks_at_creation;	/* for UDP timeout */
  	struct dbAddr	addr;
  	ASCLIENTPVT	asClientPVT;
};


/*
 * Event block extension for channel access
 * some things duplicated for speed
 */
struct event_ext{
ELLNODE				node;
caHdr				msg;
struct channel_in_use		*pciu;
struct event_block		*pdbev;		/* ptr to db event block */
unsigned			size;		/* for speed */
unsigned			mask;
char				modified;	/* mod & ev flw ctrl enbl */
char				send_lock;	/* lock send buffer */
char				get;		/* T: get F: monitor */
};


/*	NOTE: external used so they remember the state across loads */
#ifdef 	GLBLSOURCE
# define GLBLTYPE
# define GLBLTYPE_INIT(A)
#else
# define GLBLTYPE extern
# define GLBLTYPE_INIT(A)
#endif


/*
 *	for debug-level dependent messages:
 */
#ifdef DEBUG

#	define DLOG(level, fmt, a1, a2, a3, a4, a5, a6)	\
		if (CASDEBUG > level)			\
			logMsg (fmt, a1, a2, a3, a4, a5, a6)

#	define DBLOCK(level, code)			\
		if (CASDEBUG > level)			\
		{					\
			code;				\
		}

#else

#	define DLOG(level, fmt, a1, a2, a3, a4, a5, a6)	
#	define DBLOCK(level, code)		

#endif

GLBLTYPE int			CASDEBUG;
GLBLTYPE int 		 	IOC_sock;
GLBLTYPE int			IOC_cast_sock;
GLBLTYPE unsigned short		ca_server_port;
GLBLTYPE ELLLIST		clientQ;	/* locked by clientQlock */
GLBLTYPE ELLLIST		rsrv_free_clientQ; /* locked by clientQlock */
GLBLTYPE ELLLIST		rsrv_free_addrq;
GLBLTYPE ELLLIST		rsrv_free_eventq;
GLBLTYPE ELLLIST		beaconAddrList;
GLBLTYPE FAST_LOCK		clientQlock;
GLBLTYPE FAST_LOCK		rsrv_free_addrq_lck;
GLBLTYPE FAST_LOCK		rsrv_free_eventq_lck;
GLBLTYPE struct client		*prsrv_cast_client;
GLBLTYPE BUCKET            	*pCaBucket;

#define CAS_HASH_TABLE_SIZE 4096

/*
 * set true if max memory block drops below MAX_BLOCK_THRESHOLD
 */
#define MAX_BLOCK_THRESHOLD 100000
GLBLTYPE int	casBelowMaxBlockThresh;

#define SEND_LOCK(CLIENT)\
{\
FASTLOCK(&(CLIENT)->lock);\
}

#define SEND_UNLOCK(CLIENT)\
{ \
FASTUNLOCK(&(CLIENT)->lock);\
}

#define EXTMSGPTR(CLIENT)\
 ((caHdr *) &(CLIENT)->send.buf[(CLIENT)->send.stk])

/*
 *	ALLOC_MSG 	get a ptr to space in the buffer
 *	END_MSG 	push a message onto the buffer stack
 *
 */
#define	ALLOC_MSG(CLIENT, EXTSIZE)	cas_alloc_msg(CLIENT, EXTSIZE)

#define END_MSG(CLIENT)\
  EXTMSGPTR(CLIENT)->m_postsize = CA_MESSAGE_ALIGN(EXTMSGPTR(CLIENT)->m_postsize),\
  (CLIENT)->send.stk += sizeof(caHdr) + EXTMSGPTR(CLIENT)->m_postsize


#define LOCK_CLIENTQ	FASTLOCK(&clientQlock);

#define UNLOCK_CLIENTQ	FASTUNLOCK(&clientQlock);

struct client	*existing_client();
int		camsgtask();
void		cas_send_msg();
caHdr		*cas_alloc_msg();
int		rsrv_online_notify_task();
void		cac_send_heartbeat();

int 		client_stat(unsigned level);
void 		casr(unsigned level);
int 		req_server(void);
int		cast_server(void);
int 		free_client(struct client *client);
struct client 	*create_udp_client(unsigned sock);
int 		udp_to_tcp(struct client *client, unsigned sock);

int camessage(
struct client  *client,
struct message_buffer *recv
);

void cas_send_heartbeat(
struct client   *pc
);

void write_notify_reply(void *pArg);

void *casMalloc(size_t size);
void *casCalloc(size_t count, size_t size);

/*
 * !!KLUDGE!!
 *
 * this was extracted from dbAccess.h because we are unable
 * to include both dbAccess.h and db_access.h at the
 * same time.
 */
#define S_db_Blocked 	(M_dbAccess|39) /*Request is Blocked*/
#define S_db_Pending    (M_dbAccess|37) /*Request is pending*/

#endif /*INCLserverh*/
