#include "Calibrator.h"
//#include "Common.h"
//#include "DutClass.h"


#include <bitset>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <string>


const unsigned char order[64]={62, 63, 59, 60, 61, 56, 57, 58, 52, 53, 54, 55, 49, 50, 51, 45,
			       46, 47, 48, 42, 43, 44, 38, 39, 40, 41, 35, 36, 37, 31, 32, 33,
			       34, 28, 29, 30, 24, 25, 26, 27, 21, 22, 23, 17, 18, 19, 20, 14,
			       15, 16, 10, 11, 12,  7,  3,  0,  8,  4,  1,  9,  5,  2, 13,  6};


void Calibrator::Close(){
  //close root file if opened
  Write();
}

//the heart of the matter
void Calibrator::Process(Unpacker & my_unp_data){

  ResetData(); //set values of unp_data to defaults (zero)

  //channel will be set to default values for some bits of data where they don't apply (SYNC pulse)
  SetModule( my_unp_data.GetFee64Id() );
  SetChannel( my_unp_data.GetChId() );

  if( IsValidChannel(GetModule(), GetChannel() )){ //should already be checked if valid in Unpacker?
    
    SetBValidCh( true ); //not used! can be commented out ...
    SetBEnabled( b_ch_enabled[GetModule()][GetChannel()] );
    //      if( !b_ch_enabled[GetModule()][GetChannel()]) return;
    
  }
  else return; //now ensure it will be valid, no need to check again in each step....?

  SetDataType( my_unp_data.GetDataType() );

  //  SetCorrFlag(my_unp_data.GetCorrFlag());
  //  SetSyncFlag(my_unp_data.GetSyncFlag());


  //------------------------------------------------------------------
  //
  //   if INFO DATA: should only be DISC. and CORR SCALARS
  //
  //-----------------------------------------------------------------
  if(GetDataType()==2){

    if(my_unp_data.GetInfoCode()==6){ //FAST DISCRIMINATOR hit
      
      // if(my_unp_data.GetSyncFlag()) ---> we only push data from unpacker if SYNC100 pulses are fine...
      SetTmStpDisc(my_unp_data.GetTmStp()); //set function will check validity of channel

    }
    //    //if info codes for timestamp synchronization
    //    else if(my_unp_data.GetInfoCode()==2 || my_unp_data.GetInfoCode()==3 || my_unp_data.GetInfoCode()==4 ){
    //      SetBPushData(true);
    //      //fill anything?
    //    }
    //if CORRELATION SCALER 
    else if(my_unp_data.GetInfoCode()==8){

      //if(GetCorrFlag()  &&  (GetModule()==13 ||GetModule()==14 ||GetModule()==16 ) ){ //NO corr flag from Unpacker
      if(  GetModule()==13 ||GetModule()==14 ||GetModule()==16  ){

	//	double offset=  1.*my_unp_data.GetCorrScaler() - aida_time_calib*my_unp_data.GetTmStp();
	//	int64_t offset=  my_unp_data.GetCorrScaler() - my_unp_data.GetTmStp();
	int64_t offset=  4*my_unp_data.GetCorrScaler() - my_unp_data.GetTmStp(); //AIDA= 100MHz Corr scaler= 25MHz

	SetBCorrStatus(true);
	SetTmStpOffset(offset);

	if(b_debug){
	  std::cout << " OFFFFFFFFFFFFFFFFFFFFF: " << offset<<std::endl;

	  std::cout << "MODULE:"<<int(my_unp_data.GetFee64Id())<<"::  TS(aida), CORR: " << my_unp_data.GetTmStp() << ",  "<< my_unp_data.GetCorrScaler(); //<<",  "<< my_unp_data.GetCorrScaler()<<std::endl;
	  printf("  INFO 0x%lX \n", my_unp_data.GetInfoField());
	}

      }

      SetInfoCode(my_unp_data.GetInfoCode());
      SetBPushData(true);
      SetBFillTree(true);
    }

    //    if(GetBPushData() || GetBFillTree()){
    //      SetInfoCode(my_unp_data.GetInfoCode());
    //    }
  }


  //------------------------------------------------------------------
  //
  //   if ADC DATA: 
  //-----------------------------------------------------------------
  if(GetDataType()==3){

    SetAdcData(my_unp_data.GetAdcData());
    SetAdcRange(my_unp_data.GetAdcRange()) ;

    if(SetGeometry()){ //calculates DSSD, side, strip, ...

      CalibrateAdc(); //energy... range...

      //time discriminator set after calculation of time-stamp
      if( GetBEnabled() ) SetBPushData(true); //if not a dissabled chanel (e.g. noisy ch)

      SetBFillTree(true);
    }
  }


  //if we're interested in this event
  if(GetBPushData() || GetBFillTree() ){

    //timing only if it makes sense...


    SetTimeAida(my_unp_data.GetTmStp());

    if(GetDataType()==3) SetDiscFlag(SetTimeDisc()); // check if thre is a fast disc. value for this ADC hit

    SetCorrFlag();
    SetTimeExternal();


    if(b_debug){
      if(GetDataType()==2 && (my_unp_data.GetInfoCode()==8 || my_unp_data.GetInfoCode()==4 )){
	if(GetModule()==13 ||GetModule()==14 ||GetModule()==16 ){
	  std::cout << " TIME AIDA: "<< GetTimeAida() << "  TIME EXT: "<< GetTimeExternal() 
		    <<  "   offset: "<<tm_stp_corr_offset <<std::endl;
	}
      }
    }

  }


  if( GetBRootTree() && GetBFillTree()){
    
    //save this entry to TTree
    out_root_tree->Fill();
  }

 
  if(GetBHistograms()){
    //Fill histograms here

    //Do we need any of the time-stamp variables calculated only for GetBPushData() or GetBFillTree()?
    FillHistograms(); 
  } 
 

}


void Calibrator::FillHistograms(){
  //fee 1 to 16:
  if(GetModule()>0 && GetModule()<17){

    if(GetAdcRange()==0){ //decay
      hCalElCh[GetModule()-1]->Fill(GetChannel(),GetAdcData());
    }
    else if(GetAdcRange()==1){ //decay
      hCalEhCh[GetModule()-1]->Fill(GetChannel(),GetAdcData());
    }

    hist_fill_count++;
    if( ( hist_fill_count%n_update)==0 ) UpdateHistograms();
  }
}

void Calibrator::UpdateHistograms(){
  if(GetBHistograms()){
    
    for(int i=0;i<16;i++){
      cCal1->cd(i+1); hCalEhCh[i]->Draw("colz");
      cCal2->cd(i+1); hCalElCh[i]->Draw("colz");
    }
    cCal1->Update();
    cCal2->Update();
    if(GetBDebug()){
      std::cout << "   Calibrator::UpdateHistograms() at cnt " << hist_fill_count << std::endl;
    }

  }
}

void Calibrator::ResetHistograms(){
  if(GetBHistograms()){
    std::cout << ".... Calibrator:: and here we reset histograms; not implemented yet"<<std::endl;
  }
}

bool Calibrator::IsValidChannel(int module, int channel){
  if(module >=0 && module < common::N_FEE64){
    if(channel >=0 && channel < common::N_CHANNEL){
      return true; //common:n_fee64?
    }
  }

  return false;
}


void Calibrator::InitCalibrator(int opt, char *file_name){

  //file name-> to load parameters


  // if first of 'opt' is 1, enable histogramming
  if( (opt & 0x01) == 1){

    SetBHistograms(true);
    
    cCal1= new TCanvas("cCal1","cCal1",50,50,900,900); cCal1->Divide(4,4);
    cCal2= new TCanvas("cCal2","cCal2",90,90,900,900); cCal2->Divide(4,4);
    
    char hname[256];
    char htitle[256];
    for(int i=0;i<16;i++){
      sprintf(hname,"hCalEhCh%i",i+1);
      sprintf(htitle,"NNAIDA%i [20GeV range];ch;ADC data",i+1);
      hCalEhCh[i]= new TH2I(hname, htitle, 64, 0, 64, 512, 0, 65536);

      cCal1->cd(i+1); hCalEhCh[i]->Draw("colz");  //here, or specific Draw function?

      sprintf(hname,"hCalElCh%i",i+1);
      sprintf(htitle,"NNAIDA%i [20MeV range];ch;ADC data",i+1);
      hCalElCh[i]= new TH2I(hname, htitle, 64, 0, 64, 512, 0, 65536);

      cCal2->cd(i+1); hCalElCh[i]->Draw("colz");  //here, or specific Draw function?


    }
    //hUnpModAdc = new TH1I("hUnpModAdc","Fee64 for ADC data;module id",17,0,17);
  
    if(b_debug) std::cout << "db    Calibrator.cpp: Initialized histograms and canvas for this step"<<std::endl;
  }

  //if second bit of 'opt' is 1, enable TTree for output
  if( ( (opt & 0x02) >> 1)  == 1){

    std::cout << " ***     Calibrator::InitCalibrator(): initializing TTree" << std::endl;
    //Initialize TTree
    out_root_tree = new TTree("AIDA_calibrate","AIDA_calibrate");
    // (25/4) removed sync_flag!
    // (16/6) removed sync_flag again!
    out_root_tree->Branch("entry_calibrate",&cal_data,"time_aida/D:time_disc/D:time_external/D:adc_energy/D:adc_data/I:dssd/I:strip/I:adc_range/b:side/b:module/b:channel/b:data_type/b:info_code/b:corr_flag/O:disc_flag/O");
                                
    out_root_tree->SetAutoSave(-500000000);
    out_root_tree->SetAutoFlush(-50000000);


    std::cout << "  !!!!! Calibrator:: TTrree->GetAutoSave(): "<< out_root_tree->GetAutoSave() <<" !!! "<<std::endl;

    SetBRootTree(true);
  }

  LoadParameters(file_name);

  ResetData(); //sets values of unp_data structure to default values

}


void Calibrator::LoadParameters(char * file_name){

  //char* file_name= (char*)"test.dat";

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

	if(GetBDebug()) std::cout << " * LoadParam() - line: " << my_line << std::endl;

	//skip empty lines and comments
	if(!my_line.empty() && '\n' != my_line[0] && '#' != my_line[0]){
	
	  std::istringstream iss(my_line);  
	  y = -1; //?? o a valid number
	  x = -1;
	  z= -1;
	  // par_name[0]= '\0';

	  iss >> par_name >> x >> y >> z;


	  if(GetBDebug()) std::cout << " * LoadParameter() - values read: " << par_name << " "<< x << y << z << std::endl;

	  /******************
	  if(par_name=="b_mod_enabled"){
	    module= x; b_data= int(y);
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      b_mod_enabled[module]= data;
	    }
	  }
	  **********************/
	  if(par_name=="map_dssd"){
	    module= x; data= y;
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      map_dssd[module]= int(data);
	    }
	  }
	  else if(par_name=="map_side"){
	    module= x; data= y;
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      map_side[module]= int(data);
	    }
	  }
	  else if(par_name=="map_strip"){
	    module= x; data= y;
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      map_strip[module]= int(data);
	    }
	  }
	  else if(par_name=="adc_polarity"){
	    module= x; data= y;
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      adc_polarity[module]= int(data);
	    }
	  }
	  else if(par_name=="adc_gain_highE"){
	    module= x; data= y;
	    if(IsValidChannel(module, 0)){
	      par_count++;
	      adc_gain_highE[module]= data;
	    }
	  }
	  else if(par_name=="b_ch_enabled"){
	    module= x; channel= y;
	    data= z;
	    if(IsValidChannel(module, channel)){
	      par_count++;
	      b_ch_enabled[module][channel]= int( data );
	    }
	  }
	  else if(par_name=="adc_offset"){
	    module= x; channel= y;
	    data= z;
	    if(IsValidChannel(module, channel)){
	      par_count++;
	      adc_offset[module][channel]= data;
	    }
	  }
	  else if(par_name=="adc_gain"){
	    module= x; channel= y;
	    data= z;
	    if(IsValidChannel(module, channel)){
	      par_count++;
	      adc_gain[module][channel]= data;
	    }
	  }
	  else if(par_name=="disc_time_window"){
	    //module= x; channel= y;
	    data= x;
	    disc_time_window= x;
	    //if(IsValidChannel(module, channel)){
	    //  par_count++;
	    //  adc_gain[module][channel]= data;
	    //}
	  }
	  else if(par_name=="aida_time_calib"){
	    //module= x; channel= y;
	    data= x;
	    aida_time_calib= x;
	  }

	  else{
	    if(b_debug) std::cout << " Calibrator.cpp:: parameter name not recognized: " << par_name << "." <<std::endl;
	  }
	
	  /***********************/
	}
	

      }
      else{
	std::cout << " Calibrator.cpp:: LoadParameters() reached end of parameter file. Loaded new value for "<< par_count << " parameters." << std::endl;
	break;
      }
      //check forever loop not running amok
      line_count++;
      if(line_count>MAX_LINES){

	std::cout << " Calibrator.cpp:: count of lines read by LoadParameters() exceeds maximum allowed value (" << MAX_LINES << "). Parameters loaded= " << par_count<<std::endl;
	std::cout << "                  Break out of loop and continue... *****************"  << std::endl << std::endl;
	break;
      }
    }

  }
  else{
    std::cout << " Calibrator.cpp:: Error opening parameter file: " << file_name << " *****************"  << std::endl << std::endl;
  }
  return;

}




void Calibrator::ResetData(){



  
  b_push_data = false;
  b_fill_tree = false;

  //reset this?
  b_enabled= false;
  b_valid_ch= false;


  cal_data.time_aida= 0;
  cal_data.time_disc= 0;
  cal_data.time_external= 0;

  cal_data.adc_energy= 0;
  cal_data.dssd= 0;
  cal_data.strip= 0;

  cal_data.adc_range= 0;
  cal_data.side= 0;
  cal_data.module= 0;
  cal_data.channel= 0;
  cal_data.data_type= 0;
  cal_data.info_code= 0;

  //cal_data.sync_flag= false;
  cal_data.corr_flag= false; 
  cal_data.disc_flag= false; 


}


void Calibrator::Write(){
  if(GetBRootTree()){


    out_root_tree->Write(0,TObject::kOverwrite);
    std::cout << "\n *** writing ng Clibrator TTree .... "<< std::endl;
    out_root_tree->Print();
  }
  if(GetBHistograms()){

    for(int i=0;i<16;i++){
      hCalEhCh[i]->Write();
      hCalElCh[i]->Write();
    }
  }
}

bool Calibrator::SetGeometry(){


  //if(GetBValidCh()){

    cal_data.dssd = map_dssd[GetModule()];

    // higher number channels for strips at center of detector (FEE for each half)
    if(map_strip[GetModule()] == 1){
      // cal_data.strip = 63 - OrderChannel( GetChannel() );
      cal_data.strip = OrderChannel( GetChannel() );
    } else if(map_strip[GetModule()]== 2){
      //      cal_data.strip = 64 + OrderChannel( GetChannel() ); 
      cal_data.strip = 127 -  OrderChannel( GetChannel() ); 
    } else {
      cal_data.strip = -1;
    }

    //    if( map_side[GetModule()] == 0 || map_side[GetModule()]==1 )
    cal_data.side = map_side[GetModule()];

    return true;
    //}

    //return false;
}

void Calibrator::CalibrateAdc(){

  if(GetAdcRange() == 0){ //decay

    double value;
    //    value = GetAdcData() - common::ADC_ZERO + adc_offset[GetModule()][GetChannel()];
    value = GetAdcData() - common::ADC_ZERO - adc_offset[GetModule()][GetChannel()];
    SetAdcEnergy(adc_polarity[GetModule()] * adc_gain[GetModule()][GetChannel()] * value);
  }
  else if(GetAdcRange() == 1){
    double value;
    value = GetAdcData() - common::ADC_ZERO;
    SetAdcEnergy(adc_polarity[GetModule()] * adc_gain_highE[GetModule()] * value);
  }
  else SetAdcEnergy(-99999);

}

 
unsigned char Calibrator::OrderChannel( unsigned char ch ){
  //assumes we have already checked channel is within valid range!... likely in enabled channels check.
  return order[ch];
}


void Calibrator::SetBDebug(bool flag){
  b_debug= flag;
}
void Calibrator::SetBHistograms(bool flag){
  b_histograms= flag;
}
void Calibrator::SetBPushData(bool flag){
  b_push_data= flag;
}
void Calibrator::SetBFillTree(bool flag){
  b_fill_tree= flag;
}

void Calibrator::SetBRootTree(bool flag){
  if(flag){
    if( out_root_tree != NULL){
      b_root_tree= true;
      std::cout << " ***       Calibrator::SetBRootTree(): set to TRUE"<<std::endl;
    }
    else {
      std::cout << " ** WARNING ** Calibrator.cpp: attempted to set b_root_tree as valid when TTree is not initialized! *** " << std::endl;
      b_root_tree= false;
    }
  }
  else b_root_tree= false;
}

void Calibrator::SetBEnabled(bool flag){
  b_enabled= flag;
}

void Calibrator::SetBValidCh(bool flag){
  b_valid_ch= flag;
}

void Calibrator::SetBCorrStatus(bool flag){
  b_corr_status= flag;
}

//void Calibrator::SetBCorrOffset(bool flag){
// b_corr_offset= flag;
//}


void Calibrator::SetTmStpDisc(unsigned long value){
  //  if(GetBValidCh()){
    tm_stp_disc[GetModule()][GetChannel()]= value;
    //  }

}

void Calibrator::SetTmStpOffset(int64_t value){ //diff AIDA->EXTERNAL tm-stp

  //double offset=  aida_time_calib*(my_unp_data.GetCorrScaler() - my_unp_data.GetTmStp());

  tm_stp_corr_offset= value; 
  //tm_stp_corr_offset= 1.*t_EXT - aida_time_calib*t_AIDA;

}




void Calibrator::SetTimeAida(double value){
  cal_data.time_aida= value;
}

bool Calibrator::SetTimeDisc(){


  /********************
    //check we have detected previous value for DISC for this channel
    if(tm_stp_disc[GetModule()][GetChannel()]>0){

      cal_data.time_disc= tm_stp_disc[GetModule()][GetChannel()];

      tm_stp_disc[GetModule()][GetChannel()]=0; //clear value of stored DISC timestamp for this channel

      if( (GetTimeAida() - cal_data.time_disc < disc_time_window) && (GetTimeAida()>=cal_data.time_disc) ){
	return true;
      }
      else return false; 
    }


  return false;
  ***********************/
  return false;
}


void Calibrator::SetTimeExternal(){

  //  if(GetCorrFlag() && GetSyncFlag()){  
  if(GetCorrFlag()){  

    cal_data.time_external=(cal_data.time_aida + tm_stp_corr_offset)/  aida_time_calib; 
    //    cal_data.time_external= aida_time_calib*(cal_data.time_aida + tm_stp_corr_offset); 
  }
  else cal_data.time_external= 0;
}

void Calibrator::SetAdcEnergy(double value){
  cal_data.adc_energy= value;
}

void Calibrator::SetAdcData(int value){
  cal_data.adc_data= value;
}
void Calibrator::SetDSSD(int value){
  cal_data.dssd= value;
}

void Calibrator::SetStrip(int value){
  cal_data.strip= value;
}

void Calibrator::SetAdcRange(unsigned char value){
  cal_data.adc_range= value;
}

void Calibrator::SetSide(unsigned char value){
  cal_data.side= value;
}

void Calibrator::SetModule(unsigned char value){
  cal_data.module= value;
}

void Calibrator::SetChannel(unsigned char value){
  cal_data.channel= value;
}

void Calibrator::SetDataType(unsigned char value){
  cal_data.data_type= value;
}

void Calibrator::SetInfoCode(unsigned char value){
  cal_data.info_code= value;
}

//void Calibrator::SetSyncFlag(bool value){
//  cal_data.sync_flag= value;
//}

void Calibrator::SetCorrFlag(){

  cal_data.corr_flag= GetBCorrStatus() ;
}

void Calibrator::SetDiscFlag(bool value){
  cal_data.disc_flag= value;
}








  //Getters...
bool Calibrator::GetBDebug(){ return b_debug; }
bool Calibrator::GetBHistograms(){ return b_histograms; }
bool Calibrator::GetBPushData(){ return b_push_data; }
bool Calibrator::GetBFillTree(){ return b_fill_tree; }
bool Calibrator::GetBRootTree(){ return b_root_tree; }

bool Calibrator::GetBEnabled(){ return b_enabled; }
bool Calibrator::GetBValidCh(){ return b_valid_ch; }
bool Calibrator::GetBCorrStatus(){ return b_corr_status; }


unsigned long Calibrator::GetTmStpDisc(){ 
  //if(GetBValidCh()){
  return tm_stp_disc[GetModule()][GetChannel()];
  //}
  //else return 0;
}

int64_t Calibrator::GetTmStpOffset(){ return tm_stp_corr_offset; }

//Getters... for cal_data structure
double Calibrator::GetTimeAida(){ return cal_data.time_aida; }
double Calibrator::GetTimeDisc(){ return cal_data.time_disc; }
double Calibrator::GetTimeExternal(){ return cal_data.time_external; }
double Calibrator::GetAdcEnergy(){ return cal_data.adc_energy; }

int Calibrator::GetAdcData(){ return cal_data.adc_data; }
int Calibrator::GetDSSD(){ return cal_data.dssd; }
int Calibrator::GetStrip(){ return cal_data.strip; }

unsigned char Calibrator::GetAdcRange(){ return cal_data.adc_range; }
unsigned char Calibrator::GetSide(){ return cal_data.side; }
unsigned char Calibrator::GetModule(){ return cal_data.module; }
unsigned char Calibrator::GetChannel(){ return cal_data.channel; }
unsigned char Calibrator::GetDataType(){ return cal_data.data_type; }
unsigned char Calibrator::GetInfoCode(){ return cal_data.info_code; }

//bool Calibrator::GetSyncFlag(){ return cal_data.sync_flag; }
bool Calibrator::GetCorrFlag(){ return cal_data.corr_flag; }
bool Calibrator::GetDiscFlag(){ return cal_data.disc_flag; }





Calibrator::Calibrator(){

  out_root_tree = NULL;

  b_debug = false;

  b_root_tree = false;
  b_histograms = false;

  b_push_data = false;
  b_fill_tree = false;

  for(int i=0;i<common::N_FEE64;i++){
    for(int j=0;j<common::N_CHANNEL;j++){
      tm_stp_disc[i][j]=0;    
    }
  }
  tm_stp_corr_offset=0;
 

  //...Parameters....
  //by default, all channels enabled
  for(int i=0;i<common::N_FEE64;i++){
    for(int j=0;j<common::N_CHANNEL;j++){
      b_ch_enabled[i][j]= true;
    }
    //    b_mod_enabled[i]= true;
  }

  /****************
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
  ***************/
  


  //simplest possible....
  for(int i=0;i<common::N_FEE64;i++){
    map_dssd[i]= i/4;
    if( (i%4)<2 ) map_side[i]=0; //0,1,4,5,8,9 ...
    else map_side[i]= 1; //2,3,6,7,10,11 ...
    map_strip[i]= i%2; // even -> 0, odd -> 1
  }

  for(int i=0;i<common::N_FEE64;i++){
    for(int j=0;j<common::N_CHANNEL;j++){
      adc_offset[i][j]= 0;
      adc_gain[i][j]= 0.7; // keV/ch
    }
    if( (i%4)<2 ) adc_polarity[i]= 1;
    else adc_polarity[i]= -1;
    adc_gain_highE[i]= 0.7; // MeV/ch
  }

  disc_time_window= 50000;
  aida_time_calib= 2; //     ???????AIDA: 25MHz, BRIKEN: 50Hz

  b_enabled= false;
  b_valid_ch= false;

  b_corr_status= false;

  cal_data.time_aida= 0;
  cal_data.time_disc= 0;
  cal_data.time_external= 0;
  cal_data.adc_energy= 0;

  cal_data.adc_data= 0;
  cal_data.dssd= 0;
  cal_data.strip= 0;

  cal_data.adc_range= 0;
  cal_data.side= 0;
  cal_data.module= 0;
  cal_data.channel= 0;
  cal_data.data_type= 0;
  cal_data.info_code= 0;

  //cal_data.sync_flag= false;
  cal_data.corr_flag= false; 
  cal_data.disc_flag= false; 

}


