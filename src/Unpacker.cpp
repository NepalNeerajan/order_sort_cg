#include "Unpacker.h"
//#include "Common.h"
//#include "DutClass.h"


#include <bitset>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <string>

void Unpacker::Close(){


  if(GetBRootTree()){
    out_root_tree->Write(0,TObject::kOverwrite);
    std::cout << "\n *** writting Unpacker TTree .... "<< std::endl;
    out_root_tree->Print();
  }


  if(GetBHistograms())  WriteHistograms();
}

//the heart of the matter
void Unpacker::Process(DataSource & my_src_data){

  ResetData(); //set values of unp_data to defaults (zero)

  unsigned char my_char;
  unsigned int my_int;
  unsigned long my_long;


  unsigned int word_0= my_src_data.GetWord0();
  unsigned int word_1= my_src_data.GetWord1();


  my_char= (word_0 >> 30) & 0x3; 
  SetDataType(my_char);

  //ADC data
  if(GetDataType() == 3){
    my_int = word_0 & 0xFFFF; //bits 0:15
    SetAdcData(my_int);

    //channel identifier
    my_int=  (word_0 >> 16) & 0x0FFF; //bits 16:27
    my_char = (my_int >> 6) & 0x003F;
    SetFee64Id(my_char);
    my_char = my_int & 0x003F;
    SetChId(my_char);

    my_char = (word_0 >> 28) & 0x0001; //bit 28
    SetAdcRange(my_char);

    my_long = word_1 & 0x0FFFFFFF;  //bits 0:27
    SetTmStpLsb(my_long);

    if(IsValidFee64Id() && b_mod_enabled[GetFee64Id()]) SetBPushData(true);

    SetBFillTree(true);
  }  
  //Information data
  else if(GetDataType() == 2){

    my_long= word_0 & 0x000FFFFF; //bits 0:19
    SetInfoField(my_long);

    my_char = (word_0 >> 24) & 0x003F; //bits 24:29
    SetFee64Id(my_char);
    my_char = (word_0 >> 20) & 0x000F; //bits 20:23
    SetInfoCode(my_char);

    my_long = word_1 & 0x0FFFFFFF;  //bits 0:27
    SetTmStpLsb(my_long);

    // 2== PAUSE
    if(GetInfoCode()==2){
      tm_stp_msb= GetInfoField();
      //if(!GetBSyncStatus()) SetBSyncStatus(true); //data is synchronized!
    }

    // 3== RESUME
    if(GetInfoCode()==3){
      tm_stp_msb= GetInfoField();
      if(!GetBSyncStatus()) SetBSyncStatus(true); //data is synchronized!
    }

    // 4==SYNC100 pulse
    if(GetInfoCode()==4){
      tm_stp_msb= GetInfoField();
      if(!GetBSyncStatus()) SetBSyncStatus(true); //data is synchronized!

      //  SetBPushData(true);
    }

    // 6==DISC data
    else if(GetInfoCode()==6){
      my_char= GetInfoField() & 0xFFFF;  //channel number is encoded in info-field
      SetChId(my_char);

      SetBPushData(true);
    }

    // 8==Correlation Scaler
    else if(GetInfoCode()==8){
      my_char= ( GetInfoField() & 0x000F0000 ) >> 16; //index of corr scaler
      my_long = ( GetInfoField() & 0x0000FFFF ) ; //bits with timestamp

      //assumes numbering Fee64 modules starts from 1 (and now goes 1:16)
      if(IsValidFee64Id()) {
	if(my_char==0) corr_scaler_data0[GetFee64Id()]= my_long;
	else if(my_char==1) corr_scaler_data1[GetFee64Id()]= my_long;
	else if(my_char==2){
	  unsigned long scaler= my_long << 32 | corr_scaler_data1[GetFee64Id()] << 16 | corr_scaler_data0[GetFee64Id()];

	  if(b_debug){
	    if(GetFee64Id()==13 ||GetFee64Id()==14 ||GetFee64Id()==16 ){
	      
	      std::cout << " CORR SCALER: "<< 1.*scaler <<std::endl;
	    }
	  }

	  //updates state of b_corr_status, if we have already received SYN100 pulses
	  //also updates value of corr_scaler_offset
	  SetCorrScaler(scaler);
	  //	  SetBCorrStatus(true);
	  SetBPushData(true);
	}
      }
    }

    SetBFillTree(true); //Fill TTree for all Info Data Codes
    // SetPushData(true); !!only push data for selected info types: now DISC and CORR SCALAR
  }
  //Sample trace: Sample Lenght
  else if(GetDataType() == 1){

    my_int = word_0 & 0xFFFF; //bits 0:15
    SetSampleLength(my_int);

    //channel identifier
    my_int=  (word_0 >> 16) & 0x0FFF; //bits 16:27
    my_char = (my_int >> 6) & 0x003F;
    SetFee64Id(my_char);
    my_char = my_int & 0x003F;
    SetChId(my_char);

    my_long = word_1 & 0x0FFFFFFF;  //bits 0:27
    SetTmStpLsb(my_long);

    //these are default values
    // SetBFillTree(false);
    //    SetBPushData(false);
  }

  //Sample trace: Waveform data
  else if(GetDataType() == 0){
    // 4 x 14 bits Waveform samples
    // NOT IMPLEMENTED PROPERLY, only first value saved now (need more variables in unpack_data_struct)

    my_int=  (word_0 >> 16) & 0x00003FFF;    //sample n
    SetSampleLength(my_int);

    //these are default values
    //    SetBFillTree(false);
    //    SetBPushData(false); //not being processed for now.
  }
  else{
    //output error message!
    std::cerr << " **WARNING** Unpacker.cpp: DATA TYPE NOT RECOGNIZED! word 0: " << word_0 << ", data_type: "<<GetDataType()<<std::endl;
    SetBPushData(false);
  }


  //do following (settmstp, flags, etc..) only if BPushData
  SetTmStp(); //reconstruct full time stamp
  SetFlags();

  if(!GetSyncFlag()) SetBPushData(false); //do not forward data out of SYNC100 (need to improve test of SYNC status)

  // If an interesting data type, Fill TTree with this entry 
  if(GetBRootTree() && GetBFillTree()) out_root_tree->Fill();
    
  if(GetBHistograms()) FillHistograms();
 


  if(b_debug){
    if(my_src_data.GetItrData()<65){ //for first few entries in block output to screen
      printf("db      Unp:: DataType= %i, NNAIDA%i, tm-stp(lsb)= %lu, ",GetDataType(),GetFee64Id(),GetTmStpLsb());
      if(GetDataType()==3) printf("adc= %u\n",GetAdcData());
      else if(GetDataType()==2) printf("code= %i\n",GetInfoCode());
    }
    else if(GetFee64Id() == 3 && GetDataType() == 3 && GetChId()<2){
      printf("db      Unp:: NNAIDA3: ch= %i, adc= %u\n",GetChId(),GetAdcData());
    }
  }

}



void Unpacker::FillHistograms(){

  if(GetDataType()==3){ //ADC
    if(GetAdcRange()==0){
      hFEE64_ADClow->Fill(GetFee64Id());
      hCh_FEE64_ADClow->Fill(GetFee64Id(),GetChId());

    }
    else if(GetAdcRange()==1){
      hFEE64_ADChigh->Fill(GetFee64Id());
      hCh_FEE64_ADChigh->Fill(GetFee64Id(),GetChId());
    }
  }

  else if(GetDataType()==2){ //InfoCode

    hFEE64_Info->Fill(GetFee64Id());

    hInfoCode_FEE64->Fill(GetFee64Id(),GetInfoCode());

    if(GetInfoCode()==2) hFEE64_PAUSE->Fill(GetFee64Id());
    else if(GetInfoCode()==3) hFEE64_RESUME->Fill(GetFee64Id());
    else if(GetInfoCode()==4) hFEE64_SYNC100->Fill(GetFee64Id());

    else if(GetInfoCode()==6) hCh_FEE64_DISC->Fill(GetFee64Id(), GetChId() );
  }

  else if(GetDataType()==1) hFEE64_Waveform->Fill(GetFee64Id());

  //    if(
  //      hCh_FEE64_ADClow->Fill(GetFee64Id(),GetChId());
  //  }
  //if(IsValidFee64Id() && GetFee64Id()<17){
  //  hUnpMod[GetFee64Id()]->Fill(GetDataType());
  //}
}

bool Unpacker::IsValidFee64Id(){
  if(GetFee64Id() >0 && GetFee64Id() < common::N_FEE64) return true; //common:n_fee64?
  else return false;
}


bool Unpacker::IsValidFee64Id(int mod_id){
  if(mod_id >0 && mod_id < common::N_FEE64) return true; //common:n_fee64?
  else return false;
}


void Unpacker::InitUnpacker(int opt, char *file_name){

  // if first of 'opt' is 1, enable histogramming
  if( (opt & 0x01) == 1){

    std::cout << "     Unpacker::InitUnpacker(): defining histograms"<<std::endl;

    SetBHistograms(true);

    char hname[256];
    char htitle[256];

    cUnp1= new TCanvas("cUnp1","cUnpacker 1",20,20,900,900); cUnp1->Divide(4,3);


    sprintf(hname,"hFEE64_ADClow");
    sprintf(htitle,"FEE64 !ADClow;module ID");
    hFEE64_ADClow = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);

    sprintf(hname,"hFEE64_ADChigh");
    sprintf(htitle,"FEE64 !ADChigh;module ID");
    hFEE64_ADChigh = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);

    sprintf(hname,"hFEE64_Waveform");
    sprintf(htitle,"FEE64 !Waveform;module ID");
    hFEE64_Waveform = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);

    sprintf(hname,"hFEE64_Info");
    sprintf(htitle,"FEE64 !Info;module ID");
    hFEE64_Info = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);

    cUnp1->cd(1); hFEE64_ADClow->Draw();
    cUnp1->cd(2); hFEE64_ADChigh->Draw();
    cUnp1->cd(3); hFEE64_Waveform->Draw();
    cUnp1->cd(4); hFEE64_Info->Draw();


    sprintf(hname,"hInfoCode_FEE64");
    sprintf(htitle,"Info Code vs FEE64;module ID;info code");
    hInfoCode_FEE64 = new TH2I(hname, htitle, common::N_FEE64,0,common::N_FEE64,16,0,16);
    cUnp1->cd(5); hInfoCode_FEE64->Draw("col");

    sprintf(hname,"hFEE64_PAUSE");
    sprintf(htitle,"FEE64 !PAUSE;module ID");
    hFEE64_PAUSE = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);
    cUnp1->cd(6); hFEE64_PAUSE->Draw("");

    sprintf(hname,"hFEE64_RESUME");
    sprintf(htitle,"FEE64 !RESUME;module ID");
    hFEE64_RESUME = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);
    //  hFEE64_RESUME->SetLineColor(2);
    cUnp1->cd(7); hFEE64_RESUME->Draw("");

    sprintf(hname,"hFEE64_SYNC100");
    sprintf(htitle,"FEE64 !SYNC100;module ID");
    hFEE64_SYNC100 = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);
    cUnp1->cd(8); hFEE64_SYNC100->Draw("");


    //  sprintf(hname,"hFEE64_Waveform");
    //  sprintf(htitle,"FEE64 !Waveform;module ID");
    //  hFEE64_Waveform = new TH1I(hname, htitle, common::N_FEE64,0,common::N_FEE64);
    //  cUnp1->cd(8); hFEE64_Waveform->Draw("");



    sprintf(hname,"hCh_FEE64_ADClow");
    sprintf(htitle,"channel vs FEE64 !ADClow;module ID;ch ID");
    hCh_FEE64_ADClow = new TH2I(hname, htitle, common::N_FEE64,0,common::N_FEE64,64,0,64);
    cUnp1->cd(9); hCh_FEE64_ADClow->Draw("col");

    sprintf(hname,"hCh_FEE64_ADChigh");
    sprintf(htitle,"channel vs FEE64 !ADChigh;module ID;ch ID");
    hCh_FEE64_ADChigh = new TH2I(hname, htitle, common::N_FEE64,0,common::N_FEE64,64,0,64);
    cUnp1->cd(10); hCh_FEE64_ADChigh->Draw("col");

    sprintf(hname,"hCh_FEE64_DISC");
    sprintf(htitle,"channel vs FEE64 !DISC;module ID;ch ID");
    hCh_FEE64_DISC = new TH2I(hname, htitle, common::N_FEE64,0,common::N_FEE64,64,0,64);
    cUnp1->cd(11); hCh_FEE64_DISC->Draw("col");


    if(b_debug) std::cout << "db    Unpacker.cpp: Initialized histograms and canvas for this step"<<std::endl;
  }

  //if second bit of 'opt' is 1, enable TTree for output
  //  if( (opt & 0x02) == 1){
  if( ( (opt & 0x02) >> 1)  == 1){

    std::cout << " ***     Unpacker::InitUnpacker(): initializing TTree" << std::endl;

    //Initialize TTree
    out_root_tree = new TTree("AIDA_unpack","AIDA_unpack");
    // 16/6: remove corr_flag
    out_root_tree->Branch("entry_unpack",&unp_data,"tm_stp/l:corr_scaler/l:tm_stp_lsb/l:info_field/l:adc_data/i:sample_length/i:data_type/b:fee64_id/b:ch_id/b:adc_range/b:info_code/b:sync_flag/O");

    out_root_tree->SetAutoSave(-500000000);
    out_root_tree->SetAutoFlush(-50000000);

    std::cout << "  !!!!! Unpacker:: TTrree->GetAutoSave(): "<< out_root_tree->GetAutoSave() <<" !!! "<<std::endl;

    SetBRootTree(true);


  }

  LoadParameters(file_name);

  ResetData(); //sets values of unp_data structure to default values

}


void Unpacker::UpdateHistograms(){

  if(GetBHistograms()){

    for(int i=0; i<12; i++){
      cUnp1->cd(i+1)->Modified(); 
    }
    cUnp1->Update();
  }
}

void Unpacker::ResetHistograms(){

  if(GetBHistograms()){
    hFEE64_ADClow->Reset();
    hFEE64_ADChigh->Reset();  
    hFEE64_Waveform->Reset();
    hFEE64_Info->Reset();

    hInfoCode_FEE64->Reset();

    hFEE64_PAUSE->Reset();
    hFEE64_RESUME->Reset();
    hFEE64_SYNC100->Reset();

    hCh_FEE64_ADClow->Reset();
    hCh_FEE64_ADChigh->Reset();
    hCh_FEE64_DISC->Reset();
  }

}

void Unpacker::ResetData(){

  b_push_data= false;
  b_fill_tree= false;

  unp_data.tm_stp= 0;
  unp_data.corr_scaler= 0;
  unp_data.tm_stp_lsb= 0;
  unp_data.info_field= 0;

  unp_data.adc_data= 0;
  unp_data.sample_length= 0;
  unp_data.data_type= 0;
  unp_data.fee64_id= 0;
  unp_data.ch_id= 0;
  unp_data.adc_range= 0;
  unp_data.info_code= 0;

  unp_data.sync_flag= false;
  //  unp_data.corr_flag= false; 

}




void Unpacker::WriteHistograms(){



  //write histograms to TFile
  hFEE64_ADClow->Write();
  hFEE64_ADChigh->Write();  
  hFEE64_Waveform->Write();
  hFEE64_Info->Write();

  hInfoCode_FEE64->Write();

  hFEE64_PAUSE->Write();
  hFEE64_RESUME->Write();
  hFEE64_SYNC100->Write();

  hCh_FEE64_ADClow->Write();
  hCh_FEE64_ADChigh->Write();
  hCh_FEE64_DISC->Write();

  //and also TCanvas
  cUnp1->Write();

  std::cout << "      Unpacker::WriteHistograms(): done writing to file"<<std::endl;

}







void Unpacker::LoadParameters(char * file_name){

  std::ifstream param_file;

  param_file.open(file_name,std::ios::in); //need to specify as text?


  const int MAX_LINES= 40000; //4176 number of parameters used so far...
  int line_count=0;
  int par_count=0;

  char line[1024]; // string or char array?
  std::string my_line;
  char ch_first;
  std::string par_name;
  int module, channel;
  double data;
  double x, y, z;
  //  bool b_data;

  if(param_file.is_open()){

    for(;;){

      if(std::getline(param_file, my_line)){

	if(b_debug) std::cout << " * LoadParam() - line: " << my_line << std::endl;

	//skip empty lines and comments
	if(!my_line.empty() && '\n' != my_line[0] && '#' != my_line[0]){
	
	  std::istringstream iss(my_line);  
	  y = -1; //?? o a valid number
	  x = -1;
	  z= -1;
	  // par_name[0]= '\0';

	  iss >> par_name >> x >> y >> z;


	  if(b_debug) std::cout << " * LoadParameter() - values read: " << par_name << " "<< x << y << z << std::endl;
	  

	  if(par_name=="b_mod_enabled"){
	    module= x; data= y;
	    if(IsValidFee64Id(module)){
	      par_count++;
	      b_mod_enabled[module]= int(data);
	    }
	  }
	  else{
	    if(b_debug) std::cout << " Unpacker.cpp:: parameter name not recognized: " << par_name << "." <<std::endl;
	  }
	
	}
	

      }
      else{
	std::cout << " Unpacker.cpp:: LoadParameters() reached end of parameter file. Loaded new value for "<< par_count << " parameters." << std::endl;
	break;
      }
      //check forever loop not running amok
      line_count++;
      if(line_count>MAX_LINES){

	std::cout << " Unpacker.cpp:: count of lines read by LoadParameters() exceeds maximum allowed value (" << MAX_LINES << "). Parameters loaded= " << par_count<<std::endl;
	std::cout << "                  Break out of loop and continue... *****************"  << std::endl << std::endl;
	break;
      }
    }

  }
  else{
    std::cout << " Unpacker.cpp:: Error opening parameter file: " << file_name << " *****************"  << std::endl << std::endl;
  }
  return;

}










void Unpacker::SetBSyncStatus(bool flag){
  b_sync_status= flag;
}


//void Unpacker::SetBCorrStatus(bool flag){
//  b_corr_status= flag;
//}


void Unpacker::SetBDebug(bool flag){
  b_debug= flag;
}

void Unpacker::SetBHistograms(bool flag){
  b_histograms= flag;
}

void Unpacker::SetBPushData(bool flag){
  b_push_data= flag;
}

void Unpacker::SetBFillTree(bool flag){
  b_fill_tree= flag;
}


void Unpacker::SetBRootTree(bool flag){
  if(flag){
    if(out_root_tree){
      b_root_tree= true;
    }
    else {
      std::cout << " ** WARNING ** Unpacker.cpp: attempted to set b_root_tree as valid when TTree is not initialized! *** " << std::endl;
      b_root_tree= false;
    }
  }
  else b_root_tree= false;
}


void Unpacker::SetTmStp(){
  if(GetBSyncStatus()){
    unp_data.tm_stp= ( tm_stp_msb << 28 ) | unp_data.tm_stp_lsb ; //or do I use a parameter in function call for tm_stp_lsb?
   }
  else unp_data.tm_stp= 0;
}

void Unpacker::SetFlags(){
  unp_data.sync_flag= GetBSyncStatus(); // b_sync_status;
  //  unp_data.corr_flag= b_corr_status;
}

void Unpacker::SetCorrScaler(unsigned long value){
  unp_data.corr_scaler= value;
  //  unp_data.corr_flag= true; (we'll have at some point a proper flag, for missing corr pulsers and such things)
}


void Unpacker::SetTmStpLsb(unsigned long value){
  unp_data.tm_stp_lsb= value;
}

void Unpacker::SetInfoField(unsigned long value){
  unp_data.info_field= value;
}

void Unpacker::SetAdcData(unsigned int value){
  unp_data.adc_data= value;
}

void Unpacker::SetSampleLength(unsigned int value){
  unp_data.sample_length= value;
}


//validity test done here or with specific functions?
void Unpacker::SetDataType(unsigned char value){
  //if(value>=0 && value<4) unp_data.data_type= value;
  //else unp_data.data_type= 99;
  unp_data.data_type= value;
}

void Unpacker::SetFee64Id(unsigned char value){
  //if(value>=0 && value<17) unp_data.fee64_id= value;
  //else{
  //  if(b_debug) std::cout << "db       Unpacker.cpp: Invalid FEE64 momdule id: " << value << std::endl;
  //  unp_data.fee64_id= 0;
  // }
  unp_data.fee64_id= value;
}

void Unpacker::SetChId(unsigned char value){
  unp_data.ch_id= value;
}

void Unpacker::SetAdcRange(unsigned char value){
  unp_data.adc_range= value;
}

void Unpacker::SetInfoCode(unsigned char value){
  unp_data.info_code= value;
}


bool Unpacker::GetBSyncStatus(){ return b_sync_status; }
//bool Unpacker::GetBCorrStatus(){ return b_corr_status; }
bool Unpacker::GetBDebug(){ return b_debug; }

bool Unpacker::GetBPushData(){ return b_push_data; }
bool Unpacker::GetBFillTree(){ return b_fill_tree; }

bool Unpacker::GetBHistograms(){ return b_histograms; }
bool Unpacker::GetBRootTree(){ return b_root_tree; }

unsigned long  Unpacker::GetTmStp(){ return unp_data.tm_stp; }
unsigned long  Unpacker::GetCorrScaler(){ return unp_data.corr_scaler; }
unsigned long  Unpacker::GetTmStpLsb(){ return unp_data.tm_stp_lsb; }
unsigned long  Unpacker::GetInfoField(){ return unp_data.info_field; }
unsigned int  Unpacker::GetAdcData(){ return unp_data.adc_data; }
unsigned int  Unpacker::GetSampleLength(){ return unp_data.sample_length; }
unsigned char  Unpacker::GetDataType(){ return unp_data.data_type; }
unsigned char  Unpacker::GetFee64Id(){ return unp_data.fee64_id; }
unsigned char  Unpacker::GetChId(){ return unp_data.ch_id; }
unsigned char  Unpacker::GetAdcRange(){ return unp_data.adc_range; }
unsigned char  Unpacker::GetInfoCode(){ return unp_data.info_code; }

bool  Unpacker::GetSyncFlag(){ return unp_data.sync_flag; }
//bool  Unpacker::GetCorrFlag(){ return unp_data.corr_flag; }


Unpacker::Unpacker(){

  b_debug = false;

  b_root_tree = false;
  b_histograms = false;

  b_push_data = false;
  b_fill_tree = false;


  tm_stp_msb = 0;
  for(int i=0;i<common::N_FEE64;i++){
    corr_scaler_data0[i]= 0;
    corr_scaler_data1[i]= 0;
  }
 
  b_sync_status = false;
  //  b_corr_status = false;


  //...Parameters....
  //by default, all channels enabled
  for(int i=0;i<common::N_FEE64;i++){
    b_mod_enabled[i]= true;
  }

  /****************/
  //file name-> to load parameters
  b_mod_enabled[0]= false;
  b_mod_enabled[1]= true;
  b_mod_enabled[2]= true;
  b_mod_enabled[3]= true;
  b_mod_enabled[4]= true;
  b_mod_enabled[5]= false;
  b_mod_enabled[6]= true;
  b_mod_enabled[7]= true;
  b_mod_enabled[8]= true;
  b_mod_enabled[9]= false;
  b_mod_enabled[10]= true;
  b_mod_enabled[11]= true;
  b_mod_enabled[12]= false;
  b_mod_enabled[13]= true;
  b_mod_enabled[14]= true;
  b_mod_enabled[15]= false;
  b_mod_enabled[16]= true;
  b_mod_enabled[17]= false;
  b_mod_enabled[18]= false;
  b_mod_enabled[19]= false;
  b_mod_enabled[20]= false;
  b_mod_enabled[21]= false;
  b_mod_enabled[22]= false;
  b_mod_enabled[23]= false;
  b_mod_enabled[24]= false;
  b_mod_enabled[25]= false;
  b_mod_enabled[26]= false;
  b_mod_enabled[27]= false;
  b_mod_enabled[28]= false;
  b_mod_enabled[29]= false;
  b_mod_enabled[30]= true;
  b_mod_enabled[31]= false;
  b_mod_enabled[32]= false;
  /***************/


  unp_data.tm_stp= 0;
  unp_data.corr_scaler= 0;
  unp_data.tm_stp_lsb= 0;

  unp_data.info_field= 0;
  unp_data.adc_data= 0;
  unp_data.sample_length= 0;

  unp_data.data_type= 0;
  unp_data.fee64_id= 0;
  unp_data.ch_id= 0;
  unp_data.adc_range= 0;
  unp_data.info_code= 0;

  unp_data.sync_flag= false;
  //  unp_data.corr_flag= false; 
}




