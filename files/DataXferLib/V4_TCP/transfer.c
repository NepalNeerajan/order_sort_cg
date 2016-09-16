/*******************************************************************************
*  transfer.c            Interface procedures for general DA program use
*/

/*

    There are 2 fundamental options

   MAXIDS    -   permits a data acquisition program to transfer to multiple (possibly different) destinations
               -   implementation permits a multithreaded data acquisition program to use a separate connection for each thread

   TSOVERLAP    -  permits 2 buffers to be used to allow acquisition while data transfer is in progress
                -  uses threads to implement and so requires pthread support with mutexes and signals

*/

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif

#ifdef UNIX
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <errno.h>
#include "msg.h"

#include "transfer.h"

#ifdef TSOVERLAP
#define VERSION "4.0T"
#else
#define VERSION "4.0"
#endif

#define MAXIDS 8

unsigned long transferSequence[MAXIDS];  /*  block sequence number */

/* Default port number is based on MIDAS_TAPE_SERVER (RPC program number) + 100
 * i.e. if MIDAS_TAPE_SERVER is 28000205 TS_TCP_PORT is 10305
*/

#define TS_TCP_PORT 10305

/* for Mode = 4  (GSI/MBS compatible) the default port is 6500 */

#define MBS_TCP_PORT 6500

unsigned short Port[MAXIDS] = {0, 0, 0, 0, 0, 0, 0, 0};

unsigned char tsHost[MAXIDS][80];

int verbose = 0;

int BlockSize[MAXIDS] = {16384, 16384, 16384, 16384, 16384, 16384, 16384, 16384};      /*  units bytes  -  16K bytes is default */
int TferMode[MAXIDS] = {1, 1, 1, 1, 1, 1, 1, 1};

#define MINSNDBUFSIZE 32          /*  units Kbytes  */
#define MAXSNDBUFSIZE 4*1024      /* default max attempted */
#define MINRCVBUFSIZE 32          /*  units Kbytes  */
#define MAXRCVBUFSIZE 4*1024      /* default max attempted */

int SndBufSize[MAXIDS] = {MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE,MAXSNDBUFSIZE};   /*  units Kbytes */
int RcvBufSize[MAXIDS] = {MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE,MAXRCVBUFSIZE};   /*  units Kbytes */

int Endian[MAXIDS] = {1, 1, 1, 1, 1, 1, 1, 1};     /* default same as host   -     OK for PPC + VME   */

#ifndef WIN32
    #define SOCKET int
    #define SOCKET_ERROR -1
#endif

#ifdef WIN32
    WSADATA wsaData;
    int WSADATApres[MAXIDS] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif

#ifdef TSOVERLAP
#include <pthread.h>
#include <sched.h>

pthread_mutex_t M1[MAXIDS];
pthread_cond_t  C1[MAXIDS];
pthread_mutex_t M2[MAXIDS];
pthread_cond_t  C2[MAXIDS];

int pthreads_started[MAXIDS] = {0, 0, 0, 0, 0, 0, 0, 0};
int pthread_running[MAXIDS];

/*    arguments for request passed to thread */
SOCKET pthread_tsSocket[MAXIDS];
char * pthread_blockptr[MAXIDS];
int pthread_len[MAXIDS];

#ifndef WIN32
void signal_block();
#else
void signal_block() {};
#endif

#endif

/*    these require threads to be available */
int OverlapMode[MAXIDS] = {0, 0, 0, 0, 0, 0, 0, 0};
int NiceValue = 0;

/*    socket no block control   */
int NoBlock = 0;

SOCKET tsSocket[MAXIDS] = {0, 0, 0, 0, 0, 0, 0, 0};

/*
int msgDefineMessage (u_int id, u_int xclass, u_int level, char *source, char *body) {

 // printf("id=%d, class=%d, level=%d, source=%s: %s\n",id, xclass, level, source, body); 
  return OK;
}
*/

#ifdef WIN32
void transferReportError () {

  LPVOID lpMsgBuf;
  DWORD ErrNo;
  char MsgBuf[256];
  
       ErrNo = WSAGetLastError();
       FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      ErrNo,
                      0,
                      (LPTSTR) &lpMsgBuf,
                      0,
                      NULL
                      );
                      
       sprintf(MsgBuf, "Transfer Error %d - %s", ErrNo, lpMsgBuf);
       fprintf(stderr, "%s", MsgBuf);
       msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
       LocalFree (lpMsgBuf);
}
#else
void transferReportError () {
    char MsgBuf[256];

       perror("Transfer Error - ");
       sprintf(MsgBuf, "Error %d", errno);
       msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
}
#endif

#ifdef TSOVERLAP
/*******************************************************************************
    sends any pending buffer
    loops until all data has been sent  (or ERROR)

    there is one thread for each USER
*/
void * Thread_Send(void * i)
{
   SOCKET S;
   char * p;
   int l;

   int rc, done, left;
   int ID = 0;
   int nicevalue = 0;
   char *ErrMsg;
    char MsgBuf[256];

/*
  (int) ((long) i) is allowed in 64-bit compilation
  (int) i;  isn't.
*/

   ID = (int) ((long) i);    /* this is the thread (USER) number */

   if (verbose) printf("pthread %d running\n", ID);

   signal_block();   /* block signals handled by the main thread */

   for (;;) {

      if (verbose) printf("Thread_Send waiting\n");

/*    wait on the conditional variable C1 for work to be done */

      rc = pthread_mutex_lock(&M1[ID]);
      if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_lock 1: %s\n", ErrMsg);}

      while (!pthread_running[ID]) {
          rc = pthread_cond_wait (&C1[ID], &M1[ID]);   /* this will block until signaled */

#ifdef WIN32
          if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_wait 1: %s\n", ErrMsg); Sleep(1000);}
#else
          if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_wait 1: %s\n", ErrMsg); sleep(1);}
#endif

      }

      rc = pthread_mutex_unlock(&M1[ID]);
      if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_unlock 1: %s\n", ErrMsg);}

      if (verbose) printf("Thread_Send running\n");

      if (NiceValue != nicevalue) {    /* check if nice has changed */
          nicevalue = NiceValue;  
#ifndef WIN32
          nice(NiceValue);
#endif
      }

/*    pick up request argunments  */

      S = pthread_tsSocket[ID];
      p = pthread_blockptr[ID];
      l = pthread_len[ID];

      done = 0;
      left =  l;

      while (done < l) {
         rc = send(S, p+done, left, 0);
         if (rc == SOCKET_ERROR) {
            transferReportError();
            sprintf(MsgBuf, "send() failed: ");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            transferMultiClose(ID);
            break;
         }
         done += rc;
         left -= rc;
       }

       if (verbose) printf("Thread_Send finishing\n");

/*    signal the conditional variable C2 that the work has been completed */

       rc = pthread_mutex_lock(&M2[ID]);
       if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_lock 2: %s\n", ErrMsg);}

       pthread_running[ID] =0;          /* clear the running flag */

       rc = pthread_cond_signal(&C2[ID]);   /* signal the thread */
       if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_signal 2: %s\n", ErrMsg);}

       rc = pthread_mutex_unlock(&M2[ID]);  /*   allow it to run */
       if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_unlock 2: %s\n", ErrMsg);}
    }

    return ((void *)0);
}
/*******************************************************************************/
void do_send_thread(int ID, char * p, int l)
{
    int rc;
   char *ErrMsg;

/*    start the send thread
      signal the conditional variable C1 that there is work to be done
*/

        rc = pthread_mutex_lock(&M1[ID]);
        if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_lock 3: %s\n", ErrMsg);}

        pthread_tsSocket[ID] = tsSocket[ID];   /*  save the thread arguments  */
        pthread_blockptr[ID] = p;
        pthread_len[ID] = l;

        pthread_running[ID] =1;         /*  set the running flag */

        rc = pthread_cond_signal(&C1[ID]);   /* signal the thread */
        if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_signal 3: %s\n", ErrMsg);}

        rc = pthread_mutex_unlock(&M1[ID]);  /* allow it to run */
        if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_unlock 3: %s\n", ErrMsg);}
}
/********************************************************************************
*    transferTxWait       wait for any pending transfer to complete
*
*/
int transferMultiTxWait (int ID)
{
    int rc;
    int retval;
   char *ErrMsg;

        if (verbose) printf("TransferTxWait waiting\n");

/*     if (!pthread_running[ID]) return 0; */

/*    wait on the conditional variable C2 for completion */

       retval = 0;

        rc = pthread_mutex_lock(&M2[ID]);
        if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_lock 4: %s\n", ErrMsg);}

        while (pthread_running[ID]) {
            rc = pthread_cond_wait(&C2[ID], &M2[ID]);   /* this will block until signaled */

#ifdef WIN32
          if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_wait 4: %s\n", ErrMsg); Sleep(1000);}
#else
          if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_cond_wait 4: %s\n", ErrMsg); sleep(1);}
#endif

            retval = 1;
        }

        rc = pthread_mutex_unlock(&M2[ID]);
        if (rc != 0) {ErrMsg = strerror(rc); printf ("error in pthread_mutex_unlock 4: %s\n", ErrMsg);}

        if (verbose) printf("TransferTxWait complete\n");

     return retval;
}
#else
int transferMultiTxWait (int ID) {return 0;}              /*   dummy for the non thread case */
#endif
int transferTxWait() {return transferMultiTxWait(0);}
/*******************************************************************************
    same parameter list as send
    loops until all data has been sent  (or ERROR)
*/
int do_send(SOCKET S, char * p, int l)
{
   int rc, done, left;
   int j;

    if (verbose) {
        printf("sending %d bytes\n",l);

        for (j = 0; j < l;) {
           printf(" 0x%02x", p[j]);
           j++;
           if ((j/16)*16 == j) printf("\n");
        }
        printf("\n");
    }

    done = 0;
    left =  l;

    while (done < l) {
        rc = send(S, p+done, left, 0);
        if (rc == SOCKET_ERROR) return rc;
        done += rc;
        left -= rc;
    }

    return l;
}

/*******************************************************************************
    same parameter list as recv
    loops until all data has been received  (or ERROR)
*/
int do_recv(SOCKET S, char * p, int l, int t)
{
   int rc, done, left;

    done = 0;
    left =  l;
    while (done < l) {
        rc = recv(S, p+done, left, t);
        if (rc == SOCKET_ERROR)
             return rc;
        if (rc == 0) break;
        done += rc;
        left -= rc;
    }

    return l;
}

/*******************************************************************************
* transferInit   initialise interface
*            return OK or ERROR
*    tsIPStr is a char* to the name of the Tape Server
*/
int transferMultiInit (int ID, char *tsIPStr)
{
    int rs;

    if (ID < 0 || ID >= MAXIDS) return ERROR;
    
    strcpy (tsHost[ID], tsIPStr);

    rs = transferMultiTxRestart(ID);

    return rs;
}
int transferInit(char *tsIPStr) {return transferMultiInit(0, tsIPStr);}
/*******************************************************************************
* transferEndian        allows a custom value to be set
*/
int transferMultiEndian(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    Endian[ID] = n;
    printf("Setting Endian %d\n",Endian[ID]);
    return OK;
}
int transferEndian(int n) {return transferMultiEndian(0, n);}
/*******************************************************************************
* transferBlockSize        allows a custom size to be set
*/
int transferMultiBlockSize(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    BlockSize[ID] = n;
    printf("Setting Transfer Block Size %d\n",BlockSize[ID]);
    return OK;
}
int transferBlockSize(int n) {return transferMultiBlockSize(0, n);}
/*******************************************************************************
* transferSndBufSize        allows a custom size to be set
*/
int transferMultiSndBufSize(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    SndBufSize[ID] = n;
    printf("Setting TCP send buffer size %dK\n",SndBufSize[ID]);
    return OK;
}
int transferSndBufSize(int n) {return transferMultiSndBufSize(0, n);}
/*******************************************************************************
* transferRcvBufSize        allows a custom size to be set
*/
int transferMultiRcvBufSize(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    RcvBufSize[ID] = n;
    printf("Setting TCP receive buffer size %dK\n",RcvBufSize[ID]);
    return OK;
}
int transferRcvBufSize(int n) {return transferMultiRcvBufSize(0, n);}
/*******************************************************************************
* transferMode        allows the mode to be set
*/
int transferMultiMode(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    TferMode[ID] = n;
    printf("Setting Transfer Mode %d\n",TferMode[ID]);
    return OK;
}
int transferMode(int n) {return transferMultiMode(0, n);}
/*******************************************************************************
* transferBlockMode        allows the mode to be set
*/
int transferMultiBlockMode(int ID, int n)
{
    NoBlock = n;
    printf("Setting Blocking Mode %d\n",NoBlock);

    return OK;
}
int transferBlockMode(int n) {return transferMultiBlockMode(0, n);}
/*******************************************************************************
* transferUseOverlap        enables the overlap mode - needs threads
*/
int transferMultiUseOverlap(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    OverlapMode[ID] = n;
    printf("Setting Overlap Mode %d\n",OverlapMode[ID]);
    return OK;
}
int transferUseOverlap(int n) {return transferMultiUseOverlap(0, n);}
/*******************************************************************************
* transferNice        sets the nice value - needs threads
*/
int transferMultiNice(int ID, int n)
{
    NiceValue = n;
    printf("Setting nice %d\n",NiceValue);
    return OK;
}
int transferNice(int n) {return transferMultiNice(0, n);}
/*******************************************************************************
* transferPort        allows a custom port to be set
*/
int transferMultiPort(int ID, int n)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    Port[ID] = (unsigned short) n;
    return OK;
}
int transferPort(int n) {return transferMultiPort(0, n);}
/*******************************************************************************
* transferSetVerbose        allows more debug information 
*/
int transferSetVerbose(int n)
{
    verbose = (unsigned short) n;
    return OK;
}
/*******************************************************************************
* transferSetUser        allows current user to be set  - this is no longer required 
*/
int transferSetUser(int n) {return OK;}
/*******************************************************************************
* tsOpenClient            initial function to establish host/client connection
*                        create a socket
*                        find the host
*                        create a client
*                        returns client pointer or NULL
*/
int tsOpenClient (int ID, char *tsIPStr)
{         
       int retval;
       struct hostent *hp;
       unsigned int addr;
       int sndsize, sndreq, rcvsize, rcvreq, temp, length;
#ifdef TSOVERLAP
       pthread_t thread;
       pthread_attr_t attr;
#endif
    char MsgBuf[256];

    struct sockaddr_in  server;


    sprintf(MsgBuf, "TCP transfer library version %s",VERSION);
    printf ("%s\n", MsgBuf);
    msgDefineMessage (0, MSG_C_EGLK, MSG_S_INFORMATION, NULL, MsgBuf);

    if (ID < 0 || ID >= MAXIDS) return ERROR;

#ifdef TSOVERLAP

/*    start thread which will handle the data block send     */

    if (!pthreads_started[ID]) {

      retval = pthread_attr_init(&attr);
      if (retval != 0) perror("thread attribute");
      retval = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);  /* we never use join */
      if (retval != 0) perror("thread attribute");

         pthread_running[ID] = 0;

         pthread_mutex_init(&M1[ID], NULL);
         pthread_mutex_init(&M2[ID], NULL);
         pthread_cond_init(&C1[ID], NULL);
         pthread_cond_init(&C2[ID], NULL);

/* double cast to get past 64-bit compiler */
         retval = pthread_create(&thread, &attr, Thread_Send, (void *)((long)ID));
         if (retval != 0) {
            transferReportError();
            sprintf(MsgBuf, "Client: Error creating pthread: ");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            return ERROR;
         }

      pthread_attr_destroy(&attr);
      pthreads_started[ID] =1;

#ifdef WIN32
//      Sleep(2000);
#else
//      sleep(2);     /* allows the thread to get running */
#endif
   }
#endif

#ifdef LYNXOS
    if (!sigpipe) {
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags=0;

        (void) sigaction(SIGPIPE, &sa, NULL);
        sigpipe=1;
    }
#endif

#ifdef WIN32
    if (WSADATApres[ID] == 0) {
        retval = WSAStartup(MAKEWORD( 2, 2),&wsaData);
        if (retval != 0) {
            WSASetLastError(retval);
            transferReportError();
            sprintf(MsgBuf, "WSAStartup failed:");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            return ERROR;
        }
        WSADATApres[ID] = 1;
     }
#endif

     if (Port[ID] == 0) {
         Port[ID] = TS_TCP_PORT;
         if (TferMode[ID] == 4) Port[ID] = MBS_TCP_PORT;
     }

     memset(&server,0,sizeof(server));
     server.sin_port = htons(Port[ID]);
    
    /* Attempt to detect if we should call gethostbyname() or gethostbyaddr() */
    
    if (isalpha(tsIPStr[0])) {   /* server address is a name */
        hp = gethostbyname(tsIPStr);
        if (hp == NULL ) {
            sprintf(MsgBuf,"Cannot resolve address [%s]: ", tsIPStr);
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            transferMultiClose(ID);
            return ERROR;
        }
        server.sin_family = hp->h_addrtype;
        memcpy(&(server.sin_addr),hp->h_addr,hp->h_length);
    }
    else  { /* Convert nnn.nnn address to a usable one */
        addr = inet_addr(tsIPStr);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = addr;
    }

    tsSocket[ID] = socket(AF_INET, SOCK_STREAM, 0); /* Open a TCP socket */
    if (tsSocket[ID] < 0 ) {
        transferReportError();
        sprintf(MsgBuf, "Client: Error Opening socket: ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
        transferMultiClose(ID);
        return ERROR;
    }
    
/*    optimize TCP send buffer size    */

    length=sizeof(sndsize);
    if (getsockopt(tsSocket[ID],SOL_SOCKET,SO_SNDBUF,(char *)&sndsize,&length) == SOCKET_ERROR) {
        transferReportError();
        sprintf(MsgBuf, "getsockopt() #1 failed:  ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
    }
    temp = sndsize;

    for (sndreq = MINSNDBUFSIZE; sndreq <= SndBufSize[ID]; sndreq = sndreq+MINSNDBUFSIZE) {
        sndsize = sndreq*1024;
        if (setsockopt(tsSocket[ID],SOL_SOCKET,SO_SNDBUF,(char *)&sndsize,sizeof(sndsize)) == SOCKET_ERROR) {
/*
              transferReportError();
//            sprintf(MsgBuf, "setsockopt() failed:  ");
//            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
//            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
*/
              break;
        }
    }

    length=sizeof(sndsize);
    if (getsockopt(tsSocket[ID],SOL_SOCKET,SO_SNDBUF,(char *)&sndsize,&length) == SOCKET_ERROR) {
        transferReportError();
        sprintf(MsgBuf, "getsockopt() #2 failed:  ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
    }
    
    sprintf(MsgBuf, "TCP socket send buffer was %d - now %d",temp,sndsize);
    fprintf (stdout, "%d: %s\n", ID, MsgBuf);
    msgDefineMessage (0, MSG_C_EGLK, MSG_S_INFORMATION, NULL, MsgBuf);

/*    repeat for receive buffer which seems to also be needed     */

    length=sizeof(rcvsize);
    if (getsockopt(tsSocket[ID],SOL_SOCKET,SO_RCVBUF,(char *)&rcvsize,&length) == SOCKET_ERROR) {
        transferReportError();
        sprintf(MsgBuf, "getsockopt() #3 failed:  ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
    }
    temp = rcvsize;

    for (rcvreq = MINRCVBUFSIZE; rcvreq <= RcvBufSize[ID]; rcvreq = rcvreq+MINRCVBUFSIZE) {
        rcvsize = rcvreq*1024;
        if (setsockopt(tsSocket[ID],SOL_SOCKET,SO_RCVBUF,(char *)&rcvsize,sizeof(rcvsize)) == SOCKET_ERROR) {
/*
//            transferReportError();
//            sprintf(MsgBuf, "setsockopt() failed:  ");
//            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
//            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
*/
              break;
        }
    }

    length=sizeof(rcvsize);
    if (getsockopt(tsSocket[ID],SOL_SOCKET,SO_RCVBUF,(char *)&rcvsize,&length) == SOCKET_ERROR) {
        transferReportError();
        sprintf(MsgBuf, "getsockopt() #4 failed:  ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
    }
    
    sprintf(MsgBuf, "TCP socket receive buffer was %d - now %d",temp,rcvsize);
    fprintf (stdout, "%d: %s\n", ID, MsgBuf);
    msgDefineMessage (0, MSG_C_EGLK, MSG_S_INFORMATION, NULL, MsgBuf);

    sprintf(MsgBuf, "TCP socket created OK - now connecting to %s port %d",tsIPStr, ntohs(server.sin_port));
    fprintf (stdout, "%d: %s\n", ID, MsgBuf);
    msgDefineMessage (0, MSG_C_EGLK, MSG_S_INFORMATION, NULL, MsgBuf);

    if (connect(tsSocket[ID],(struct sockaddr*)&server,sizeof(server)) == SOCKET_ERROR) {
        transferReportError();
        sprintf(MsgBuf, "connect() failed:  ");
        fprintf (stderr, "%d: %s\n", ID, MsgBuf);
        msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
        transferMultiClose(ID);
        return ERROR;
    }

    if (NoBlock != 0) {

        if (fcntl(tsSocket[ID], F_SETFL, O_NONBLOCK) == -1) {
            transferReportError();
            sprintf(MsgBuf, "fcntl() failed:  ");
            fprintf (stderr, "%s\n", MsgBuf);
        }
    }

    sprintf(MsgBuf, "Connected to %s port %d",tsIPStr, ntohs(server.sin_port));
    fprintf (stdout, "%d: %s\n", ID, MsgBuf);
    msgDefineMessage (0, MSG_C_EGLK, MSG_S_INFORMATION, NULL, MsgBuf);

    return OK;
}
/*******************************************************************************
* transferClose            final function to close host/client connection
*                        destroys a socket
*/
void transferMultiClose (int ID)
{

    if (ID < 0 || ID >= MAXIDS) return;

    if (tsSocket[ID] != 0) {
#ifdef WIN32
             closesocket (tsSocket[ID]);
#else
             close (tsSocket[ID]);
#endif
             tsSocket[ID] = 0;
    }
    
#ifdef WIN32
    if (WSADATApres[ID] != 0) {
        WSACleanup();
        WSADATApres[ID] = 0;
    }
#endif
}
void transferClose() {transferMultiClose(0);}
/*******************************************************************************
* transferStatus          returns state of socket  = 0 for no connection
*           
*/
int transferMultiStatus (int ID)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    return (tsSocket[ID]);
}
int transferStatus() {return transferMultiStatus(0);}
/*******************************************************************************
* clientInit   initialise 
*              initialise control structures
*              initialise client structure
*/
int clientInit (int ID, char *host)
{
    int rs;

    if (ID < 0 || ID >= MAXIDS) return ERROR;

    transferSequence[ID] = 0;
    if (TferMode[ID] == 6) transferSequence[ID] = 1;

    transferMultiClose (ID);
    strcpy (tsHost[ID], host);
    rs = tsOpenClient (ID, tsHost[ID]);
    return (rs);
}
/*******************************************************************************
* transferTxRestart   transmit the line restart message
*/
int transferMultiTxRestart (int ID)
{
    int rs;

    if (ID < 0 || ID >= MAXIDS) return ERROR;

     (void) clientInit (ID, tsHost[ID]);
     if (tsSocket[ID] == 0)  return ERROR;

     rs = transferMultiTxData (ID, NULL, 0, -1);
     return rs;
}
int transferTxRestart() {return transferMultiTxRestart(0);}
/*******************************************************************************
*/
int transferMultiState (ID)
{

    if (ID < 0 || ID >= MAXIDS) return ERROR;

#ifdef TSOVERLAP
    return pthread_running[ID];
#else
    return 0;
#endif
}
int transferState() {return transferMultiState(0);}
/********************************************************************************
* transferTxData       Transmit the data 
*                  Check window
*/
int transferMultiTxData (int ID, char *data, int stream, int length)
{
        int retval, len;
        HEADER * blockptr;
        MBS_BLKHEADER * mbsblockptr;
        ACK * ackptr;
        unsigned short flags;
        int i;
        unsigned char * ptr;
        unsigned char b1, b2, b3, b4;
        char MsgBuf[256];
        char Ack [sizeof(ACK)];
        char Restart [1024];           /* size for restart message */

    if (ID < 0 || ID >= MAXIDS) return ERROR;

/*    if no connection currently exists - open one */

    if (tsSocket[ID] == 0)  (void)transferMultiTxRestart(ID);
    if (tsSocket[ID] == 0)  return ERROR;

/*    send the data block  */

      if (length < 0) {

/*    length < 0 is used for the RESTART message  */

          blockptr = (HEADER*) Restart;

          if (TferMode[ID] == 4) {                /*  GSI/MBS mode  */
              mbsblockptr = (MBS_BLKHEADER*) Restart;
              mbsblockptr->mbs_endian = 1;
              mbsblockptr->mbs_blocksize = BlockSize[ID];
          } else {
              if (TferMode[ID] == 1) {flags = 0;} else {flags = 2;}
              blockptr->hdr_flags = htons(flags);
              blockptr->hdr_stream = htons((short)stream);
              blockptr->hdr_endian = 1;
              blockptr->hdr_ID = htons((short)ID);
          }
          blockptr->hdr_sequence = 0;
          blockptr->hdr_blocklength = htonl(BlockSize[ID]);
          blockptr->hdr_datalength = -1;
          blockptr->hdr_offset = 0;
          blockptr->hdr_id1 = 0;
          blockptr->hdr_id2 = 0;

          len = sizeof(Restart);
       } else {
           if (TferMode[ID] == 2 || TferMode[ID] == 4) {
              if (stream == 0) return OK;
              blockptr = (HEADER*) data;     /*   TferMode=2 or 4  -  no protocol header   */
           } else {

                  blockptr = (HEADER*) (data - sizeof(HEADER));

/*    32 bytes are available BEFORE the supplied pointer for control information   */
/*    mode = 1 requires ACK for each request; mode = 3 does not  */

                  if (TferMode[ID] == 1) {flags = 0;} else {flags = 2;}
                  blockptr->hdr_flags = htons(flags);

                  blockptr->hdr_stream = htons((short)stream);
                  blockptr->hdr_endian = Endian[ID];
                  blockptr->hdr_ID = htons((short)ID);
                  blockptr->hdr_sequence = htonl(transferSequence[ID]);
                  blockptr->hdr_blocklength = htonl(BlockSize[ID]);
                  blockptr->hdr_datalength = htonl(length);
                  blockptr->hdr_offset = htonl(0);
                  blockptr->hdr_id1 = htonl(HDR_ID1);
                  blockptr->hdr_id2 = htonl(HDR_ID2);

                  transferSequence[ID]++;
           }
           len = BlockSize[ID];
        }

#ifdef TSOVERLAP

      if (length > 0 &&  TferMode[ID] != 1) {

        (void) transferMultiTxWait(ID);      /*  ensure there is no pending request */

/*    a suitable request to be passed to the send thread */

        if (verbose) printf("transferTxData sending\n");

        do_send_thread(ID,(char *)blockptr,len);

        if (!OverlapMode[ID]) transferMultiTxWait(ID);

      } else {

#endif

/*    threads are not in use - or an acknowledgement is required */

        retval = do_send(tsSocket[ID],(char *)blockptr,len);
        if (retval == SOCKET_ERROR) {
            transferReportError();
            sprintf(MsgBuf, "send() failed: ");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            transferMultiClose(ID);
            return  ERROR;
        }

#ifdef TSOVERLAP
      }
#endif

        if (verbose) printf("Sent %d bytes to server\n",len);

        if (TferMode[ID] == 2 || TferMode[ID] == 4) return OK;

        if (TferMode[ID] == 6) return OK;

        if (flags == 2) {return OK;}   /* ACK is suppressed */

/*    wait for acknowledgement  */

        retval = do_recv(tsSocket[ID],Ack,sizeof (Ack),0 );
        if (retval == SOCKET_ERROR) {
            transferReportError();
            sprintf(MsgBuf, "recv() failed: ");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            transferMultiClose(ID);
            return ERROR;
        }
        if (retval == 0) {
            sprintf(MsgBuf, "Server closed connection\n");
            fprintf (stderr, "%d: %s\n", ID, MsgBuf);
            msgDefineMessage (0, MSG_C_EGLK, MSG_S_ERROR, NULL, MsgBuf);
            transferMultiClose(ID);
            return ERROR;
        }
        if (verbose) printf("Received %d bytes from server\n",retval);

/*    process the acknowledgement   */

        ackptr = (ACK*) &Ack[0];
        if (ntohs(ackptr->acq_code) != 0) return ntohs(ackptr->acq_code);

    return OK;
}
int transferTxData (char *data, int stream, int length) {return transferMultiTxData (0, data, stream, length);}
/*******************************************************************************/
