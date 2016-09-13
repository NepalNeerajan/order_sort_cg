#include "DataSource.h"
//#include "Common.h"
//#include "DutClass.h"

#include <bitset>
//#include <fstream>
//#include <iostream>
//#include <stdio.h>
//#include <sstream>
//#include <string>

#include "c_functions.h"

void DataSource::Close(){

  //if reading source data from MIDAS file
  if(GetBSourceOpen() && source==1){
    if(input_file.is_open()){
      input_file.close();
      SetBSourceOpen(false);

      std::cout << " DataSource.cpp: Closing input data file ..." << std::endl;
    }
  }

  else if(GetBSourceOpen() && source==2){
    std::cout << " DataSource.cpp: ... nothing to close when attached online?" << std::endl;
    int i;
    i = dataSpyClose(GetDataSpyId());
    SetBSourceOpen(false);
  }

  //if data Xfer going on... close with transferMultiClose()??
}


void DataSource::Process(){

  //check also if source open?

  if(!GetBEndOfBuffer()){
    
    // Assemble words from string of unsigned char.
    word_0 = (block_data[itr_data] & 0xFF)  | (block_data[itr_data+1] & 0xFF) << 8 | 
      (block_data[itr_data+2] & 0xFF) << 16 | (block_data[itr_data+3] & 0xFF) << 24;
    
    // Assemble words from string of unsigned char.
    word_1 = (block_data[itr_data+4] & 0xFF) | (block_data[itr_data+5] & 0xFF) << 8 | 
      (block_data[itr_data+6] & 0xFF) << 16 | (block_data[itr_data+7] & 0xFF) << 24;
    
    if(b_debug && itr_data<17){
      printf("db     itr%i  word0: %X \n", itr_data, word_0);
      printf("db            word1: %X \n", word_1);
    }


    //We reject all data when one of the words is 0xFFFF
    if( word_0 == 0xFFFFFFFF || word_1 == 0xFFFFFFFF){
      //      std::cerr << "\n  *WARNIGN*  DataSource.cpp: Found 'End of block' before reaching data_length bytes.\n     itr_data, itr_block: " << itr_data << "  "<<itr_block << "  (word0,word1: "<<std::hex<<word_0<<"  "<<word_1<<") *********\n"<<std::dec<<std::endl; 
      SetBEndOfBuffer(true);
      SetBPushData(false);
    }


    //update itr_data
    SetItrData(itr_data + 8);
    SetBPushData(true);

    if(itr_data >= data_length) SetBEndOfBuffer(true);
  }
  else { //need new buffer (or we've reached end of file)
    ReadBuffer(); //this thing returns a bool
    SetBPushData(false);
  }


}


bool DataSource::ReadBuffer(){

  //if reading data from a file
  if(GetBSourceOpen() && source==1){
    if(itr_block < num_blocks){
      input_file.read((char*)&block_header, sizeof(block_header));
      input_file.read((char*)&block_data, sizeof(block_data));

      itr_block++; //could also use set method...
      SetItrData(0);
      SetBEndOfData(false);
      SetBEndOfBuffer(false);

      // Process header.
      unsigned char header_id[8]; // 8 byte. Must be this string 'EBYEDATA'.
      header_id[0] = block_header[0];
      header_id[1] = block_header[1];
      header_id[2] = block_header[2];
      header_id[3] = block_header[3];
      header_id[4] = block_header[4];
      header_id[5] = block_header[5];
      header_id[6] = block_header[6];
      header_id[7] = block_header[7];
      
      unsigned int header_sequence; // 4 byte.
      header_sequence =  
	(block_header[8] & 0xFF) << 24 | (block_header[9]& 0xFF) << 16 |
	(block_header[10]& 0xFF) << 8  | (block_header[11]& 0xFF);
    
      //      unsigned short int header_stream; // 2 byte.
      //      header_stream = (block_header[12] & 0xFF) << 8 | (block_header[13]& 0xFF);
    
      //    unsigned short int header_tape; // 2 byte.
      //    header_tape = (block_header[14] & 0xFF) << 8 | (block_header[15]& 0xFF);
    
      unsigned short int header_MyEndian; // 2 byte. If 256 then correct endianess.
      header_MyEndian = (block_header[16] & 0xFF) << 8 | (block_header[17]& 0xFF);
    
      unsigned short int header_DataEndian; // 2 byte.
      header_DataEndian = (block_header[18] & 0xFF) << 8 | (block_header[19]& 0xFF);
    
      // *** Most important information from header ***
      data_length= (block_header[20] & 0xFF) | (block_header[21]& 0xFF) << 8 |
	(block_header[22] & 0xFF) << 16  | (block_header[23]& 0xFF) << 24 ;


     
      if(b_debug){

	/**** Print header info *************************/
	std::cout << "== DATA ONLINE: BLOCK: " << itr_block << " (itr= "<< itr_data<<"  "<< header_id <<")"<< std::endl;
	std::cout << "   Header sequence: 0x" << std::hex <<header_sequence <<std::dec<< std::endl;
	std::cout << "   Header MyEndian/DataEndian: " << header_MyEndian << "  " << header_DataEndian << std::endl;
	std::cout << "-- Header Data Length: " <<  data_length << " (0x" << std::hex<< data_length <<std::dec <<")"<<std::endl;      
	
      }

      return true;
    }
    else { //if we've read all data in input file
      SetBEndOfData(true);
      return false;
    }
    
  }

  //if reading data online through DataSpy methods 
  else if(GetBSourceOpen() && source==2){

    int i;

    for(;;){
      DataLength = dataSpyRead (id, (char*)&BufferIn[sizeof(HEADER)-sizeof(DATA_HEADER)], BufferSize);  //  read into buffer at offset 8 bytes; real data starts at 8+24 = 32  
      if (verbose>1) printf("   read done with code=%d\n",DataLength);
      
      if (DataLength > 0) {
      
	memcpy (data_header.header_id, (char*)&BufferIn[sizeof(HEADER)-sizeof(DATA_HEADER)], sizeof(DATA_HEADER));
	BufferLength = data_header.header_dataLen;   //  the actual application data less headers  
	if (verbose>1) printf("   buffer length=%d\n",BufferLength);

	//extract from BufferIn array of MIDAS data (header+data block)
	//get block header
	memcpy(block_header, &BufferIn[block_offset], sizeof(block_header));
	//get block data
	memcpy(block_data, &BufferIn[block_offset+HEADER_SIZE], sizeof(block_data));

	SetItrData(0);
	SetBEndOfData(false);
	SetBEndOfBuffer(false);
    
	// *** Most important information from header ***
	data_length= (block_header[20] & 0xFF) | (block_header[21]& 0xFF) << 8 |
	  (block_header[22] & 0xFF) << 16  | (block_header[23]& 0xFF) << 24 ;
	
	
	if(b_debug){
	  
	  //   **** Print header info *************************
	  // Process header.
	  unsigned char header_id[8]; // 8 byte. Must be this string 'EBYEDATA'.
	  header_id[0] = block_header[0];
	  header_id[1] = block_header[1];
	  header_id[2] = block_header[2];
	  header_id[3] = block_header[3];
	  header_id[4] = block_header[4];
	  header_id[5] = block_header[5];
	  header_id[6] = block_header[6];
	  header_id[7] = block_header[7];
	  
	  unsigned int header_sequence; // 4 byte.
	  header_sequence =  
	    (block_header[8] & 0xFF) << 24 | (block_header[9]& 0xFF) << 16 |
	    (block_header[10]& 0xFF) << 8  | (block_header[11]& 0xFF);
	
	
	  unsigned short int header_MyEndian; // 2 byte. If 256 then correct endianess.
	  header_MyEndian = (block_header[16] & 0xFF) << 8 | (block_header[17]& 0xFF);
	  
	  unsigned short int header_DataEndian; // 2 byte.
	  header_DataEndian = (block_header[18] & 0xFF) << 8 | (block_header[19]& 0xFF);
	  
	  std::cout << "== DATA BLOCK: " << itr_block << " (itr= "<< itr_data<<"  "<< header_id <<")"<< std::endl;
	  std::cout << "   Header sequence: 0x" << std::hex <<header_sequence <<std::dec<< std::endl;
	  std::cout << "   Header MyEndian/DataEndian: " << header_MyEndian << "  " << header_DataEndian << std::endl;
	  std::cout << "-- Header Data Length: " <<  data_length << " (0x" << std::hex<< data_length <<std::dec <<")"<<std::endl;      


	}

	return true; //success reading next buffer

      } else {
	if (DelayTicks) {

	  time_request.tv_sec = DelayTicks/1000;
	  time_request.tv_nsec = (DelayTicks - (time_request.tv_sec * 1000)) * 1000000;
	  i = nanosleep(&time_request, NULL);
	}
      }

      //this will continue looping until a new buffer is read by Spy Program
  }

    
  }
   

  //if didn't succeed in reading buffer
  return false;
  
}








void DataSource::InitDataSource(int opt, int my_id, std::string &file_name, bool b_Xfer){ 

  SetBSourceOpen(false);
  SetBSendData(false);

  if(opt==1){
    if(b_debug) std::cout << "   + DataSource: Initialize data source from file: " << file_name << std::endl;

    input_file.open(file_name.c_str(), std::ios::in|std::ios::binary);
      
    if (input_file.is_open()){
    
      // Calculate the size of the file.
      input_file.seekg (0, input_file.end);
      int size_end = input_file.tellg();
      input_file.seekg (0, input_file.beg);
      int size_beg = input_file.tellg();

      if(SetFileSize( size_end - size_beg)){ //also sets num_blocks
	SetSource(1);
	SetItrBlock(0);
	SetItrData(0);

	SetBSourceOpen(true);

	std::cout << " DataSource::InitDataSource(): Opened input file " << file_name << ", with size " << size_end - size_beg  << "  **************"  << std::endl << std::endl;  

	if(! ReadBuffer() )  std::cout << " DataSource.cpp:: Something fishy going on! Should have been able to read first buffer from Input file: " << file_name << " !   **************"  << std::endl << std::endl;
	
      }
      else{
	std::cout << " DataSource.cpp::InitDataSource(): Input file " << file_name << " is empty or too small (size " << size_end - size_beg  << ")  **************"  << std::endl << std::endl;
      }
      
    }
    else{  
      std::cout << " DataSource.cpp::InitDataSource(): Error opening input file " << file_name << " *****************"  << std::endl << std::endl;
    }
  
  }

  else if(opt==2){

    std::cout << "  DataSource::InitDataSource(): attaching online to shared memory area with ID= " << my_id << "." << std::endl;

    int i;
    SetDataSpyId(my_id);
    i= dataSpyOpen(GetDataSpyId());

    SetBSourceOpen(true);
    
    SetSource(2);
    SetItrBlock(0);
    SetItrData(0);
    
    std::cout << "+  DataSource: attached online  **************"  << std::endl << std::endl;  
    
    if(! ReadBuffer() )  std::cout << " DataSource::InitDataSource(): Something fishy going on! Should have been able to read first buffer online !!   **************"  << std::endl << std::endl;
    
  }

  else{
    std::cout << "!!!  DataSource::InitDataSource(): invalid source_option= " << opt <<". DataSource is not initialized **************"  << std::endl << std::endl; 
  }


  if(b_Xfer){ //also forward data to another DataSink

    if( TS_Server == NULL){
      std::cout <<  "  DataSource::InitDataSource():  TS_Server not defined; will not forward data" << std::endl;
      return;
    }
    
    (void) transferMultiBlockSize(ID, BufferSize);
    (void) transferMultiMode(ID, Mode);
    (void) transferMultiPort(ID, Port);
    (void) transferMultiInit(ID, TS_Server);
    
    std::cout << "   DataSource::InitDataSource(): initialized data trasfer protocols  to Port= " << Port << ", TS_Server= "<<TS_Server <<" ****"  << std::endl;  
    
    SetBSendData(true);

  }
}

















void DataSource::TransferBuffer(double ts){

  //  printf("......transfer buffer....\n");

  if(GetBuffOffset() > MAX_LENGTH_DATA | ts > (ts_0 + DELTA_TS )){
    int s;

    //buffer lenght... calculate useful data... and add a few 0xfffffff if required
    BufferLength= GetBuffOffset();

    int empty= MAIN_SIZE - BufferLength;

    // add 0xFFFFFFFFFFFFFFFFFFFFFFFF at end
    for(int i=0;(i<24 && i< empty);i++){
	BufferOut[BufferLength]= 0xFF;
	BufferLength++;
      }

    s = transferMultiTxData (ID, &BufferOut[sizeof(HEADER)], stream, /*my_length*/ BufferLength);   /*   write data from offset 32 bytes  */

    //  if (b_debug) printf("Transfer: code %u, at count %u, ts %lu (0x%lX). %u  bytes of data.\n", s, count, prev_ts, prev_ts, buff_offset);
    if (b_debug) printf("Transfer: code %u, ts %f (0x%f). %u  bytes of data.\n", s, ts, ts, buff_offset);
    
    ts_0= ts;
    SetBuffOffset(HEADER_SIZE+8); //Initnal offset to take into account header....
  }

  return;

}




void DataSource::SetSource(int my_source){
  source = my_source;
}

bool DataSource::SetFileSize(int size){

  file_size= size;
  num_blocks=  file_size / BLOCK_SIZE;
  
  if( (file_size%BLOCK_SIZE) != 0){
    std::cout << " *WARNIGN*  DataSource.cpp:: File size is not an integer number of BLOCK_SIZE! file, block, Nblock: " << file_size << "  " << BLOCK_SIZE << "  "<< num_blocks << std::endl << std::endl;
  }
  
  if(b_debug){
    std::cout << "db    DataSource.cpp:: Input file has size, #blocks: " << file_size << "  " << num_blocks << std::endl << std::endl;
  }

  if (file_size>0 && num_blocks>0) return true;
  else return false;
  
}

void DataSource::SetBSourceOpen(bool flag){
  b_source_open= flag;
}

void DataSource::SetBSendData(bool flag){
  b_send_data= flag;
}

void DataSource::SetItrBlock(int itr){
  itr_block= itr;
}

void DataSource::SetItrData(int itr){
  itr_data= itr;
}

void DataSource::SetBDebug(bool flag){
  b_debug= flag;
  if(b_debug) std::cout <<"db    DataSource: set b_debug = true"<<std::endl;
}

void DataSource::SetBEndOfData(bool flag){
  b_end_of_data= flag;
}

void DataSource::SetBEndOfBuffer(bool flag){
  b_end_of_buffer= flag;
}

void DataSource::SetBPushData(bool flag){
  b_push_data= flag;
}


void DataSource::SetBuffOffset(int value){
  buff_offset= value;
}

void DataSource::SetDataSpyId(int value){
  id= value;
}


bool DataSource::GetBSourceOpen(){ return b_source_open; }

bool DataSource::GetBSendData(){ return b_send_data; }

bool DataSource::GetBEndOfData(){ return b_end_of_data; }

bool DataSource::GetBEndOfBuffer(){ return b_end_of_buffer; }

bool DataSource::GetBPushData(){ return b_push_data; }

int DataSource::GetItrBlock(){ return itr_block; }

int DataSource::GetItrData(){ return itr_data; }


int DataSource::GetBuffOffset(){ return buff_offset; }

unsigned int DataSource::GetWord0(){ return word_0; }

unsigned int DataSource::GetWord1(){ return word_1; }


int DataSource::GetDataSpyId(){ return id; }

DataSource::DataSource(){

  b_debug= false;

  b_source_open = false;

  source=0; //0= not initialized
  word_0=0; 
  word_1=0;
  data_length=0;
  file_size=0;
  num_blocks=0;
  //input_file= ??? null ???

  b_end_of_data= false;

  itr_block=0;
  itr_data=0;

  //for data transfer
  b_send_data = false;
  ts_0= 0;
  prev_ts= 0;
  buff_offset= HEADER_SIZE;

  //for data online
  std::cout << "I'm here"<<std::endl;
  TS_Server= (char*) "10.32.6.54";

  std::cout << " and here..."<<std::endl;
  Port=10310;
  ID=2;
  id=0;
  Mode=3;
  stream = 1;
  verbose=0;
  BufferSize=64*1024;
  DelayTicks=10;

  block_offset= 8;

}
