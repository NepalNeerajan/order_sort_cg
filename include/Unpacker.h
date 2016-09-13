#ifndef _Unpacker_H //what is going on here?
#define _Unpacker_H

#include <fstream>
#include <iostream>
#include <string>


#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>

#include "DataSource.h"
#include "Common.h"

class Unpacker{
 private:
  bool b_debug;

  bool b_root_tree; //do we write data to file
  bool b_histograms;

  bool b_push_data;
  bool b_fill_tree;

  TTree * out_root_tree;

  unsigned long tm_stp_msb; //most significant bits of timestamp

  unsigned long corr_scaler_data0[common::N_FEE64]; //[16]; // [common::n_fee64]? need one for each module? how to check if it falls out of correlation?
  unsigned long corr_scaler_data1[common::N_FEE64]; //[16];

  //FF  long long corr_scaler_offset;

  bool b_sync_status;
  //  bool b_corr_status; 

  //-----------------------------------------------
  //   BEGIN PARAMETERS
  //-----------------------------------------------
  bool b_mod_enabled[common::N_FEE64]; 
  //-----------------------------------------------
  //   END PARAMETERS
  //-----------------------------------------------


  // !!!------------------------------------!!!
  //
  // note: if new variables included, they must also be added to definition
  // of output_root_tree Branch in InitUnpacker()
  //
  // !!!-------------------------------------!!!
  struct unpack_data_struct{
    unsigned long tm_stp;
    unsigned long corr_scaler;
    unsigned long tm_stp_lsb;  //least significant bits timestamp
    unsigned long info_field;

    //  unsigned short sample_data[4];
    unsigned int adc_data;
    unsigned int sample_length;
    //type: 0= sample waveform, 1= sample lenght, 2= info data, 3= ADC data
    unsigned char data_type;
    unsigned char fee64_id;
    unsigned char ch_id;
    unsigned char adc_range;
    unsigned char info_code;

    bool sync_flag;
    //    bool corr_flag;

  } unp_data;



 public:

  TCanvas *cUnp1;
  //  TH1I *hUnpMod[17]; //replace by [common::n_fee64+1] (modules start from 1?)

  TH1I *hFEE64_ADClow;
  TH1I *hFEE64_ADChigh;  
  TH1I *hFEE64_Waveform;
  TH1I *hFEE64_Info;

  TH2I *hInfoCode_FEE64;

  TH1I *hFEE64_PAUSE;
  TH1I *hFEE64_RESUME;
  TH1I *hFEE64_SYNC100;

  TH2I * hCh_FEE64_ADClow;
  TH2I * hCh_FEE64_ADChigh;
  TH2I * hCh_FEE64_DISC;


  Unpacker();
  ~Unpacker(){};


  void InitUnpacker(int opt, char *file_name); 
  void Process(DataSource & my_src_data);
  void Close();
  void LoadParameters(char *file_name);

  void FillHistograms();
  bool IsValidFee64Id();
  bool IsValidFee64Id(int mod_id);

  void UpdateHistograms();
  void WriteHistograms();
  void ResetHistograms();

  void ResetData();
  //  void Write();


  void SetBDebug(bool flag);
  void SetBHistograms(bool flag);
  void SetBPushData(bool flag);
  void SetBFillTree(bool flag);
  void SetBRootTree(bool flag);
  void SetBSyncStatus(bool flag);
  //  void SetBCorrStatus(bool flag);

  //  void SetTmStp(long long value);
  void SetTmStp();
  void SetFlags();

  void SetCorrScaler(unsigned long value);
  void SetTmStpLsb(unsigned long value);
  void SetInfoField(unsigned long value);
  void SetAdcData(unsigned int value);
  void SetSampleLength(unsigned int value);
  void SetDataType(unsigned char value);
  void SetFee64Id(unsigned char value);
  void SetChId(unsigned char value);
  void SetAdcRange(unsigned char value);
  void SetInfoCode(unsigned char value);


  unsigned long GetTmStp();
  unsigned long GetCorrScaler();
  unsigned long GetTmStpLsb();
  unsigned long GetInfoField();
  unsigned int GetAdcData();
  unsigned int GetSampleLength();
  unsigned char GetDataType();
  unsigned char GetFee64Id();
  unsigned char GetChId();
  unsigned char GetAdcRange();
  unsigned char GetInfoCode();

  bool GetSyncFlag();
  //  bool GetCorrFlag();

  bool GetBSyncStatus();
  //  bool GetBCorrStatus();

  bool GetBPushData();
  bool GetBFillTree();
  bool GetBHistograms();
  bool GetBDebug();
  bool GetBRootTree();

};


#endif
