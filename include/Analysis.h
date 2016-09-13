/*! \file Analysis.h
 * \class Analysis
 * 
 * \brief A brief description of the class.
 *
 * A more detailed description of the class and what it does.
 *
 * \author A. Estrade (additions by C. Griffin)
 * \date
 */

#ifndef _Analysis_H //what is going on here?
#define _Analysis_H

#include <iostream>

//#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TH2.h>
#include <TH1.h>

#include "Calibrator.h"
#include "DataSource.h"
#include "Common.h"

class Analysis{
 private:

  bool b_debug;              ///< TRUE: output debug info to terminal. FALSE: no debug output  
  bool b_histograms;         ///< TRUE: Create histograms. FASLE: Don't create histograms.
  bool b_root_tree;          ///< TRUE: Write output tree. FALSE: Don't write a tree.
  //  bool b_push_data;
  //  bool b_fill_tree;
  TTree * out_root_tree;     ///< Output root tree to contain

  // ----- correlations in time of different types -----
  static const int n_update= 10000000;

  //-----------------------------------------------
  //   BEGIN PARAMETERS
  //-----------------------------------------------
  //
  // ----- parameters: Set/(Get?) should be private functions and only performed once when initializing step -----
  bool b_mod_enabled[common::N_FEE64];     ///< Bool to represent whether a strip should be active in analysis. Range ?
  int geo_detector[common::N_FEE64];       ///< Int to determine which DSSD a FEE is connected to. (1->common::N_DSSD. -1 means not used)
  int geo_side[common::N_FEE64];           ///< Int to represent whether a FEE is p- or n- side. (p-side=1, n-side=0. -1 means not used) 
  int geo_strip[common::N_FEE64];          ///< Int to define position on detector FEE deals with. (1 = left/bottom half, 2 = right/top half from beam direction)
  
  double event_time_window;                ///< Size of time window for creating events (unit?)

  int dE_i_lim;                            ///< 
  int dX_i_lim;                            ///< Max strip deviation in implant particle passing between two DSSDs
  int dE_d_lim;                            ///<
  int dX_d_lim;                            ///< Max strip deviation in decay particle passing between....
  int E_i_min;                             ///< Minimum energy of implant event
  int E_d_min;                             ///< Minimum energy of decay event
  int E_d_max;                             ///< Maximum energy of decay event     

  // double t0_i, t0_d;

  int strip_min_i[common::N_DSSD][2];      ///< Highest number strip fired during implant event, i.e. high gain range
  int strip_max_i[common::N_DSSD][2];      ///< Lowest number strip fired during implant event, i.e. high gain range
  int strip_min_d[common::N_DSSD][2];      ///< Lowest number strip fired during decay event, i.e. low gain rang
  int strip_max_d[common::N_DSSD][2];      ///< Highest number strip fired during decay event, i.e. low gain range

  double e_sum_d[common::N_DSSD][2];       ///< Total energy deposited during decay event

  //-----------------------------------------------
  //   END PARAMETERS
  //-----------------------------------------------

  int event_count;                         //
  double t_low_prev;                       //
  double t_high_prev;                      //
  double t_disc_prev;                      //

  bool b_pulser;                           //

  // !!!------------------------------------!!!
  //
  // note: if new variables included, they must also be added to definition
  // of output_root_tree Branch in InitCalibrator()
  //
  // !!!-------------------------------------!!!
  /*! \struct 
   * A structure to contain the more detailed event information.
   *
   */
  struct event_struct{

    int64_t e_i[common::N_DSSD][2];   ///<
    int64_t e_d[common::N_DSSD][2];   ///<

    int64_t t0;                       ///<
    int64_t t0_ext;                   ///<
    int64_t dt;                       ///<

    int multiplicity;                 ///<

    int n_side_i[common::N_DSSD][2];  ///<
    int n_side_d[common::N_DSSD][2];  ///<

    int n_det_i[common::N_DSSD];      ///<
    int n_det_d[common::N_DSSD];      ///<

    int x_i[common::N_DSSD];          ///<
    int y_i[common::N_DSSD];          ///<
    int x_d[common::N_DSSD];          ///<
    int y_d[common::N_DSSD];          ///<

    int dx_d[common::N_DSSD];         ///<
    int dy_d[common::N_DSSD];         ///<

    unsigned char decay_flag;
    unsigned char implant_flag;

  } evt_data;

  /*! \struct
   * A structure containing the 'compact' event information.
   *
   */
  struct dssd_hits{
    Long64_t t;
    Long64_t t_ext;
    int x;
    int y;
    int z;
    int ex;
    int ey;
    int flag;
  } hit;

 public:

  //-----ADC data singles
  // . dE(low) vs ch
  // . dE(low) vs ch with discriminator
  // . dE(high) vs channel
  // . ch vs module (low, disc, high)
  //
  TH2I * hADClowCh[common::N_FEE64];        ///< N_FEE64 D array of TH2I* graphing ADC spectrum in 2D for low energy range.
  TH2I * hADChighCh[common::N_FEE64];       ///< N_FEE64 D array of TH2I* graphing ADC spectrum in 2D for high energy range.
  TH2I * hADCdiscCh[common::N_FEE64];       ///< N_FEE64 D array of TH2I* graphing ADC spectrum in 2D for fast discriminator.
  TH1I * hCh_ADClow[common::N_FEE64];       ///< N_FEE64 D array of TH1I* graphing ADC spectrum in 1D for low energy range.
  TH1I * hCh_ADChigh[common::N_FEE64];      ///< N_FEE64 D array of TH1I* graphing ADC spectrum in 1D for high energy range.
  TH1I * hCh_ADCdisc[common::N_FEE64];      ///< N_FEE64 D array of TH1I* graphing ADC spectrum in 1D for fast discriminator.

  TH1I * hADClow_all[common::N_DSSD];       ///< N_DSSD D array of TH1I* graphing ADC combined ADC spectrum of calibrated strips for whole DSSD.

  TH1I * hElow[common::N_FEE64];            ///< N_FEE64 D array of TH1I* 
  TH1I * hEdisc[common::N_FEE64];           ///< N_FEE64 D array of TH1I* 
  TH1I * hEhigh[common::N_FEE64];           ///< N_FEE64 D array of TH1I* 

  //----------Monitor performance
  // . SYNC/PAUS/RESUME per FEE
  // . ADC(high) per FEE
  // . ADC(low) per FEE
  // . ADC(disc) per FEE

  TH1I * hSyncFEE;                          ///< 
  TH1I * hPauseFEE;                         ///< 
  TH1I * hResumeFEE;                        ///< 
  TH1I * hADClowFEE;                        ///< 
  TH1I * hADChighFEE;                       ///< 
  TH1I * hADCdiscFEE;                       ///< 

  //----------- time distributions
  TH1I * hTimeADClow[common::N_DSSD];       ///< Timestamp distribution of lowE range ADC words in each DSSD.
  TH1I * hTimeADCdisc[common::N_DSSD];      ///< Timestamp distribution of fast disc words in each DSSD.
  TH1I * hTimeADChigh[common::N_DSSD];      ///< Timestamp distribution of highE range ADC words in each DSSD.

  TH1I * hTimeStamp;                        ///< 
  TH1I * hTimeStampExt;                     ///< 
  TH1I * hTimeStampFlag;                    ///< 

  //------------Event building
  //. E pulser(high multiplicity)
  //. 

  TH1I * hEvt_X[common::N_DSSD];            ///< X-strip distribution of implant events.
  TH1I * hEvt_Y[common::N_DSSD];            ///< Y-strip distribution of implant events.
  TH2I * hEvt_XY[common::N_DSSD];           ///< x-y plot of events.

  TH1I * hEvt_dX[common::N_DSSD];           ///< Change in x-strip position between DSSD planes.
  TH1I * hEvt_dY[common::N_DSSD];           ///< Change in y-strip position between DSSD planes.
  TH2I * hEvt_dXdX[common::N_DSSD];         ///< Change in x-strip position between DSSDi/DSSDi+1 vs. DSSDi+1/DSSDi+2
  TH2I * hEvt_dYdY[common::N_DSSD];         ///< Change in x-strip position between DSSDi/DSSDi+1 vs. DSSDi+1/DSSDi+2

  TH2I * hEvt_ExEy_if[common::N_DSSD];      ///< E_x vs E_y for implant events in DSSDi
  TH2I * hEvt_XY_if[common::N_DSSD];        ///< x-y plot of implant events in DSSDi
  TH1I * hEvt_Eside_if[common::N_DSSD][2];  ///< Energy spectrum of decay events in each side of DSSDi

  TH1I * hEvt_Eside[common::N_DSSD][2];     ///< Energy spectrum of n- ([][0]) and p-side ([][1]) events
  TH1I * hEvt_Eaida;                        ///< Total energy deposited by implant events over all DSSDs (per event).
  //TH1I * hEvt_Eaida_gE;                     ///<
  //TH1I * hEvt_Eaida_gX;                     ///<

  TH2I * hEvt_ExEy[common::N_DSSD];         ///< E_x vs E_y for each DSSD
  TH2I * hEvt_EdE;                          ///<
  //TH2I * HEvt_EdE_gE;
  //TH2I * HEvt_EdE_gX;
  //TH2I * HEvt_EdE_g***;                   //also advanced patterns... hits first det but not last dssd?

  TH1I * hEvt_Multi[common::N_DSSD][2];     ///< Multiplicity of implant events in DSSDi and n-/p-side [i][n=0,p=1].
  //TH2I * hEvt_MultiSide[common::N_DSSD];    ///< 
  //TH1I * hEvt_Hits_gE[4][2];              ///<
  //TH1I * hEvt_Hits_gX[4][2];              ///<

  TH1I * hEvt_HitsSide;                     ///< Tallies hits in each side of each DSSD
  TH1I * hEvt_HitsDet;                      ///< Tallies hits in each of the DSSDs
  //TH1I * hEvt_HitsDet_gE;                   ///<
  //TH1I * hEvt_HitsDet_gX;                   ///<

  TH1I * hEvt_TmStpDist[common::N_DSSD];    ///<

  //*************************************
  //            Decay events
  //*************************************
  TH1I * hEvt_Eside_d[common::N_DSSD][2];   ///< Energy spectrum of decay events for DSSDi and n-/p-sdie [i][n=0,p=1]. 
  //TH1I * hEvt_Eaida;
  //TH1I * hEvt_Eaida_gE;
  //TH1I * hEvt_Eaida_gX;

  TH2I * hEvt_ExEy_d[common::N_DSSD];       ///< E_x vs E_y for decay events
  //TH2I * hEvt_ExEy_sum_d[common::N_DSSD];   ///<

  TH2I * hEvt_EPulser_d;                    ///< E_x vs E_y for pulser events. Found by calculating (E1n+...+EN_DSSDn)/N_DSSD vs (E1p+...+EN_DSSDp)/N_DSSD.
  
  TH2I * hEvt_XY_d[common::N_DSSD];         ///< x-y distrution of decay events for each DSSD
  //TH1I * hEvt_X_d[common::N_DSSD];          ///< X-strip distribution of decay events.
  //TH1I * hEvt_Y_d[common::N_DSSD];          ///< Y-strip distribution of decay events.
  //TH1I * hEvt_dX_d[common::N_DSSD];         ///< Change in x-position of decay events between DSSD planes.
  //TH1I * hEvt_dY_d[common::N_DSSD];         ///< Change in y-position of decay events between DSSD planes.

  TH2I * hEvt_ExEy_df[common::N_DSSD];      ///< E_x vs E_y for decay events in DSSDi w/  condition decay_flag>0 && decay_flay\%10==i
  TH2I * hEvt_XY_df[common::N_DSSD];        ///< x-y plot of decay events in DSSDi w/ condition decay_flag>0 && decay_flay\%10==i
  TH1I * hEvt_Eside_df[common::N_DSSD][2];  ///< Energy spectrum of decay events each side in DSSDi w/ condition decay_flag>0 && decay_flay\%10==i

  TH2I * hEvt_ExEy_df2[common::N_DSSD];     ///< E_x vs E_y for decay events in DSSDi w/  condition decay_flag>10 && decay_flay\%10==i
  TH2I * hEvt_XY_df2[common::N_DSSD];       ///< x-y plot of decay events in DSSDi, w/ condition decay_flag>10 && decay_flay\%10==i
  TH1I * hEvt_Eside_df2[common::N_DSSD][2]; ///< Energy spectrum of decay events each side in DSSDi w/ condition decay_flag>0 && decay_flay\%10==i

  TH1I * hEvt_MultiDet_d;                      ///< Tally of decay events in DSSDs.
  TH2I * hEvt_MultiSide_d;                     ///< Tally of DSSDs/sides with decay info.
  TH1I * hEvt_MultiStrip_d[common::N_DSSD][2]; ///< Strip multiplicity of decay events (n- [][0] and p- [][1] sides)

  TH2I * hEvt_MultidX_d[common::N_DSSD][2];    ///< Strip multiplicity vs strip_max-strip_min by DSSD side

  TH2I * hEvt_MultiID;                       ///< Muliplicity of implant events vs decay events.
  TH1I * hEvt_HitsFlag;                      ///< Number of events with implant/decay flags (0:implant, 1:decay).

  // *************************************
  //                 Canvaes
  // *************************************

  TCanvas *cADClow[2];                       ///< 2D and 1D low energy range ADC spectra for all active FEEs. Panel [0] 4xN_DSSD, panel [1] 8xN_DSSD (XxY) 
  TCanvas *cADCdisc[2];                      ///< 2D and 1D discriminator spectra for all active FEEs. Panel [0] 4xN_DSSD, panel [1] not currently used.
  TCanvas *cADChigh[2];                      ///< 2D and 1D high energy range ADC spectra for all active FEEs. Panel [0] 4xN_DSSD, panel [1] not currently used.
  TCanvas *cEall[2];                         ///< Overlay of lowE ADC and fast disc spectra [0] and highE energy spectra [1].
  TCanvas *cTimeDist[common::N_DSSD];        ///< Timestamp diganostics per DSSD. All panels 

  TCanvas *cEvtE1;                           ///< Energy plots for implant events: n- and p-side energy, E_x vs E_y and same w/&w/o flag constraint
  //TCanvas *cEvtE2;                           ///<
  TCanvas *cEvtXY;                           ///< XY plots for implant events.
  TCanvas *cEvtdXdY;                         ///< dX/dY plots for fast ions between planes.
  TCanvas *cEvtMulti;                        ///< Multiplicity and DSSD/side tally of implant events.
  //TCanvas *cEvtHits;                         ///<
  //TCanvas *cEvtTS;                           ///<

  TCanvas *cEvtE_d;                          ///< Energy plots for decay events.
  TCanvas *cEvtE_df;                         ///< Energy plots for decay events, with flag constraints.
  TCanvas *cEvtXY_d;                         ///< XY plots for decay events (w + w/o constraints).
  TCanvas *cEvtXY2_d;                        ///< XY plots for decay events (n- and p-sides)
  TCanvas *cEvtMulti_d;                      ///< Multiplicity of deacy events by DSSD/side and some other things.
  //TCanvas *cEvtHit_d;                        ///<

  // *******************************************
  //                  METHODS
  // *******************************************

  Analysis();
  ~Analysis(){};

  void InitAnalysis(int opt); //
  void Process(DataSource & my_source, Calibrator & my_cal_data);
  void Close();
  //  void LoadParameters(char *file_name);

  bool BuildEvent(Calibrator & my_cal_data);
  void CloseEvent();
  void InitEvent(Calibrator & my_cal_data);

  void FillHistogramsSingles(Calibrator & my_cal_data);
  void FillHistogramsEvent();
  void UpdateHistograms();
  void ResetHistograms();

  //  bool IsValidChannel(int module, int channel);
  //  void Update();
  //  void SetCanvas2(TCanvas * canvas);

  void ResetEvent();
  void WriteHistograms();
  //  void ResetHistograms();

  void WriteOutBuffer(DataSource & my_source);

  bool IsChEnabled(Calibrator & my_cal_data);

  //get and set: which ones!

  bool SetEventTimeWindow(double value);
  //  void CalibrateAdc();

  void PrintEvent();

  //Setters...
  void SetBDebug(bool flag);
  void SetBHistograms(bool flag);
  void SetBPushData(bool flag);
  void SetBFillTree(bool flag);
  void SetBRootTree(bool flag);




  //Getters...
  double GetEventTimeWindow();

  bool GetBDebug();
  bool GetBHistograms();
  bool GetBPushData();
  bool GetBFillTree();
  bool GetBRootTree();

  int GetMultiplicity();

};


#endif
