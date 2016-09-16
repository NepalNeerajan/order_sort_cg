#ifndef INCtransferh
#define INCtransferh

#define MAX_STREAM 4
#define MAXPORTS 8

typedef struct block_header {              /* format of data block  */
        unsigned short hdr_flags;          /* see below */
        unsigned short hdr_stream;         /* =0 for forced ack request or 1=>MAX_STREAM  */
        unsigned short hdr_endian;         /*   data byte order */
        unsigned short hdr_ID;
        unsigned int hdr_sequence;        /* for this stream */
        unsigned int hdr_blocklength;     /*  total length of this block including the header */
        unsigned int hdr_datalength;      /*  length of user data in the block  */
        unsigned int hdr_offset;          /*  very large blocks may be fragmented  */
        unsigned int hdr_id1;             /*  for spy to locate header  =0x19062002 */
        unsigned int hdr_id2;             /*  for spy to locate header  =0x09592400 */
} HEADER;

#define HDR_ID1 0x19062002
#define HDR_ID2 0x09592400

typedef struct mbs_block_header {
        unsigned int mbs_endian;
        unsigned int mbs_blocksize;
} MBS_BLKHEADER;


typedef struct ack {
        unsigned short acq_flags;          /* bit 0 = 1 for ack  */
        unsigned short acq_code;           /* =0 for good ack otherwise error reason */
        unsigned short acq_ts_state;       /* tape server state - see below */
        unsigned short acq_stream;         /* =0 for forced ack ack or 1=>MAX_STREAM  */
        unsigned short acq_endian;         /* =1 transmitted in host byte order */
        unsigned short acq_ID;             /* value of ID variable  0 => */
        unsigned int acq_sequence;        /* for stream != 0 is sequence being acked */
        unsigned short acq_stream_state1;           /* see below  */
        unsigned short acq_stream_state2;           /*   */
        unsigned short acq_stream_state3;           /*   */
        unsigned short acq_stream_state4;           /*   */
        unsigned short acq_stream_window1;          /*   */
        unsigned short acq_stream_window2;          /*   */
        unsigned short acq_stream_window3;          /*   */
        unsigned short acq_stream_window4;          /*   */
} ACK;

        
int transferTxData (char *, int, int);
int transferTxRestart ();
int transferBlockSize(int);
int transferMode(int);
int transferBlockMode(int);
int transferEndian(int);
int transferPort(int);
int transferInit (char *);
void transferClose ();
int transferStatus ();
int transferSetVerbose(int);
int transferSetUser(int);
int transferNice(int);
int transferUseOverlap(int);
int transferTxWait();
int transferState();
int transferSndBufSize(int);
int transferRcvBufSize(int);

/*     All these have the thread ID as the first argument  */
int transferMultiTxData (int, char *, int, int);
int transferMultiTxRestart (int);
int transferMultiBlockSize(int, int);
int transferMultiMode(int, int);
int transferMultiBlockMode(int, int);
int transferMultiEndian(int, int);
int transferMultiPort(int, int);
int transferMultiInit (int, char *);
void transferMultiClose (int);
int transferMultiStatus (int);
int transferMultiNice(int, int);
int transferMultiUseOverlap(int, int);
int transferMultiTxWait(int);
int transferMultiState(int);
int transferMultiSndBufSize(int, int);
int transferMultiRcvBufSize(int, int);

#ifndef OK
#define OK 0
#endif

#ifndef ERROR
#define ERROR -1
#endif


#endif
