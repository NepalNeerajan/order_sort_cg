#ifndef _DataSource_H //what is going on here?
#define _DataSource_H

#include <fstream>
#include <iostream>
//#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <string>

#include <ctype.h>
#include <stdbool.h>

#include <time.h>





class DataSource{
 private:
  bool b_debug;

  bool b_source_open;
  bool b_send_data;
  bool b_end_of_buffer;
  bool b_end_of_data;
  bool b_push_data;

  int source;
  unsigned int word_0; 
  unsigned int word_1;
  int data_length;

  int buff_offset;
  double ts_0;
  double prev_ts;

  //static? all instances of class have same value, can be defined here
  static const int HEADER_SIZE=24; // Size of header in bytes
  static const int BLOCK_SIZE= 0x10000; //Max block size is 64kb. Amount of useful data given in header
  static const int MAIN_SIZE=  BLOCK_SIZE - HEADER_SIZE; // works?
  static const int MAX_LENGTH_DATA = 4000;
  //  static const int MAX_LENGTH_DATA = MAIN_SIZE - 3*352; // 352 current size of thingy being transmitted... some safety....
  static const int DELTA_TS = 40000000;

  char block_header[HEADER_SIZE]; 


  char block_data[MAIN_SIZE]; //AIDA data here, for either type of data source

  //new for data from online source

  typedef struct s_data_header {
    char header_id[8];
    int header_sequence;
    short  header_stream;
    short  header_tape;
    short  header_MyEndian;
    short  header_DataEndian;
    int header_dataLen;
  } DATA_HEADER;


  unsigned char BufferIn [64+(64*1024)];    /*  a 64Kbyte data buffer + a bit extra to handle block headers  */

  DATA_HEADER data_header;
  int BufferLength;
  int DataLength;
  
  //DataRelayFilter -n 10.32.6.54 -p 10307 -I 2
  char *TS_Server; //= NULL;
  int Port; //=10305;
  int ID; //=0;
  int id; // id for connection to shared memory area through DataSpy library routines
  int Mode; //=3;
  unsigned int stream; // = 1;
  int verbose; //=0;
  //int s;
  //int i;
  int BufferSize; //=64*1024;

  int block_offset; //= 8; //offset of ther headers Vic uses in DataSpy libraries, to be calculated in a smarter way??


  int DelayTicks; //=10;
  
  struct timespec time_request;  
  
  
  //to read data from file
  std::ifstream input_file;
  int file_size;
  int num_blocks;
  int itr_block;
  int itr_data;



    
 public:


  //public because it is accessed by Analysis::WriteOutBuffer(); could implement a method to make it private...
  char BufferOut [64+(64*1024)];    /*  a 64Kbyte data buffer + a bit extra to handle block headers  */


  DataSource();
  ~DataSource(){};

  void InitDataSource(int opt, int my_id, std::string &file_name, bool b_Xfer);

  void Process();
  bool ReadBuffer();
  void Close();

  void TransferBuffer(double ts);


  bool SetFileSize(int size);
  void SetBSourceOpen(bool flag);
  void SetSource(int my_source);
  void SetItrBlock(int itr);
  void SetItrData(int itr);
  void SetBDebug(bool flag);
  void SetBEndOfData(bool flag);
  void SetBEndOfBuffer(bool flag);
  void SetBPushData(bool flag);
  void SetBSendData( bool value);
  void SetBuffOffset(int value);

  void SetDataSpyId(int value);

  bool GetBSourceOpen();
  bool GetBEndOfBuffer();
  bool GetBEndOfData();
  bool GetBPushData();
  int GetItrBlock();
  int GetItrData();
  unsigned int GetWord0();
  unsigned int GetWord1();
  bool GetBSendData();
  int GetBuffOffset();

  int GetDataSpyId();

};


#endif
