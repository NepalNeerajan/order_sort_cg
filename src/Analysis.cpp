/*! 
 * \file Analysis.cpp
 * \class Analysis
 *
 * Semi-commented C++ file responsible for the analysis section of the AIDA data processing.
 * A class which takes unpakced and calibrated AIDA data, builds events and carries out some analysis.
 *
 * \author A. Estrade (additions from C. Giffin)
 * \date
 */

#include "Analysis.h"
//#include "Common.h"
//#include "DutClass.h"

//#include <bitset>
//#include <fstream>
#include <iomanip>
#include <stdio.h>
//#include <sstream>
#include <string>

//the heart of the matter

/*! \fn Process(DataSource& my_source, Calibrator& my_cal_data)
 * Processes data from my_source or my_cal_data
 * \param[in] my_source DataSource type which comes from somewhere...
 * \param[in] my_cal_data Calibrated data passed from Calibrator.cpp...
 * \return void
 */
void Analysis::Process(DataSource & my_source, Calibrator & my_cal_data){

  //ResetData(); //set values of unp_data to defaults (zero)
  //channel will be set to default values for some bits of data where they don't apply (SYNC pulse)
  
  if(GetBHistograms()) FillHistogramsSingles(my_cal_data);

  //if end of this event
  if( BuildEvent(my_cal_data) ){

    if(GetMultiplicity()>0){
       CloseEvent();

      if(b_debug){
	PrintEvent();
      }

      if(GetBHistograms()) FillHistogramsEvent();
      if(my_source.GetBSendData()) WriteOutBuffer(my_source);
    }
    InitEvent(my_cal_data);
  }
}

//the real heart of the matter

/*! \fn BuildEvent(Calibrator& my_cal_data)
 * Build events from the calibrated data passed from Calibrator.cpp
 * \param[in] my_cal_data Calibrated raw data from Calibrator.cpp
 * \return Bool: TRUE if ...., FALSE if...
 */
bool Analysis::BuildEvent(Calibrator & my_cal_data){

  if(!my_cal_data.GetAdcRange() && !my_cal_data.GetModule()==30) return false; //skip low energy range of NNAIDA#30
 
  if(!IsChEnabled(my_cal_data)) return false; //skip channels not enabled

  //consider windows backwards and forwards in time
  if(0){
    if( my_cal_data.GetTimeDisc() > ( evt_data.t0 + event_time_window)
	|| my_cal_data.GetTimeDisc() < ( evt_data.t0 - event_time_window) ){

      return true;
    }
  }
  else  if( my_cal_data.GetTimeAida() > ( evt_data.t0 + event_time_window)
     || my_cal_data.GetTimeAida() < ( evt_data.t0 - event_time_window) ){
    
    return true; //time to start a new event!
  }

  //If good data to add to current event....

  evt_data.dt= my_cal_data.GetTimeAida() - evt_data.t0; //assume monotonically increasing tm-stps

  int det= my_cal_data.GetDSSD();         //Detector triggered
  int strip= my_cal_data.GetStrip();      //Strip triggered
  int side= my_cal_data.GetSide();        //Side triggered
  int energy;                             //Energy of 'event'
  int range = my_cal_data.GetAdcRange();  //High(1) or low(0) energy range

  //Get energy of event and normalise for ADC zero point
  if(det>0 ){
    if(side==0) energy= my_cal_data.GetAdcData() - common::ADC_ZERO;
    else if(side==1) energy = common::ADC_ZERO - my_cal_data.GetAdcData();
  }
  else if (det==0) energy = common::ADC_ZERO - my_cal_data.GetAdcData();

  evt_data.multiplicity++;
    
  //IMPLANT IMPLANT IMPLANT
  if( range == 1 ){                 //If highE range...
    evt_data.n_det_i[det-1]= evt_data.n_det_i[det-1]+1;               //Add to implant event counters for detector
    evt_data.n_side_i[det-1][side]= evt_data.n_side_i[det-1][side]+1; //and side
    
    if(energy>evt_data.e_i[det-1][side]){   //if new max value for E of this event (energy is current, evt_data.e_i is previous)
      evt_data.e_i[det-1][side]= energy;
      if(side==0) evt_data.y_i[det-1]= strip;       //Set highest Edep strip for... n-side: horizontal strips
      else if(side==1) evt_data.x_i[det-1]= strip;  //                              p-side: vertical strips
    }
  }
 
  //DECAY DECAY DECAY
  else if( range == 0 ){                            //If lowE range...
    evt_data.n_det_d[det-1]= evt_data.n_det_d[det-1]+1;                //Add to decay event counters for detector
    evt_data.n_side_d[det-1][side]= evt_data.n_side_d[det-1][side]+1;  //and side
    
    e_sum_d[det-1][side]= e_sum_d[det-1][side]+energy;    //Sum energy of decay events
    
    if(strip>strip_max_d[det-1][side]) strip_max_d[det-1][side]= strip;  //Set min/max strips fired to
    if(strip<strip_min_d[det-1][side]) strip_min_d[det-1][side]= strip;  //gague multiplicity and spread

    if(energy>evt_data.e_d[det-1][side]){   //If new max value for E of this event...
      evt_data.e_d[det-1][side]= energy;
      if(side==0) evt_data.y_d[det-1]= strip;      //Set highest Edep strip for... n-side: horizontal strips
      else if(side==1) evt_data.x_d[det-1]= strip; //                              p-side: vertical strips
    }
  }
  
  // -----------------------------------------------------------
  //   Fill some histograms with internal event data
  // ----------------------------------------------------------
  if(GetBHistograms()){
    
    hEvt_TmStpDist[0]->Fill( evt_data.dt );
    hEvt_TmStpDist[3]->Fill( my_cal_data.GetTimeAida() - evt_data.t0 );
    
    if(my_cal_data.GetDiscFlag()) hEvt_TmStpDist[2]->Fill( my_cal_data.GetTimeDisc() - evt_data.t0 );
    
  }
  return false;
}

/*! \fn CloseEvent()
 * Determines type of event and writes event info to output tree
 *
 *
 */
void Analysis::CloseEvent(){

  bool b_beam= false;
  //  if(evt_data.n_det_i[0]>0 && evt_data.e_i[0][0]>18000 && evt_data.e_i[0][0]<30500){
  //  b_beam= true;
  // }

  int NmaxI=4;        //Max number of strips to be triggered in implant event
  int EminI=500;      //Min Edep for event to be considered real
  int N_max_decay=4;  //Man number of strips to be triggered in decay event


  //IMPLANTS IMPLANTS

  //IMPLANTS IMPLANTS
  // Det#1  Det#1  Det#1
  if( evt_data.n_side_i[0][0]>0 && evt_data.n_side_i[0][1]>0 ){
    //Zero implant events in all downstream DSSDs
    if( evt_data.n_det_i[1]==0 && evt_data.n_det_i[2]==0 && evt_data.n_det_i[3]==0 && evt_data.n_det_i[4]==0 && evt_data.n_det_i[5]==0  ){
      //... small cluster (after implementing strip_min/max_i[][]) ....
      if( evt_data.n_side_i[0][0]<NmaxI && evt_data.n_side_i[0][1]<NmaxI ){
	//some minimum energy deposition (using Emax only for now)
	if( evt_data.e_i[0][0]>EminI && evt_data.e_i[0][1]>EminI ){
	  evt_data.implant_flag= 1;
	}
      }
    }
  }

  // Det#2  Det#2  Det#2
  if( evt_data.n_side_i[1][0]>0 && evt_data.n_side_i[1][1]>0 ){
    //Zero implant events in all downstream DSSDs
    if( evt_data.n_det_i[2]==0 && evt_data.n_det_i[3]==0 && evt_data.n_det_i[4]==0 && evt_data.n_det_i[5]==0 ){
      //... small cluster (after implementing strip_min/max_i[][]) ....
      if( evt_data.n_side_i[1][0]<NmaxI && evt_data.n_side_i[1][1]<NmaxI ){
	//some minimum energy deposition (using Emax only for now)
	if( evt_data.e_i[1][0]>EminI && evt_data.e_i[1][1]>EminI ){
	  evt_data.implant_flag= 2;
	}
      }
    }
  }
  
  // Det#3  Det#3  Det#3
  if( evt_data.n_side_i[2][0]>0 && evt_data.n_side_i[2][1]>0 ){
    //Zero implant events in all downstream DSSDs
    if( evt_data.n_det_i[3]==0 && evt_data.n_det_i[4]==0 && evt_data.n_det_i[5]==0  ){
      //... small cluster (after implementing strip_min/max_i[][]) ....
      if( evt_data.n_side_i[2][0]<NmaxI && evt_data.n_side_i[2][1]<NmaxI ){
	//some minimum energy deposition (using Emax only for now)
	if( evt_data.e_i[2][0]>EminI && evt_data.e_i[2][1]>EminI ){
	  evt_data.implant_flag= 3;
	}
      }
    }
  }

  // Det#4  Det#4  Det#4
  if( evt_data.n_side_i[3][0]>0 && evt_data.n_side_i[3][1]>0 ){
    //Zero implant events in all downstream DSSDs
    if( evt_data.n_det_i[4]==0 && evt_data.n_det_i[5]==0 ){
      //... small cluster (after implementing strip_min/max_i[][]) ....
      if( evt_data.n_side_i[3][0]<NmaxI && evt_data.n_side_i[3][1]<NmaxI ){
	//some minimum energy deposition (using Emax only for now)
	if( evt_data.e_i[3][0]>EminI && evt_data.e_i[3][1]>EminI ){
	  evt_data.implant_flag= 4;
	}
      }
    }
  }
  
  // Det#5  Det#5  Det#5
  if( evt_data.n_side_i[4][0]>0 && evt_data.n_side_i[4][1]>0 ){
    //Zero implant events in all downstream DSSDs
    if( evt_data.n_det_i[5]==0 ){
      //... small cluster (after implementing strip_min/max_i[][]) ....
      if( evt_data.n_side_i[4][0]<NmaxI && evt_data.n_side_i[4][1]<NmaxI ){
	//some minimum energy deposition (using Emax only for now)
	if( evt_data.e_i[4][0]>EminI && evt_data.e_i[4][1]>EminI ){
	  evt_data.implant_flag= 5;
	}
      }
    }
  }
  
  //IMPLANTS IMPLANTS
  // Det#6  Det#6  Det#6
  if( evt_data.n_side_i[5][0]>0 && evt_data.n_side_i[5][1]>0 ){
    //Detectors 1-5 have N>1
     if( evt_data.n_det_i[0]>1 && evt_data.n_det_i[1]>1 && evt_data.n_det_i[2]>1 && evt_data.n_det_i[3]>1 && evt_data.n_det_i[4]>1 ){
      //dX(56) is ok
      if( abs(evt_data.x_i[4]-evt_data.x_i[5]) < dX_i_lim && abs(evt_data.y_i[4]-evt_data.y_i[5]) < dX_i_lim){
	//... small cluster (after implementing strip_min/max_i[][]) ....
	if( evt_data.n_side_i[5][0]<NmaxI && evt_data.n_side_i[5][1]<NmaxI ){
	  //some minimum energy deposition (using Emax only for now)
	  if(evt_data.e_i[5][0]>EminI && evt_data.e_i[5][1]>EminI){
	    evt_data.implant_flag= 6;
	  }
	}
      }
    }
  }

  // Pulser event?
  if(evt_data.implant_flag<1){  // If no implant
    if( (evt_data.n_side_d[0][0]>35 && evt_data.n_side_d[1][0]>35 && evt_data.n_side_d[2][0]>35 &&  /* If all p-side DSSDs fire more than 35 strips */
	       evt_data.n_side_d[3][0]>35 && evt_data.n_side_d[4][0]>35 && evt_data.n_side_d[5][0]>35) 
	|| 
	(evt_data.n_side_d[0][1]>35 && evt_data.n_side_d[1][1]>35 && evt_data.n_side_d[2][1]>35 &&  /* Or all n-side DSSDs fire mroe than 35 strips */
	       evt_data.n_side_d[3][1]>35 && evt_data.n_side_d[4][1]>35 && evt_data.n_side_d[5][1]>35) ){
      b_pulser= true;    
    }
  }
  
  // *******************************************
  //              DECAYS DECAYS
  // *******************************************

  if(!b_pulser && evt_data.implant_flag==0){    // If not a pulser and 
    
    for(int det=0; det < common::N_DSSD; det++) {  // Loop over detectors ( 0 -> common::N_DSSD )
      
      // If number of low energy strips fired > 0, but less than the max set above (N_max_decay)
      if(evt_data.n_side_d[det][0]>0 && evt_data.n_side_d[det][1]>0 && evt_data.n_side_d[det][0]<=N_max_decay && evt_data.n_side_d[det][1]<=N_max_decay){
	if(evt_data.e_d[det][0]>E_d_min && evt_data.e_d[det][1]>E_d_min){   // If E_dep > than minmum energy required to count as decay
	  if(evt_data.e_d[det][0]<E_d_max && evt_data.e_d[det][1]<E_d_max){    // If E_dep < than maximum likely decay product energy
	    if( (strip_max_d[det][0]-strip_min_d[det][0]) < (evt_data.n_side_d[det][0]+1) ){  // If p-side (x) strip cluster < #strips fired +1, i.e. compact cluster
	      if( (strip_max_d[det][1]-strip_min_d[det][1])< (evt_data.n_side_d[det][1]+1)){  // If n-side (y) strip cluster < #strips fired +1, i.e. compact cluster
		
		evt_data.decay_flag= det;

	      } //Cluster size y
	    } //Cluster size x
	  } //E<Emax 
	} //E>Emin
      } //Decay Det2
    } //Loop over det #
  } //Not implant or pulser hit
  
  
  if(GetBRootTree()){
    
    if(evt_data.implant_flag>0){     // If implant event present...

      int det= evt_data.implant_flag%10;

      //Set info
      hit.t= evt_data.t0;
      hit.t_ext= evt_data.t0_ext;
      hit.z= det;
      hit.x= evt_data.x_i[det];
      hit.y= evt_data.y_i[det];
      hit.ex= evt_data.e_i[det][1];            // p-side strips
      hit.ey= evt_data.e_i[det][0];            // n-side strips
      hit.flag= 1+ evt_data.implant_flag%10;   //1, 2, ..., 6. Implant flag from Calibrator.cpp 10+det# (i.e. DSSD 1 = det0) (?)

      out_root_tree->Fill();  // Write to tree
    }
    
    else if(evt_data.decay_flag>0){    // Else is decay event present... (do the same)

      int det= evt_data.decay_flag%10;

      hit.t= evt_data.t0;
      hit.t_ext= evt_data.t0_ext;
      hit.z= det;
      hit.x= evt_data.x_d[det];
      hit.y= evt_data.y_d[det];
      hit.ex= evt_data.e_d[det][1];           // p-side strips
      hit.ey= evt_data.e_d[det][0];           // n-side strips
      hit.flag= 11 + evt_data.decay_flag%10;  //11, 12, ..., 16

      out_root_tree->Fill();
  
    }

  }

}

/*! \fn InitEvent(Calibrator& my_cal_data)
 *  Initialise event
 * 
 * \param[in] my_cal_data Calibrated raw data
 */
void Analysis::InitEvent(Calibrator & my_cal_data){
    
  ResetEvent();

  //check again just in case we're trying to initialize with wrong data
  if(!my_cal_data.GetAdcRange() || !IsChEnabled(my_cal_data)){
    evt_data.t0= -9999999; //bogus timestamp... should take care of things
  }

  if(my_cal_data.GetDiscFlag()){
    evt_data.t0= my_cal_data.GetTimeDisc();
    evt_data.t0_ext= my_cal_data.GetTimeExternal();

  }
  else{
    evt_data.t0= my_cal_data.GetTimeAida();
    evt_data.t0_ext= my_cal_data.GetTimeExternal();

  }
  //evt_data.t0= my_cal_data.GetTimeAida();

  BuildEvent(my_cal_data);


}

/*! \fn WriteOutBuffer(DataSource& my_source)
 *
 *
 *
 */
void Analysis::WriteOutBuffer(DataSource & my_source){

  //int s_double= sizeof(double);
  //int s_int= sizeof(int);
  //int s_char= sizeof(char);

  int offset=my_source.GetBuffOffset(); 
  //int 4
  //char 1

  //  int my_int[4][2];
  //  for(int i=0;i<4;i++){
  //    my_int[i][0]= evt_data.e_i[1][0];
  //  }

  int32_t aida_id= 0xA1DA;

  memcpy(my_source.BufferOut+offset, (char*) &aida_id, sizeof(int32_t) );
  memcpy(my_source.BufferOut+offset+sizeof(int32_t), (char*) &evt_data, sizeof(evt_data) );


  if(b_debug){
    std::cout << "\n size of evt_data: "<< sizeof(evt_data) << std::endl ;
    
    int j=0;
    for(int i= offset; i< offset+sizeof(evt_data)+sizeof(int32_t); i++){
      if((j%16)==0) std::cout<< std::endl <<" -- "; //printf("\n");
      if( (j%4)==0) std::cout << " 0x";
      //    std::cout << std::setw(2) << std::hex << (unsigned char) my_source.BufferOut[i]; 
      
      printf("%02hhx",my_source.BufferOut[i]);
      
      j++;
    }
  }

  offset= offset + sizeof(evt_data)+sizeof(int32_t);

  /********************
  //                                                  1234567812345678
  my_source.BufferOut[offset]= ( 0xABCDABCD << 8 ) |(int)evt_data.e_i[0][0]  & 0xFFFFFFFF ;
  my_source.BufferOut[offset+1*s_double]= ( 0xABCDABCD << 8 ) | (int)evt_data.e_i[0][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+2*s_double]= (int)evt_data.e_i[1][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+3*s_double]= (int)evt_data.e_i[1][1] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+4*s_double]= (int)evt_data.e_i[2][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+5*s_double]= (int)evt_data.e_i[2][1] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+6*s_double]= (int)evt_data.e_i[3][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+7*s_double]= (int)evt_data.e_i[3][1] & 0xFFFFFFFFFFFFFFFF ;

  offset= offset+ 8*s_double;

  //  for(int i=0;i<4;i++){
    //    my_int[i][0]= evt_data.e_d[1][0];
    //  }


  my_source.BufferOut[offset]= (int)evt_data.e_d[0][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+1*s_double]= (int)evt_data.e_d[0][1] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+2*s_double]= (int)evt_data.e_d[1][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+3*s_double]= (int)evt_data.e_d[1][1] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+4*s_double]= (int)evt_data.e_d[2][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+5*s_double]= (int)evt_data.e_d[2][1] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+6*s_double]= (int)evt_data.e_d[3][0] & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+7*s_double]= (int)evt_data.e_d[3][1] & 0xFFFFFFFFFFFFFFFF ;

  offset= offset+ 8*s_double;


  my_source.BufferOut[offset]= (int)evt_data.t0 & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+s_double]= (int)evt_data.t0_ext & 0xFFFFFFFFFFFFFFFF ;
  my_source.BufferOut[offset+2*s_double]= (int)evt_data.dt & 0xFFFFFFFFFFFFFFFF ;

  offset= offset + 3*s_double;
  
  //                                                     12345678
  my_source.BufferOut[offset]= evt_data.multiplicity & 0xFFFFFFFF ;

  my_source.BufferOut[offset+s_int]= evt_data.n_side_i[0][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+2*s_int]= evt_data.n_side_i[0][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+3*s_int]= evt_data.n_side_i[1][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+4*s_int]= evt_data.n_side_i[1][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+5*s_int]= evt_data.n_side_i[2][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+6*s_int]= evt_data.n_side_i[2][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+7*s_int]= evt_data.n_side_i[3][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+8*s_int]= evt_data.n_side_i[3][1] & 0xFFFFFFFF ;
  
  my_source.BufferOut[offset+9*s_int]= evt_data.n_side_d[0][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+10*s_int]= evt_data.n_side_d[0][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+11*s_int]= evt_data.n_side_d[1][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+12*s_int]= evt_data.n_side_d[1][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+13*s_int]= evt_data.n_side_d[2][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+14*s_int]= evt_data.n_side_d[2][1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+15*s_int]= evt_data.n_side_d[3][0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+16*s_int]= evt_data.n_side_d[3][1] & 0xFFFFFFFF ;

  offset= offset + 17*s_int;
  
  my_source.BufferOut[offset]= evt_data.n_det_i[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+s_int]= evt_data.n_det_i[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+2*s_int]= evt_data.n_det_i[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+3*s_int]= evt_data.n_det_i[3] & 0xFFFFFFFF ;

  my_source.BufferOut[offset+4*s_int]= evt_data.n_det_d[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+5*s_int]= evt_data.n_det_d[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+6*s_int]= evt_data.n_det_d[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+7*s_int]= evt_data.n_det_d[3] & 0xFFFFFFFF ;

  my_source.BufferOut[offset+8*s_int]= evt_data.x_i[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+9*s_int]= evt_data.x_i[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+10*s_int]= evt_data.x_i[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+11*s_int]= evt_data.x_i[3] & 0xFFFFFFFF ;

  my_source.BufferOut[offset+12*s_int]= evt_data.y_i[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+13*s_int]= evt_data.y_i[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+14*s_int]= evt_data.y_i[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+14*s_int]= evt_data.y_i[3] & 0xFFFFFFFF ;

  my_source.BufferOut[offset+16*s_int]= evt_data.x_d[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+17*s_int]= evt_data.x_d[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+18*s_int]= evt_data.x_d[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+19*s_int]= evt_data.x_d[3] & 0xFFFFFFFF ;

  my_source.BufferOut[offset+20*s_int]= evt_data.y_d[0] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+21*s_int]= evt_data.y_d[1] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+22*s_int]= evt_data.y_d[2] & 0xFFFFFFFF ;
  my_source.BufferOut[offset+23*s_int]= evt_data.y_d[3] & 0xFFFFFFFF ;

  offset= offset+ 24*s_int;


  my_source.BufferOut[offset]= evt_data.decay_flag & 0xFF ;
  my_source.BufferOut[offset+s_char]= evt_data.implant_flag & 0xFF ;

  offset= offset+2*s_char;
  ********************/

  my_source.SetBuffOffset(offset);
  my_source.TransferBuffer(evt_data.t0);

  return;

}

/*! \fn FillHistogramSingles(Calibrator& my_cal_data)
 * 
 *
 *
 *
 */
void Analysis::FillHistogramsSingles(Calibrator & my_cal_data){
  
  int mod= my_cal_data.GetModule();
  int det= my_cal_data.GetDSSD();
  int ch= my_cal_data.GetChannel();
  int side= my_cal_data.GetSide();
  int range= my_cal_data.GetAdcRange();

  if(b_debug) std::cout << " about to fill histograms-singles....mod= "<< mod << " range= "<<range  << std::endl;

  if(b_mod_enabled[mod]){
    if(det<0 || det>common::N_DSSD){
      std::cout << "\n******************     det" << det<< "->0  ***********************"<<std::endl;
      det=0;
    }
    
    if(ch<0 || ch>63){
      std::cout << "\n******************     ch" << ch<< "->0  ***********************"<<std::endl;
      ch=0;
    }
    
    if(mod<0 || mod>common::N_FEE64){
      std::cout << "\n******************     mod" << mod<< "->0  ***********************"<<std::endl;
      mod=0;
    }
    
    if(side<0 || side>2){
      std::cout << "\n******************     side" << side<< "->0  ***********************"<<std::endl;
      side=0;
    }
    
    //DECAY DECAY DECAY
    if(my_cal_data.GetAdcRange()==0){
      hADClowCh[mod-1]->Fill(ch,my_cal_data.GetAdcData());
      hCh_ADClow[mod-1]->Fill(ch);
      hElow[mod-1]->Fill(my_cal_data.GetAdcData());

      if(my_cal_data.GetDiscFlag()) hEdisc[mod-1]->Fill(my_cal_data.GetAdcData());
      
      if(my_cal_data.GetDiscFlag()){
	hADCdiscCh[mod-1]->Fill(ch,my_cal_data.GetAdcData());
	hCh_ADCdisc[mod-1]->Fill(ch);
      }
      
      hTimeADClow[0]->Fill(my_cal_data.GetTimeAida()-t_low_prev);
      t_low_prev= my_cal_data.GetTimeAida();
      
      if(my_cal_data.GetDiscFlag()){
	hTimeADCdisc[0]->Fill(my_cal_data.GetTimeDisc()-t_disc_prev);
	t_disc_prev= my_cal_data.GetTimeDisc();
      }
    }

    //IMPLANT IMPLANT IMPLANT
    else if(my_cal_data.GetAdcRange()==1){
      hADChighCh[mod-1]->Fill(ch,my_cal_data.GetAdcData());
      hCh_ADChigh[mod-1]->Fill(ch);
      hEhigh[mod-1]->Fill(my_cal_data.GetAdcData());
      
      hTimeADChigh[0]->Fill(my_cal_data.GetTimeAida()-t_high_prev);
      t_high_prev= my_cal_data.GetTimeAida();
    }
    
    if(0) std::cout << " TS(aida), TS(ext): " << my_cal_data.GetTimeAida()<<",  "<< my_cal_data.GetCorrFlag() << ":  "<< my_cal_data.GetTimeExternal()<<std::endl;
    
    hTimeStamp->Fill( my_cal_data.GetTimeAida() );
    
    if(my_cal_data.GetCorrFlag()){
      hTimeStampExt->Fill( my_cal_data.GetTimeExternal() );
      
      hTimeStampFlag->Fill( 1 );
    }
    else  hTimeStampFlag->Fill( 0 );
    
  }
}

/*! \fn FillHistogramsEvent()
 *
 *
 *
 */
void Analysis::FillHistogramsEvent(){

  double e_det[common::N_DSSD] = {0}; // Highest single E_dep per DSSD in FillHistogramsEvents()
  double e_aida = 0;                  // Total E_dep from implants over all DSSDs in FillHistogramsEvents()
  //bool b_gE = true;                   //
  //bool b_gX = true;                   //

  int multi_d = 0;                    // Total number of decay events across all detectors in FillHistogramsEvents()
  int multi_i = 0;                    // Total number of implant events across all detectors in FillHistogramsEvents()

  if(b_debug) std::cout<<" ...evt histogrms..." << std::endl;

  for(int i=0;i<common::N_DSSD; i++) {

    multi_i += evt_data.n_det_i[i];    //Add to cumulative number of events
    multi_d += evt_data.n_det_d[i];    //

    if(evt_data.e_i[i][0]>evt_data.e_i[i][1]) e_det[i]=evt_data.e_i[i][0];  //Find highest E_dep in each DSSD
    else e_det[i]=evt_data.e_i[i][1];
    
    if(evt_data.n_det_i[i]>0) e_aida += e_det[i];    //Total E_dep over all DSSDs
  }  //End DSSD loop - summation calcs
  
  hEvt_MultiID->Fill(multi_i,multi_d);
  if(evt_data.implant_flag>0) hEvt_HitsFlag->Fill(0);
  if(evt_data.decay_flag>0) hEvt_HitsFlag->Fill(1);

  if(b_debug) {
    // PrintEvent();
    printf("        eaida %f,    edet[i]  %f  %f  %f  %f \n", e_aida, e_det[0], e_det[1], e_det[2], e_det[3]);
  }

  if(multi_i>0) {   //If implant events....

    for(int i=0;i<common::N_DSSD; i++){
      
      for(int j=0;j<2;j++){
	if(evt_data.n_side_i[i][j]>0){
	  hEvt_Eside[i][j]->Fill( evt_data.e_i[i][j] );
	  hEvt_Multi[i][j]->Fill(evt_data.n_side_i[i][j]);
	  
	  if( (evt_data.implant_flag%10)==i+1 && evt_data.implant_flag>0) hEvt_Eside_if[i][j]->Fill( evt_data.e_i[i][j] );
	  
	}  // End if implant events in side
      }  // End sides
      
      if(evt_data.n_det_i[i]>0){    //If implant events in DSSDi+1... Fill hists.
	
	hEvt_ExEy[i]->Fill(evt_data.e_i[i][1],evt_data.e_i[i][0]);
	
	hEvt_X[i]->Fill(evt_data.x_i[i]);
	hEvt_Y[i]->Fill(evt_data.y_i[i]);
	hEvt_XY[i]->Fill(evt_data.x_i[i],evt_data.y_i[i]);
	
	if((evt_data.implant_flag%10)==i+1 && evt_data.implant_flag>0){
	  hEvt_ExEy_if[i]->Fill(evt_data.e_i[i][1],evt_data.e_i[i][0]);
	  hEvt_XY_if[i]->Fill(evt_data.x_i[i],evt_data.y_i[i]);
	}
      }

      if(evt_data.n_det_i[i]>0){
	hEvt_HitsDet->Fill(i+1,1);
      }
    } // End DSSDs
        
    hEvt_Eaida->Fill(e_aida);         //Fill total energy deposited histo
    hEvt_EdE->Fill(e_det[0],e_aida);  //
    
    // **** Track heavy particles ****
    for(int i=0; i<common::N_DSSD; i++) {
      
      if(evt_data.n_det_i[i] > 0 && evt_data.n_det_i[i+1] > 0 && i<common::N_DSSD-1) {   // Position change between DSSDi and DSSDi+1
	hEvt_dX[i]->Fill(evt_data.x_i[i+1] - evt_data.x_i[i]);
	hEvt_dY[i]->Fill(evt_data.y_i[i+1] - evt_data.y_i[i]);
      }

      if(evt_data.n_det_i[i] > 0 && evt_data.n_det_i[i+1] > 0 && evt_data.n_det_i[i+2] > 0 && i<common::N_DSSD-2) {  //Pos change between DSSDi+1/DSSDi vs DSSDi+2/DSSDi+1
	hEvt_dXdX[i]->Fill(evt_data.x_i[i+1] - evt_data.x_i[i], evt_data.x_i[i+2] - evt_data.x_i[i+1]);
	hEvt_dYdY[i]->Fill(evt_data.y_i[i+1] - evt_data.y_i[i], evt_data.y_i[i+2] - evt_data.y_i[i+1]);
      }
    } // End loop over DSSDs

    if(evt_data.n_det_i[0] > 0 && evt_data.n_det_i[common::N_DSSD-1] > 0) {     //Position change between first and last DSSD
      hEvt_dX[common::N_DSSD-1]->Fill(evt_data.x_i[common::N_DSSD-1] - evt_data.x_i[0]);
      hEvt_dY[common::N_DSSD-1]->Fill(evt_data.y_i[common::N_DSSD-1] - evt_data.y_i[0]);

      if(evt_data.n_det_i[common::N_DSSD/2] > 0) {     //Position change between first/middle vs middle/last
	hEvt_dXdX[common::N_DSSD-2]->Fill(evt_data.x_i[common::N_DSSD/2] - evt_data.x_i[0], evt_data.x_i[common::N_DSSD-1] - evt_data.x_i[common::N_DSSD/2]);
	hEvt_dYdY[common::N_DSSD-2]->Fill(evt_data.y_i[common::N_DSSD/2] - evt_data.y_i[0], evt_data.y_i[common::N_DSSD-1] - evt_data.y_i[common::N_DSSD/2]);
      }
    }
    // **** End particle tracking ****
    
    for(int i=0;i<common::N_DSSD;i++){     //Fill number of hits per side
      if(evt_data.n_side_i[i][0]>0) hEvt_HitsSide->Fill(i+1,1); 
      if(evt_data.n_side_i[i][1]>0) hEvt_HitsSide->Fill(i+1+0.5,1); 
    }
    
    /**************
    if(b_gE){
      hEvt_Eaida_gE->Fill(e_aida);
    }
    
    if(b_gX){
      hEvt_Eaida_gX->Fill(e_aida);
      // hEvt_HitsDet_gX->Fill(i,1);
    }
    *****************/
    
    /*************
    for(int i=1;i<common::N_DSSD;i++){
      if(evt_data.n_det_i[i]>0){

	if(b_gE) hEvt_HitsDet_gE->Fill(i,1);
	if(b_gX) hEvt_HitsDet_gX->Fill(i,1);
      }
    }
    **************/
    
    hEvt_TmStpDist[1]->Fill( evt_data.dt );
    
    if(b_debug) std::cout << "db     Analysis::FillHistrgramsEvent():   done with evt_implant (multi="<<multi_i<<")"<< std::endl;
    
  } //End implant events
  
  if(b_pulser) {    //If pulser event...
    hEvt_EPulser_d->Fill( (evt_data.e_d[0][0] + evt_data.e_d[1][0] + evt_data.e_d[2][0] + evt_data.e_d[3][0] + evt_data.e_d[4][0] + evt_data.e_d[5][0])/6.,
			  (evt_data.e_d[0][1] + evt_data.e_d[1][1] + evt_data.e_d[2][1] + evt_data.e_d[3][1] + evt_data.e_d[4][1] + evt_data.e_d[5][1])/6. );
  } //End pulser events
  
  else if(multi_d>0){
    
    // EVENT : DECAY : HISTOGRAMS
    int multi_det_d=0;
    
    for(int i=0;i<common::N_DSSD; i++) {   //Loop over all DSSDs 
      
      if(evt_data.n_det_d[i]>0) multi_det_d++;   //Counter for decay events
      
      for(int j=0;j<2;j++) {               //Loop over sides
	if(evt_data.n_side_d[i][j]>0) {   //If side has decays events...
	  
	  hEvt_Eside_d[i][j]->Fill( evt_data.e_d[i][j] );  //Fill hist
	  
	  if(evt_data.decay_flag>0 && (evt_data.decay_flag%10) == i+1){
	    hEvt_Eside_df[i][j]->Fill( evt_data.e_d[i][j] );
	    if(evt_data.decay_flag>10) hEvt_Eside_df2[i][j]->Fill( evt_data.e_d[i][j] );
	  }
	}
      }
      
      if(evt_data.n_det_d[i]>0) {   //If DSSD has decay events...
	
	hEvt_ExEy_d[i]->Fill(evt_data.e_d[i][1],evt_data.e_d[i][0]);
	hEvt_XY_d[i]->Fill(evt_data.x_d[i],evt_data.y_d[i]);
	
	if(evt_data.decay_flag>0 && (evt_data.decay_flag%10) == i+1){
	  hEvt_ExEy_df[i]->Fill(evt_data.e_d[i][1],evt_data.e_d[i][0]);
	  hEvt_XY_df[i]->Fill(evt_data.x_d[i],evt_data.y_d[i]);
	  
	  if(evt_data.decay_flag>10){
	    hEvt_ExEy_df2[i]->Fill(evt_data.e_d[i][1],evt_data.e_d[i][0]);
	    hEvt_XY_df2[i]->Fill(evt_data.x_d[i],evt_data.y_d[i]);
	  }
	}
      }
      
      hEvt_MultiDet_d->Fill(multi_det_d);
      
      if(evt_data.n_side_d[i][0]>0 && evt_data.n_side_d[i][1]>0)       hEvt_MultiSide_d->Fill(2,i+1); //If decay events in both sides
      else if(evt_data.n_side_d[i][1]>0 && evt_data.n_side_d[i][0]==0) hEvt_MultiSide_d->Fill(1,i+1); //Else if only decay events in p-side
      else if(evt_data.n_side_d[i][0]>0 && evt_data.n_side_d[i][1]==0) hEvt_MultiSide_d->Fill(0.,i+1.); //Else if only decay events in n-side
      
      if(evt_data.n_side_d[i][0]>0){
	hEvt_MultiStrip_d[i][0]->Fill(evt_data.n_side_d[i][0]);
	hEvt_MultidX_d[i][0]->Fill(evt_data.n_side_d[i][0],strip_max_d[i][0]-strip_min_d[i][0]);
		
	if(evt_data.n_side_d[i][1]>0){
	  hEvt_MultiStrip_d[i][1]->Fill(evt_data.n_side_d[i][0]);
	  hEvt_MultidX_d[i][1]->Fill(evt_data.n_side_d[i][1],strip_max_d[i][1]-strip_min_d[i][1]);
	}
      }
    }
    if(b_debug) std::cout << "db     Analysis::FillHistrgramsEvent():   done with evt_decay (multi="<<multi_d<<")"<< std::endl;
    
  }
  
}

/*!
 *
 *
 *
 */

void Analysis::UpdateHistograms(){
  
  if(GetBHistograms()){
    std::cout << "  Analysis::UpdateHistograms()... updating"<<std::endl;
    
    for(int i=0;i<2;i++){
      for(int j=0;j<4*common::N_DSSD;j++){
	if(i==0){
	  cADClow[i]->cd(j+1)->Modified();
	  cADCdisc[i]->cd(j+1)->Modified();
	  cADChigh[i]->cd(j+1)->Modified();
	} //End if(i==0)
	
	cEall[i]->cd(j+1)->Modified();
	
      } //End j loop
      
      for(int j=0;j<(8*4);j++){
	cADClow[1]->cd(j+1)->Modified();
      }
      
      cEall[i]->Update();
    }
    
    cADClow[0]->Update();
    cADCdisc[0]->Update();
    cADChigh[0]->Update();
    cADClow[1]->Update();
    
    
    
    for(int i=1;i<8;i++){
      cTimeDist[0]->cd(i)->Modified();
    }
    cTimeDist[0]->Update();
    
    
    for(int i=0;i<common::N_DSSD*6;i++) {
      if(i<common::N_DSSD*2) {
	cEvtXY2_d  ->cd(i+1)->Modified();
      }
      if(i<common::N_DSSD*3) {
	cEvtXY     ->cd(i+1)->Modified();
	cEvtMulti  ->cd(i+1)->Modified();
	cEvtXY_d   ->cd(i+1)->Modified();
	cEvtMulti_d->cd(i+1)->Modified();
      }
      if(i<common::N_DSSD*4) {
	cEvtdXdY   ->cd(i+1)->Modified();
      }
      if(i<common::N_DSSD*5) {
	cEvtE_d    ->cd(i+1)->Modified();
      }
      if(i<common::N_DSSD*6) {
	cEvtE1     ->cd(i+1)->Modified();
      }
    }
    cEvtE1->Update();
    cEvtXY->Update();
    cEvtdXdY->Update();
    cEvtMulti->Update();
    cEvtE_d->Update();
    cEvtXY_d->Update();
    cEvtXY2_d->Update();
    cEvtMulti_d->Update();
    }
}

void Analysis::InitAnalysis(int opt){
  
  event_count= 0;
  t_low_prev= 0;
  t_high_prev= 0;
  t_disc_prev= 0;

  //  b_implant_det2= false;
  b_pulser= false;

  ResetEvent();

  //file name-> to load parameters
  b_mod_enabled[0]= true;
  b_mod_enabled[1]= true;
  b_mod_enabled[2]= true;
  b_mod_enabled[3]= true;
  b_mod_enabled[4]= true;
  b_mod_enabled[5]= true;
  b_mod_enabled[6]= true;
  b_mod_enabled[7]= true;
  b_mod_enabled[8]= true;
  b_mod_enabled[9]= true;
  b_mod_enabled[10]= true;
  b_mod_enabled[11]= true;
  b_mod_enabled[12]= true;
  b_mod_enabled[13]= true;
  b_mod_enabled[14]= true;
  b_mod_enabled[15]= true;
  b_mod_enabled[16]= true;
  b_mod_enabled[17]= true;
  b_mod_enabled[18]= true;
  b_mod_enabled[19]= true;
  b_mod_enabled[20]= true;
  b_mod_enabled[21]= true;
  b_mod_enabled[22]= true;
  b_mod_enabled[23]= true;
  b_mod_enabled[24]= false;
  b_mod_enabled[25]= false;
  b_mod_enabled[26]= false;
  b_mod_enabled[27]= false;
  b_mod_enabled[28]= false;
  b_mod_enabled[29]= false;
  b_mod_enabled[30]= false;
  b_mod_enabled[31]= false;
  //b_mod_enabled[33]= false;

  /******/
  std::string nombre[32];
  nombre[0]="NNAIDA1 (Det6, Pside)";
  nombre[1]="NNAIDA2 (Det5, Pside)";
  nombre[2]="NNAIDA3 (Det6, Nside)";
  nombre[3]="NNAIDA4 (Det5, Nside)";
  nombre[4]="NNAIDA5 (Det6, Pside)";
  nombre[5]="NNAIDA6 (Det5, Pside)";
  nombre[6]="NNAIDA7 (Det5, Nside)";
  nombre[7]="NNAIDA8 (Det6, Nside";
  nombre[8]="NNAIDA9 (Det4, Pside)";
  nombre[9]="NNAIDA10 (Det3, Pside)";
  nombre[10]="NNAIDA11 (Det4, Nside)";
  nombre[11]="NNAIDA12 (Det3, Nside)";
  nombre[12]="NNAIDA13 (Det4, Pside)";
  nombre[13]="NNAIDA14 (Det3, Pside)";
  nombre[14]="NNAIDA15 (Det3, Nside)";
  nombre[15]="NNAIDA16 (Det4, Nside)";
  nombre[16]="NNAIDA17 (Det1, Pside)";
  nombre[17]="NNAIDA18 (Det2, Pside)";
  nombre[18]="NNAIDA19 (Det2, Nside)";
  nombre[19]="NNAIDA20 (Det1, Nside)";
  nombre[20]="NNAIDA21 (Det2, Pside)";
  nombre[21]="NNAIDA22 (Det1, Pside)";
  nombre[22]="NNAIDA23 (Det1, Nside)";
  nombre[23]="NNAIDA24 (Det2, Nside)";
  nombre[24]="NNAIDA25 (DetX, Xside)";
  nombre[25]="NNAIDA26 (DetX, Xside)";
  nombre[26]="NNAIDA27 (DetX, Xside)";
  nombre[27]="NNAIDA28 (DetX, Xside)";
  nombre[28]="NNAIDA29 (DetX, Xside)";
  nombre[29]="NNAIDA30 (DetX, Xside)";
  nombre[30]="NNAIDA31 (DetX, Xside)";
  nombre[31]="NNAIDA32 (DetX, Xside)";
  /***********/

  int m_side[32]= {1,1,0,0,     /* 1:4 */
		   1,1,0,0,     /* 5:8 */
		   1,1,0,0,     /* 9:12 */
		   1,1,0,0,     /* 13:16 */
		   1,1,0,0,     /* 17:20 */
		   1,1,0,0,     /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,-1,-1,-1  /* 29:32 */};

  int m_dssd[32]= {6,5,6,5,     /* 1:4 */
		   6,5,5,6,     /* 5:8 */
		   4,3,4,3,     /* 9:12 */
		   4,3,3,4,     /* 13:16 */
		   1,2,2,1,     /* 17:20 */
		   2,1,1,2,     /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,-1,-1,-1  /* 29:32 */};

  int m_strip[32]= {1,1,1,1,    /* 1:4 */
		    2,2,2,2,    /* 5:8 */
		    1,1,1,1,    /* 9:12 */
		    2,2,2,2,    /* 13:16 */
		    1,1,1,1,    /* 17:20 */
		    2,2,2,2,    /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,-1,-1,-1  /* 29:32 */};

  for(int i=0;i<common::N_FEE64;i++){
    geo_side[i]= m_side[i];
    geo_detector[i]= m_dssd[i];
    geo_strip[i]= m_strip[i];
    
  }

  // if first of 'opt' is 1, enable histogramming
  if( (opt & 0x01) == 1){
    char hname[256];
    char htitle[256];
    std::string stitle;
    std::string full_title;

    SetBHistograms(true);
    
    // for(int i=0;i<common::N_DSSD;i++){
    sprintf(hname,"cADCspec_lowE_2D");
    cADClow[0]= new TCanvas(hname, hname, 10,10,1200,900);  cADClow[0]->Divide(4,common::N_DSSD);
    sprintf(hname,"cADCspec_lowE_1D");
    cADClow[1]= new TCanvas(hname, hname, 10,10,1800,1000); cADClow[1]->Divide(8,common::N_DSSD);
    sprintf(hname,"cADCspec_highE_0");
    cADChigh[0]= new TCanvas(hname, hname, 20,20,1200,900); cADChigh[0]->Divide(4,common::N_DSSD);
    sprintf(hname,"cADCdisc_0");
    cADCdisc[0]= new TCanvas(hname, hname, 30,30,1200,900); cADCdisc[0]->Divide(4,common::N_DSSD);
    sprintf(hname,"cEall_0");
    cEall[0]= new TCanvas(hname, hname, 40,40,1200,900);    cEall[0]->Divide(4,common::N_DSSD);
    sprintf(hname,"cEall_1");
    cEall[1]= new TCanvas(hname, hname, 40,40,1200,900);    cEall[1]->Divide(4,common::N_DSSD);
    
    //sprintf(hname,"cADChigh_1");
    //cADChigh[1]= new TCanvas(hname, hname, 20,20,1200,900); cADChigh[1]->Divide(5,3);
    //sprintf(hname,"cADCdisc_1");
    //cADCdisc[1]= new TCanvas(hname, hname, 30,30,1200,900); cADCdisc[1]->Divide(5,3);
        
    for(int i=0;i<common::N_FEE64;i++){
      if(b_mod_enabled[i]){
	
	int k;
	k = 4*(geo_detector[i]-1)+geo_side[i]*2+geo_strip[i];    //Unique i.d. number for all FEE64s 1->common::N_FEE64(inc). Groups DSSDs together.
	int l;
	l = 8*(geo_detector[i]-1)+geo_side[i]*2+geo_strip[i];    //Unique i.d. number for all FEE64s 1->common::N_FEE64(inc). Groups high/low ADC range together w/ DSSD.
	int m;
	m = 8*(geo_detector[i]-1)+geo_side[i]*2+geo_strip[i]+4;  //Unique i.d. number for all FEE64s 1->common::N_FEE64(inc). Groups high/low ADC range together w/ DSSD.
	
	if(k<1 || k>common::N_FEE64 || l<1 || l>((common::N_DSSD*8)-4) || m<5 || m>(common::N_DSSD*8)) {
	  std::cout << " **** Analysis::InitAnalysis(): ERROR IN GEOMETRY"<<
	    "\n        mod, det, side, strip: " << i << " " << geo_detector[i]<< " " << geo_side[i]<< " " << "  " <<geo_strip[i]<< std::endl;
	}
	
	// *******************************************
	//                ADC singles
	// *******************************************
	
	//Low E range 2D ADC plots
	sprintf(hname,"hADClowFEE%iDSSD%i",i+1,geo_detector[i]);
	sprintf(htitle,"2D ADC plot for lowE range;ch;ADC data");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hADClowCh[i]= new TH2I(hname, full_title.data(), 64, 0, 64, 1024, 0, 65536);
	cADClow[0]->cd(k); hADClowCh[i]->Draw("colz"); gPad->SetLogz(1);
	
	//Low E range 1D ADC spectrum
	sprintf(hname,"hCh_ADClowFEE%i",i+1);
	sprintf(htitle,"1D ADC plot for lowE range;ch");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hCh_ADClow[i]= new TH1I(hname, full_title.data(), 64, 0, 64);
	cADClow[1]->cd(l); hCh_ADClow[i]->Draw("");

	//High E range 2D ADC plots
	sprintf(hname,"hADChighFEE%i",i+1);
	sprintf(htitle," ADC(high);ch;ADC data");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hADChighCh[i]= new TH2I(hname, full_title.data(), 64, 0, 64, 1024, 0, 65536);
	cADChigh[0]->cd(k); hADChighCh[i]->Draw("colz"); gPad->SetLogz(1);

	//High E range 1D ADC spectrum
	sprintf(hname,"hCh_ADChighFEE%i",i);
	sprintf(htitle," ADC(high);ch");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hCh_ADChigh[i]= new TH1I(hname, full_title.data(), 64, 0, 64);
	cADClow[1]->cd(m); hCh_ADChigh[i]->Draw("");

	// ADC(decay range) ! discriminator hit
	sprintf(hname,"hADCdiscFEE%i",i+1);
	sprintf(htitle," ADC(low) !DISC hits;ch;ADC data");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hADCdiscCh[i]= new TH2I(hname, full_title.data(), 64, 0, 64, 1024, 0, 65536);
	cADCdisc[0]->cd(k); hADCdiscCh[i]->Draw("colz"); gPad->SetLogz(1);

	//Fast disc hits
	sprintf(hname,"hCh_ADCdiscFEE%i",i+1);
	sprintf(htitle," ADC(low) !DISC hit;ch");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	hCh_ADCdisc[i]= new TH1I(hname, full_title.data(), 64, 0, 64);
	
	//Low E range energy spectrum
	sprintf(hname,"hElowFEE%i",i+1);
	sprintf(htitle," E(low); E [MeV]");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	if(geo_side[i]==0)       hElow[i]= new TH1I(hname, full_title.data(), 1000, 30000, 65000);
	else if(geo_side[i]==1)  hElow[i]= new TH1I(hname, full_title.data(), 1000, 0, 34000);
	cEall[0]->cd(k); hElow[i]->Draw(""); gPad->SetLogy(1);

	//Fast disc energy spectrum
	sprintf(hname,"hEdiscFEE%i",i+1);
	sprintf(htitle," E(disc); E [MeV]");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	if(geo_side[i]==0)       hEdisc[i]= new TH1I(hname, full_title.data(), 1000, 30000, 65000);
	else if(geo_side[i]==1)  hEdisc[i]= new TH1I(hname, full_title.data(), 1000, 0, 34000);
	hEdisc[i]->SetLineColor(kRed);
	cEall[0]->cd(k); hEdisc[i]->Draw("same"); gPad->SetLogy(1);

	//High energy range enrgy spectrum
	sprintf(hname,"hEhighFEE%i",i+1);
	sprintf(htitle," E(high); E [GeV]");
	stitle= htitle;
	full_title= nombre[i]+stitle;
	if(geo_side[i]==0)       hEhigh[i]= new TH1I(hname, full_title.data(), 1000, 30000, 50000);
	else if(geo_side[i]==1)  hEhigh[i]= new TH1I(hname, full_title.data(), 1000, 15000, 34000);
	cEall[1]->cd(k); hEhigh[i]->Draw(""); gPad->SetLogy(1);

      } //End if(b_mod_enabled)
    } //End loop over common::N_FEE64
    
    for(int i=0;i<common::N_DSSD;i++){

      sprintf(hname,"cTimeDist_DSSD%i",i+1);
      cTimeDist[i] = new TCanvas(hname, hname, 40,40,1100,700); cTimeDist[i]->Divide(4,2);
     
      //time stamp distribution for ADClow
      sprintf(hname,"hTimeADClow_DSSD%i",i+1);
      sprintf(htitle,"#DeltaT ts !ADC(low);time stamp difference");
      hTimeADClow[i] = new TH1I(hname, htitle, 1000, -500, 9500);

      cTimeDist[i]->cd(1); hTimeADClow[i]->Draw(""); gPad->SetLogy(1);

      //time stamp distribution for ADCdisc
      sprintf(hname,"hTimeADCdisc_DSSD%i",i+1);
      sprintf(htitle,"#DeltaT ts !ADC(disc);time stamp difference");
      hTimeADCdisc[i] = new TH1I(hname, htitle, 1000, -500, 9500);
      cTimeDist[i]->cd(2); hTimeADCdisc[i]->Draw(""); gPad->SetLogy(1);
      
      //time stamp distribution for ADChigh
      sprintf(hname,"hTimeADChigh_DSSD%i",i+1);
      sprintf(htitle,"#Deltat ts !ADC(high);time stamp difference");
      hTimeADChigh[i] = new TH1I(hname, htitle, 1000, -500, 9500);
      cTimeDist[i]->cd(3); hTimeADChigh[i]->Draw(""); gPad->SetLogy(1);
    
    } //End loop over common::N_DSSD

    hTimeStamp = new TH1I("hTimeStamp","Time stamp;tmstp [1/1e6]",1000,0,1e5);
    //  cTimeDist[0]->cd(4);  hTimeStamp->Draw(""); gPad->SetLogy(1);

    hTimeStampExt = new TH1I("hTimeStampExt","Time stamp External;tmstp [1/1e6]",1000,0,1e5);
    //  cTimeDist[0]->cd(5);  hTimeStampExt->Draw(""); gPad->SetLogy(1);

    hTimeStampFlag = new TH1I("hTimeStampFlag","Time stamp Ext Flag;corr_flag",2,0,2);
    cTimeDist[0]->cd(4);  hTimeStampFlag->Draw(""); gPad->SetLogy(1);

    // *********************************************************
    //       Histograms to monitor event reconstruction
    // *********************************************************

    std::cout << "     InitAnalysis(): initialize histograms for event clustering"<<std::endl;

    cEvtE1 = new TCanvas("cEvtE1","cEvt ImplantEnergyPlots", 100,100,1200,1200); cEvtE1->Divide(common::N_DSSD,6);
    cEvtXY = new TCanvas("cEvtXY","cEvt ImplantXY", 140,140,900,1000); cEvtXY->Divide(common::N_DSSD,3);
    cEvtdXdY = new TCanvas("cEvtdXdY","cEvt Implant_dX:dY", 140,140,1200,800); cEvtdXdY->Divide(common::N_DSSD,4);
    cEvtMulti = new TCanvas("cEvtMulti","cEvt Multiplicity", 140,140,1200,1000); cEvtMulti->Divide(common::N_DSSD,3);
    //    cEvtE2= new TCanvas("cEvtE2","cEvt Energy2", 120,120,1200,800); cEvtE2->Divide(3,2);
    //    cEvtHits= new TCanvas("cEvtHits","cEvt Hit Patterns", 150,150,1200,800); cEvtHits->Divide(2,2);
    //    cEvtTS=  new TCanvas("cEvtTS","cEvt Time Stamp", 160,160,1300,600); cEvtTS->Divide(2,2);

    for(int i=0;i<common::N_DSSD;i++) {

      //Energy distribution of n-side implant events
      sprintf(hname,"hEvt_Eside%i_0",i+1);
      sprintf(htitle,"Energy of n-side implant events (DSSD%i);Energy [ch]",i+1);
      hEvt_Eside[i][0] = new TH1I(hname, htitle, 512, 0, 32768);
      cEvtE1->cd(i+1); hEvt_Eside[i][0]->Draw(""); gPad->SetLogy(1);
      
      //Energy distribution of p-side implant events
      sprintf(hname,"hEvt_Eside%i_1",i+1);
      sprintf(htitle,"Energy (DSSD%i p-side);Energy [ch]",i+1);
      hEvt_Eside[i][1] = new TH1I(hname, htitle, 512, 0, 32768);
      cEvtE1->cd(i+common::N_DSSD+1); hEvt_Eside[i][1]->Draw(""); gPad->SetLogy(1);
      
      //E_x vs E_y for implant events
      sprintf(hname,"hEvt_ExEy%i",i+1);
      sprintf(htitle,"Energy N vs P side (DSSD%i);Energy n-side [ch];Energy p-side [ch]",i+1);
      hEvt_ExEy[i] = new TH2I(hname, htitle, 256, 0, 16384, 256, 0, 16384);
      cEvtE1->cd(i+(2*common::N_DSSD)+1); hEvt_ExEy[i]->Draw("colz"); gPad->SetLogz(0);
      
      //x-strip distribution of implant events
      sprintf(hname,"hEvt_X_DSSD%i",i+1);
      sprintf(htitle,"x-strip distribution of implant events in DSSD%i;X [ch]",i+1);
      hEvt_X[i] = new TH1I(hname, htitle, 128, 0, 128);
      cEvtXY->cd(i+1); hEvt_X[i]->Draw(""); gPad->SetLogy(1);
      
      //y-strip distribution of implant events
      sprintf(hname,"hEvt_Y_DSSD%i",i+1);
      sprintf(htitle,"y-strip distribution of implant events in DSSD%i;Y [ch]",i+1);
      hEvt_Y[i] = new TH1I(hname, htitle, 128, 0, 128);
      cEvtXY->cd(i+common::N_DSSD+1); hEvt_Y[i]->Draw(""); gPad->SetLogy(1);
      
      //x-y plot of implant event distribution
      sprintf(hname,"hEvt_XY_DSSD%i",i+1);
      sprintf(htitle,"x-y plot of implant event distribution in DSSD%i;X [ch];Y [ch]",i+1);
      hEvt_XY[i] = new TH2I(hname, htitle, 32, 0, 128, 32, 0, 128);
      cEvtXY->cd(i+(2*common::N_DSSD)+1); hEvt_XY[i]->Draw("colz"); gPad->SetLogz(0);
      
      if(i<common::N_DSSD-1) {
	
	//Change in x/y-position between planes
	sprintf(hname,"hEvt_dX_%i-%i",i+2,i+1);
	sprintf(htitle,"dX of fast ions between DSSD%i and DSSD%i;X%i - X%i [ch]",i+2,i+1,i+2,i+1);
	hEvt_dX[i] = new TH1I(hname, htitle, 64, -128, 128);
	cEvtdXdY->cd(i+1); hEvt_dX[i]->Draw(""); gPad->SetLogy(1);
	
	sprintf(hname,"hEvt_dY_%i-%i",i+2,i+1);
	sprintf(htitle,"dY of fast ions between DSSD%i and DSSD%i;Y%i - Y%i [ch]",i+2,i+1,i+2,i+1);
	hEvt_dY[i] = new TH1I(hname, htitle, 64, -128, 128);
	cEvtdXdY->cd(i+1+common::N_DSSD); hEvt_dY[i]->Draw(""); gPad->SetLogy(1);
      }
      
      else if(i==common::N_DSSD-1) {
	sprintf(hname,"hEvt_dX_FirstLast");
	sprintf(htitle,"Fast ion dX between the first and last DSSDs;X_first - X_last [ch]");
	hEvt_dX[i] = new TH1I(hname, htitle, 64, -128, 128);
	cEvtdXdY->cd(i+1); hEvt_dX[i]->Draw(""); gPad->SetLogy(1);

	sprintf(hname,"hEvt_dY_FirstLast");
	sprintf(htitle,"Fast ion dY between the first and last DSSDs;Y_first - Y_last [ch]");
	hEvt_dY[i] = new TH1I(hname, htitle, 64, -128, 128);
	cEvtdXdY->cd(i+1+common::N_DSSD); hEvt_dX[i]->Draw(""); gPad->SetLogy(1);
      }
      
      if(i<common::N_DSSD-2) {

	//Change in x/y-position between DSSDi+1/DSSDi vs change between DSSDi+2/DSSDi+1
	sprintf(hname,"hEvt_dXdX_%i-%i-%i",i+3,i+2,i+1);
	sprintf(htitle,"Fast ion dX between DSSD%i/DSSD%i vs DSSD%i/DSSD%i;X%i - X%i [ch];X%i - X%i [ch]",i+2,i+1,i+3,i+2,i+2,i+1,i+3,i+2);
	hEvt_dXdX[i] = new TH2I(hname, htitle, 32, -128, 128, 32, -128, 128);
	cEvtdXdY->cd(i+1+(2*common::N_DSSD)); hEvt_dXdX[i]->Draw("colz"); gPad->SetLogz(1);

	sprintf(hname,"hEvt_dXdX_%i-%i-%i",i+3,i+2,i+1);
	sprintf(htitle,"Fast ion dY between DSSD%i/DSSD%i vs DSSD%i/DSSD%i;X%i - X%i [ch];X%i - X%i [ch]",i+2,i+1,i+3,i+2,i+2,i+1,i+3,i+2);
	hEvt_dYdY[i] = new TH2I(hname, htitle, 32, -128, 128, 32, -128, 128);
	cEvtdXdY->cd(i+1+(3*common::N_DSSD)); hEvt_dYdY[i]->Draw("colz"); gPad->SetLogz(1);
      }

      else if(i==common::N_DSSD-2) {
	
	//Change in x-y position between first/middle/last planes
	sprintf(hname,"hEvt_dXdX_%i-%i-%i",common::N_DSSD,(common::N_DSSD/2)+1,1);
	sprintf(htitle,"Fast ion dX between DSSD%i/DSSD%i vs DSSD%i/DSSD%i;X%i - X%i [ch];X%i - X%i [ch]",
		(common::N_DSSD/2)+1,1,common::N_DSSD,(common::N_DSSD/2)+1,(common::N_DSSD/2)+1,1,common::N_DSSD,(common::N_DSSD/2)+1);
	hEvt_dXdX[i] = new TH2I(hname, htitle, 32, -128, 128, 32, -128, 128);
	cEvtdXdY->cd(i+1+(2*common::N_DSSD)); hEvt_dXdX[i]->Draw("colz"); gPad->SetLogz(1);
	
	sprintf(hname,"hEvt_dYdY_%i-%i-%i",common::N_DSSD,(common::N_DSSD/2)+1,1);
	sprintf(htitle,"Fast ion dY between DSSD%i/DSSD%i vs DSSD%i/DSSD%i;X%i - X%i [ch];X%i - X%i [ch]",
		(common::N_DSSD/2)+1,1,common::N_DSSD,(common::N_DSSD/2)+1,(common::N_DSSD/2)+1,1,common::N_DSSD,(common::N_DSSD/2)+1);
	hEvt_dYdY[i] = new TH2I(hname, htitle, 32, -128, 128, 32, -128, 128);
	cEvtdXdY->cd(i+1+(3*common::N_DSSD)); hEvt_dYdY[i]->Draw("colz"); gPad->SetLogz(1);
	
      }
    
      //Strip multiplicity of n- and p- side implant events
      sprintf(hname,"hEvt_Multi_DSSD%i_0",i+1);
      sprintf(htitle,"Multiplicity of n-side implant events (DSSD%i);N hits (n-side)",i+1);
      hEvt_Multi[i][0] = new TH1I(hname, htitle, 15, 0, 15);
      cEvtMulti->cd(i+1); hEvt_Multi[i][0]->Draw(""); gPad->SetLogy(1);
      
      sprintf(hname,"hEvt_Multi_DSSD%i_1",i+1);
      sprintf(htitle,"Multiplicity of p-side events (DSSD%i);N hits (p-side)",i+1);
      hEvt_Multi[i][1] = new TH1I(hname, htitle, 15, 0, 15);
      cEvtMulti->cd(i+1+common::N_DSSD); hEvt_Multi[i][1]->Draw(""); gPad->SetLogy(1);
      
    } //End loop over common::N_DSSD
    
    //Tally of implant events per DSSD side
    hEvt_HitsSide = new TH1I("hEvt_HitsSide","Detector implant events (each side);DSSD# + side(n=0, p=1)*0.5;n_hits",2*common::N_DSSD,1,common::N_DSSD+1);
    cEvtMulti->cd((2*common::N_DSSD)+1); hEvt_HitsSide->Draw(""); gPad->SetLogy(1);
    
    //Tally of implant per DSSD
    hEvt_HitsDet = new TH1I("hEvt_HitsDet","Detector implant events; DSSD#; n_hits",common::N_DSSD,1,common::N_DSSD+1);
    cEvtMulti->cd((2*common::N_DSSD)+2); hEvt_HitsDet->Draw(""); gPad->SetLogy(1);

    //E_x vs E_y for pulser events
    hEvt_EPulser_d = new TH2I("hEvt_EPulser_E","Pulser spectra;<E n-side> [ch];<E p-side> [ch]",100,0,32000, 100, 0, 32000); 
    
    //hEvt_HitsDet_gE= new TH1I("hEvt_HitsDet_gE","Detector Hits (IC->Det3) !E cut;N hits (Det)",common::N_DSSD,1,common::N_DSSD+1);
    //cEvtMulti->cd(11); hEvt_HitsDet_gE->Draw(""); gPad->SetLogy(1);
    
    //hEvt_HitsDet_gX= new TH1I("hEvt_HitsDet_gX","Detector Hits (IC->Det3) !X cut;N hits (Det)",common::N_DSSD,0,common::N_DSSD+1);
    //cEvtMulti->cd(12); hEvt_HitsDet_gX->Draw(""); gPad->SetLogy(1);
    
    hEvt_TmStpDist[0] = new TH1I("hEvt_TmStpDist0","Time Stamp Dist (within event);ts - ts_{0} [arb. units]",500,-2500,4500); 
    cTimeDist[0]->cd(5);
    //cEvtTS->cd(1); 
    hEvt_TmStpDist[0]->Draw(); gPad->SetLogy(1);

    hEvt_TmStpDist[1] = new TH1I("hEvt_TmStpDist1","Time Stamp Dist (last hit);#Delta ts [arb. units]",500,-2500,4500); 
    cTimeDist[0]->cd(6);
    //    cEvtTS->cd(2); 
    hEvt_TmStpDist[1]->Draw(); gPad->SetLogy(1);

    hEvt_TmStpDist[2] = new TH1I("hEvt_TmStpDist2","Time Stamp Dist (within event !DISC);#Delta ts [arb. units]",500,-2500,4500); 
    cTimeDist[0]->cd(7);
    //    cEvtTS->cd(3); 
    hEvt_TmStpDist[2]->Draw(); gPad->SetLogy(1);

    hEvt_TmStpDist[3] = new TH1I("hEvt_TmStpDist3","Time Stamp Dist (within event !ADC);#Delta ts [arb. units]",500,-2500,4500); 
    cTimeDist[0]->cd(8);
    //    cEvtTS->cd(4); 
    hEvt_TmStpDist[3]->Draw(); gPad->SetLogy(1);

    //Create canvases for decay event plots
    cEvtE_d = new TCanvas("cEvtE_d","cEvt Energy1 (DECAY)", 100,100,1200,1000); cEvtE_d->Divide(common::N_DSSD,5);
    cEvtXY_d = new TCanvas("cEvtXY_d","cEvt DecayXY", 140,140,1200,900); cEvtXY_d->Divide(common::N_DSSD,3);
    cEvtXY2_d = new TCanvas("cEvtXY2_d","Multiplicity vs Cluster size (DECAY)", 140,140,1000,800); cEvtXY2_d->Divide(common::N_DSSD,2);
    cEvtMulti_d = new TCanvas("cEvtMulti_d","cEvt Multiplicity (DECAY)", 140,140,1200,800); cEvtMulti_d->Divide(common::N_DSSD,3);
    //  cEvtHits_d= new TCanvas("cEvtHits_d","cEvt Hit Patterns (DECAY)", 150,150,1200,800); cEvtHits_d->Divide(2,2);

    //cEvtE_d->cd(3);  hEvt_EPulser_d->Draw("colz"); gPad->SetLogz(0);

    for(int i=0;i<common::N_DSSD;i++){

      // ************************************************
      //               Decay event historgams
      // ************************************************

      // Energy distribution of decay events
      sprintf(hname,"hEvt_Eside_d%i",i+1);
      sprintf(htitle,"Energy distribution of n-side decay events (DSSD%i);E [***]",i+1);
      hEvt_Eside_d[i][0] = new TH1I(hname, htitle, 512, 0, 32768);

      sprintf(hname,"hEvt_Eside_d%i",i+1);
      sprintf(htitle,"Energy distribution of p-side decay events (DSSD%i);E [***]",i+1);
      hEvt_Eside_d[i][1] = new TH1I(hname, htitle, 512, 0, 32768);

      cEvtE_d->cd(i+1);  hEvt_Eside_d[i][0]->Draw(); gPad->SetLogy(1);
      cEvtE_d->cd(i+1+common::N_DSSD);  hEvt_Eside_d[i][1]->Draw(); gPad->SetLogy(1);

      //E_x vs E_y for decay events
      sprintf(hname,"hEvt_ExEy_d%i",i+1);
      sprintf(htitle,"Ex vs Ey for decay events (DSSD%i);Ex [***];Ey [***]",i+1);
      hEvt_ExEy_d[i]= new TH2I(hname, htitle, 256, 0, 16384, 256, 0, 16384);
      cEvtE_d->cd(i+1+(2*common::N_DSSD));  hEvt_ExEy_d[i]->Draw("colz"); gPad->SetLogz(1);
      
      //x-y distribution of decay evnets
      sprintf(hname,"hEvt_XY_d%i",i+1);
      sprintf(htitle,"XY distribution of decay events (DSSD%i);x;y",i+1);
      hEvt_XY_d[i]= new TH2I(hname, htitle, 128, 0, 128, 128, 0, 128);
      cEvtXY_d->cd(i+1);  hEvt_XY_d[i]->Draw("colz"); gPad->SetLogz(1);

      //Multiplicity of decay events by n- or p-side
      sprintf(hname,"hEvt_MultiStrip_d%i_0",i+1);
      sprintf(htitle,"Strip multiplicity of n-side decay events (DSSD%i);N strips",i+1);
      hEvt_MultiStrip_d[i][0] = new TH1I(hname,htitle,40,0,40);
      cEvtMulti_d->cd(i+1+common::N_DSSD);  hEvt_MultiStrip_d[i][0]->Draw(""); gPad->SetLogy(1);

      sprintf(hname,"hEvt_MultiStrip_d%i_1",i+1);
      sprintf(htitle,"Strip multiplicity of p-side decay events (DSSD%i);N strips",i+1);
      hEvt_MultiStrip_d[i][1] = new TH1I(hname,htitle,40,0,40);
      cEvtMulti_d->cd(i+1+(2*common::N_DSSD));  hEvt_MultiStrip_d[i][1]->Draw(""); gPad->SetLogy(1);

      //Energy spectrum of decay events
      sprintf(hname,"hEvt_Eside_df%i",i+1);
      sprintf(htitle,"Energy spectrum of n-side decay events (DSSD%i);E [***]",i+1);
      hEvt_Eside_df[i][0]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_df[i][0]->SetLineColor(kRed);

      sprintf(hname,"hEvt_Eside_df%i",i+1);
      sprintf(htitle,"Energy spectrum of p-side decay events (DSSD%i);E [***]",i+1);
      hEvt_Eside_df[i][1]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_df[i][1]->SetLineColor(kRed);

      //Energy spectrum of decay events by side
      sprintf(hname,"hEvt_Eside_df2%i",i+1);
      sprintf(htitle,"E(decay) Det%i n-side !decay2;E [***]",i+1);
      hEvt_Eside_df2[i][0]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_df2[i][0]->SetLineColor(kGreen);

      sprintf(hname,"hEvt_Eside_df2%i",i+1);
      sprintf(htitle,"E(decay) Det%i p-side !decay2;E [***]",i+1);
      hEvt_Eside_df2[i][1]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_df2[i][1]->SetLineColor(kGreen);

      //E_x vs E_y for decay, w/ extra conditions
      sprintf(hname,"hEvt_ExEy_df%i",i);
      sprintf(htitle,"Ex vs Ey for decay events (DSSD%i, decay_flag>0 && decay_flag%%10==%i);Ex [***];Ey [***]",i+1,i+1);
      hEvt_ExEy_df[i]= new TH2I(hname, htitle, 256, 0, 16384, 256, 0, 16384);

      sprintf(hname,"hEvt_ExEy_df2%i",i);
      sprintf(htitle,"Ex vs Ey for decay events (DSSD%i, decay_flag>10 && decay_flag%%10==%i);Ex [***];Ey [***]",i+1,i+1);
      hEvt_ExEy_df2[i]= new TH2I(hname, htitle, 256, 0, 16384, 256, 0, 16384);

      cEvtE_d->cd(i+1);                 hEvt_Eside_df[i][0]->SetLineColor(kRed); hEvt_Eside_df[i][0]->Draw("same");// gPad->SetLogy(1);
      cEvtE_d->cd(i+1+common::N_DSSD);  hEvt_Eside_df[i][1]->SetLineColor(kRed); hEvt_Eside_df[i][1]->Draw("same");// gPad->SetLogy(1);

      cEvtE_d->cd(i+1);                 hEvt_Eside_df2[i][0]->SetLineColor(kGreen); hEvt_Eside_df2[i][0]->Draw("same");// gPad->SetLogy(1);
      cEvtE_d->cd(i+1+common::N_DSSD);  hEvt_Eside_df2[i][1]->SetLineColor(kGreen); hEvt_Eside_df2[i][1]->Draw("same");// gPad->SetLogy(1);

      cEvtE_d->cd(i+1+(3*common::N_DSSD));   hEvt_ExEy_df[i]->Draw("colz"); gPad->SetLogz(1);
      cEvtE_d->cd(i+1+(4*common::N_DSSD));   hEvt_ExEy_df2[i]->Draw("colz"); gPad->SetLogz(1);
	
      //XY plots of decay events with conditions
      sprintf(hname,"hEvt_XY_df%i",i);
      sprintf(htitle,"X vs Y (decay) Det%i !decay;Ex [***];Ey [***]",i);
      hEvt_XY_df[i]= new TH2I(hname, htitle, 128, 0, 128, 128, 0, 128);

      sprintf(hname,"hEvt_XY_df2%i",i);
      sprintf(htitle,"X vs Y (decay) Det%i !decay2;Ex [***];Ey [***]",i);
      hEvt_XY_df2[i]= new TH2I(hname, htitle, 128, 0, 128, 128, 0, 128);

      cEvtXY_d->cd(i+1+common::N_DSSD);      hEvt_XY_df[i]->Draw("colz"); gPad->SetLogz(0);
      cEvtXY_d->cd(i+1+(2*common::N_DSSD));  hEvt_XY_df2[i]->Draw("colz"); gPad->SetLogz(0);
      
      //Strip multiplicty vs strip_max - strip_min by side (decay events)
      sprintf(hname,"hEvt_MultidX_d%i_0",i+1);
      sprintf(htitle,"Multiplicity vs strip_max-strip_min for n-side decays (DSSD%i);hit multiplicity;x_max - x_min [ch]",i+1);
      hEvt_MultidX_d[i][0]= new TH2I(hname, htitle, 32, 0, 32, 64, 0, 64);
      cEvtXY2_d->cd(i+1);  hEvt_MultidX_d[i][0]->Draw("colz"); gPad->SetLogz(1);

      sprintf(hname,"hEvt_MultidX_d%i_1",i);
      sprintf(htitle,"Multiplicity vs strip_max-strip_min for p-side decays (DSSD%i), p-side;hit multiplicity;x_max - x_min [ch]",i);
      hEvt_MultidX_d[i][1]= new TH2I(hname, htitle, 32, 0, 32, 64, 0, 64);
      cEvtXY2_d->cd(i+1+common::N_DSSD);  hEvt_MultidX_d[i][1]->Draw("colz"); gPad->SetLogz(1);

      // ******************************************************
      //                Implant event histograms
      // ******************************************************
      
      //Energy spectrum of implant events by side
      sprintf(hname,"hEvt_Eside_if%i_0",i+1);
      sprintf(htitle,"Energy spectrum of n-side implant events (DSSD%i);Energy [ch]",i+1);
      hEvt_Eside_if[i][0]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_if[i][0]->SetLineColor(kRed);
      cEvtE1->cd(i+1); hEvt_Eside_if[i][0]->Draw("same"); gPad->SetLogy(1);
      
      sprintf(hname,"hEvt_Eside_if%i_1",i+1);
      sprintf(htitle,"Energy spectrum of p-side implant events (DSSD%i);Energy [ch]",i+1);
      hEvt_Eside_if[i][1]= new TH1I(hname, htitle, 512, 0, 16384);
      hEvt_Eside_if[i][1]->SetLineColor(kRed);
      cEvtE1->cd(i+5); hEvt_Eside_if[i][1]->Draw("same"); gPad->SetLogy(1);
      
      //E_x vs E_y for implant events
      sprintf(hname,"hEvt_ExEy_if%i",i+1);
      sprintf(htitle,"E_x vs E_y for implant events (DSSD%i);Energy n-side [ch];Energy p-side [ch]",i+1);
      hEvt_ExEy_if[i]= new TH2I(hname, htitle, 256, 0, 16384, 256, 0, 16384);
      cEvtE1->cd(i+13); hEvt_ExEy_if[i]->Draw("colz"); gPad->SetLogz(0);

      //XY plot of implant events
      sprintf(hname,"hEvt_XY_if%i",i+1);
      sprintf(htitle,"X vs Y distribution of implant events (DSSD%i);Ex [***];Ey [***]",i+1);
      hEvt_XY_if[i]= new TH2I(hname, htitle, 128, 0, 128, 128, 0, 128);

      //Composite ADC spectrum combinining all calibrated strips
      sprintf(hname,"hADClow_all_DSSD%i",i);
      sprintf(htitle,"Composite ADC spectrum for whole DSSD%i;Channel;Counts",i);
      hADClow_all[i]= new TH1I(hname, htitle, 4096, 0, 65536);

    }

    //Number of decay events by detector and side
    hEvt_MultiDet_d = new TH1I("hEvt_MultiDet_d","Tally of decay events per DSSD;DSSD#;Events#",common::N_DSSD,1,common::N_DSSD+1);
    hEvt_MultiSide_d = new TH2I("hEvt_MultiSide_d","Tally of decay events by DSSD/side;Side (0 = n, 1 = p, 2 = both);DSSD#",3,0,3,common::N_DSSD,1,common::N_DSSD+1);

    //Multiplicity of implant events vs decay events
    hEvt_MultiID= new TH2I("hEvt_MultiID","Multiplicity of Implant vs Decay events;N implant hits;N decay hits",40,0,40,40,0,40);

    //Implant and decay flags
    hEvt_HitsFlag= new TH1I("hEvt_HitsFlag","Number of Implant & Decay Flags;Flag (0: implant, 1: decay)",2,0,2);

    cEvtMulti_d->cd(1);  hEvt_MultiID->Draw("colz"); gPad->SetLogz(1);
    cEvtMulti_d->cd(2);  hEvt_MultiDet_d->Draw(); gPad->SetLogy(1);
    cEvtMulti_d->cd(3);  hEvt_MultiSide_d->Draw("colz"); gPad->SetLogz(1);
    cEvtMulti_d->cd(4);  hEvt_HitsFlag->Draw(""); gPad->SetLogy(1);
  
    if(b_debug) std::cout << "db    Analysis.cpp: Initialized histograms and canvas for this step"<<std::endl;
    
  }

  //if second bit of 'opt' is 1, enable TTree for output
  if( ( (opt & 0x02) >> 1) == 1){
  
    std::cout << " ***     Analysis::InitAnalysis(): initializing TTree" << std::endl;
    //Initialize TTree
    out_root_tree = new TTree("AIDA_hits","AIDA_hits");
    out_root_tree->Branch("aida_hit",&hit,"t/L:t_ext/L:x/I:y/I:z/I:ex/I:ey/I:flag/I");
    SetBRootTree(true);
  }

  //  std::cout << "  Analysis::InitHistograms(): finished with initialization " << std::endl;

}


void Analysis::ResetEvent(){

  b_pulser= false;

  evt_data.multiplicity= 0;
  for(int i=0;i<common::N_DSSD;i++){

    e_sum_d[i][0]=0;
    e_sum_d[i][1]=0;

    strip_max_d[i][0]=0;
    strip_max_d[i][1]=0;
    strip_min_d[i][0]=128;
    strip_min_d[i][1]=128;

    evt_data.n_det_i[i]=0;  
    evt_data.n_side_i[i][0]=0;  
    evt_data.n_side_i[i][1]=0;  
    evt_data.x_i[i]=-999;  
    evt_data.y_i[i]=-999;  
    evt_data.e_i[i][0]=-99999;  
    evt_data.e_i[i][1]=-99999;  

    evt_data.n_det_d[i]=0;  
    evt_data.n_side_d[i][0]=0;  
    evt_data.n_side_d[i][1]=0;  
    evt_data.x_d[i]=-999;  
    evt_data.y_d[i]=-999;  
    evt_data.e_d[i][0]=-99999;  
    evt_data.e_d[i][1]=-99999;  

    evt_data.dx_d[i]=-1;  
    evt_data.dy_d[i]=-1;  

    evt_data.t0= -99999;
    evt_data.t0_ext= -99999;
    evt_data.dt= 0; 

  }

  evt_data.decay_flag= 0;
  evt_data.implant_flag= 0;

  hit.t= -1;
  hit.t_ext= -1;
  hit.x= -1;
  hit.y= -1;
  hit.z= -1;
  hit.ex= -1;
  hit.ey= -1;
  hit.flag= -1;
  
}

/*! \fn
 *
 *
 *
 */
void Analysis::WriteHistograms(){
    
  hTimeStamp->Write();
  hTimeStampExt->Write();
  hTimeStampFlag->Write();
  
  for(int i=0;i<common::N_FEE64;i++){
    if(b_mod_enabled[i]){
      hADClowCh[i]->Write();
      hADCdiscCh[i]->Write();
      hADChighCh[i]->Write();
      hCh_ADClow[i]->Write();
      hCh_ADChigh[i]->Write();
      hCh_ADCdisc[i]->Write();
      hADClow_all[i]->Write();
      
      hElow[i]->Write();
      hEhigh[i]->Write();
      hEdisc[i]->Write();
    } 
  }
  
  for(int i=0; i<common::N_DSSD;i++){
    
    hEvt_TmStpDist[i]->Write();
    
    hEvt_ExEy[i]->Write();
    hEvt_X[i]->Write();
    hEvt_Y[i]->Write();
    hEvt_XY[i]->Write();
 
    hEvt_XY_d[i]->Write();   
    hEvt_ExEy_d[i]->Write();
    //      hEvt_X_d[i]->Write();
    //      hEvt_Y_d[i]->Write();
    hEvt_ExEy_df[i]->Write();
    hEvt_XY_df[i]->Write();
    hEvt_ExEy_df2[i]->Write();
    hEvt_XY_df2[i]->Write();
    
    hEvt_ExEy_if[i]->Write();
    hEvt_XY_if[i]->Write();

    hEvt_dX[i]->Write(); // 1:2, 1:3, 2:3
    hEvt_dY[i]->Write();
    hEvt_dXdX[i]->Write();
    hEvt_dYdY[i]->Write();
    
    hTimeADClow[i]->Write();
    hTimeADCdisc[i]->Write();
    hTimeADChigh[i]->Write();

    hADClow_all[i]->Write();

    for(int j=0;j<2;j++){
      
      hEvt_Eside[i][j]->Write();
      
      hEvt_Eside_d[i][j]->Write();
      hEvt_Eside_df[i][j]->Write();
      hEvt_Eside_df2[i][j]->Write();
      
      hEvt_Eside_if[i][j]->Write();
      
      hEvt_Multi[i][j]->Write();
      hEvt_MultidX_d[i][j]->Write();
      
    }
  }

  hEvt_EdE->Write();
  
  hEvt_Eaida->Write();
  //hEvt_Eaida_gE->Write();
  //hEvt_Eaida_gX->Write();
    
  hEvt_HitsSide->Write();
  hEvt_HitsDet->Write();
  //hEvt_HitsDet_gE->Write();
  //hEvt_HitsDet_gX->Write();

  hEvt_MultiID->Write();
  hEvt_HitsFlag->Write();

  hEvt_EPulser_d->Write();
  
  //Write canvases
  for(int i=0; i<2; i++) {
    cADClow[i]->Write();
    cADChigh[i]->Write();
    cADCdisc[i]->Write();
    cEall[i]->Write();
  }

  for(int i=0;i<common::N_DSSD;i++) {
    cTimeDist[i]->Write();
  }

  cEvtE1->Write(); 
  //  cEvtE2->Write(); 
  cEvtXY->Write();
  cEvtdXdY->Write();
  cEvtMulti->Write();
  //  cEvtHits->Write();
  //  cEvtTS->Write();
  cEvtE_d->Write();
  cEvtXY_d->Write();
  cEvtXY2_d->Write();
  cEvtMulti_d->Write();

  std::cout << "\n done writing Analysis histograms to file..." << std::endl;

}




void Analysis::ResetHistograms(){

  if(GetBHistograms()){

    for(int i=0;i<common::N_FEE64;i++){
      if(b_mod_enabled[i]){
	hADClowCh[i]->Reset();
	hADCdiscCh[i]->Reset();
	hADChighCh[i]->Reset();
	hCh_ADClow[i]->Reset();
	hCh_ADChigh[i]->Reset();
	hCh_ADCdisc[i]->Reset();
	
	hElow[i]->Reset();
	hEhigh[i]->Reset();
	hEdisc[i]->Reset();
	
      }
    }
    
    hTimeADClow[0]->Reset();
    hTimeADCdisc[0]->Reset();
    hTimeADChigh[0]->Reset();
    
    hTimeStamp->Reset();
    hTimeStampExt->Reset();
    hTimeStampFlag->Reset();
        
    for(int i=0; i<common::N_DSSD;i++){

      hEvt_ExEy[i]->Reset();
      hEvt_X[i]->Reset();
      hEvt_Y[i]->Reset(); 
      hEvt_XY[i]->Reset();     
      hEvt_dX[i]->Reset();
      hEvt_dY[i]->Reset();      
      hEvt_dXdX[i]->Reset();
      hEvt_dYdY[i]->Reset();

      hEvt_ExEy_if[i]->Reset();
      hEvt_XY_if[i]->Reset();

      hEvt_ExEy_d[i]->Reset();
      hEvt_ExEy_df[i]->Reset();
      hEvt_ExEy_df2[i]->Reset();
      hEvt_XY_df[i]->Reset();
      hEvt_XY_df2[i]->Reset();
      hEvt_XY_d[i]->Reset();

      hADClow_all[i]->Reset();
      
      for(int j=0;j<2;j++){
	hEvt_Eside[i][j]->Reset();
	hEvt_Eside_if[i][j]->Reset();
	hEvt_Eside_df[i][j]->Reset();
	hEvt_Eside_df2[i][j]->Reset();
	hEvt_Multi[i][j]->Reset();
	hEvt_Eside_d[i][j]->Reset();
	hEvt_MultidX_d[i][j]->Reset();
      }
    }
    
    hEvt_TmStpDist[1]->Reset();
    hEvt_TmStpDist[0]->Reset();
    hEvt_TmStpDist[2]->Reset();
    hEvt_TmStpDist[3]->Reset();
    
    hEvt_EdE->Reset();
    
    hEvt_Eaida->Reset();
    //hEvt_Eaida_gE->Reset();
    //hEvt_Eaida_gX->Reset();
    
    hEvt_HitsSide->Reset();
    hEvt_HitsDet->Reset();
    //hEvt_HitsDet_gE->Reset();
    //hEvt_HitsDet_gX->Reset();
    
    hEvt_MultiID->Reset();
    hEvt_HitsFlag->Reset();
    
    hEvt_EPulser_d->Reset();


    std::cout << "        Analysis::ResetHistograms(): all histograms have been reset..." << std::endl;
  }

}

bool Analysis::IsChEnabled(Calibrator & my_cal_data){

  if(b_mod_enabled[my_cal_data.GetModule()]){

    //can also skip individual channels here...

    if(my_cal_data.GetModule()==30){
      //NNAIDA#11:1.10      if( my_cal_data.GetChannel() == 10) return true;
      if( my_cal_data.GetChannel() == 24) return true; //NNAIAD#11:2.8
      else return false;
    }

    return true;
  }

  else return false;
}

bool Analysis::SetEventTimeWindow(double value){

  if(value>0){
    event_time_window= value;
    return true;
  }
  else{
    event_time_window =0;
    return false;
  }
}


void Analysis::PrintEvent(){

  printf("\n  *EVT*   Multiplicity : ");
  printf("\n  *EVT*        N total= %i",evt_data.multiplicity);
  printf("\n  *EVT*i       N det0=  %i  (%i, %i)",evt_data.n_det_i[0], evt_data.n_side_i[0][0], evt_data.n_side_i[0][1]);
  printf("\n  *EVT*i       N det1=  %i  (%i, %i)",evt_data.n_det_i[1], evt_data.n_side_i[1][0], evt_data.n_side_i[1][1]);
  printf("\n  *EVT*i       N det2=  %i  (%i, %i)",evt_data.n_det_i[2], evt_data.n_side_i[2][0], evt_data.n_side_i[2][1]);
  printf("\n  *EVT*i       N det3=  %i  (%i, %i)",evt_data.n_det_i[3], evt_data.n_side_i[3][0], evt_data.n_side_i[3][1]);
  printf("\n  *EVT*i  Position     : ");
  printf("\n  *EVT*i       X det0=  %i   Y det0= %i",evt_data.x_i[0], evt_data.y_i[0]);
  printf("\n  *EVT*i       X det1=  %i   Y det1= %i",evt_data.x_i[1], evt_data.y_i[1]);
  printf("\n  *EVT*i       X det2=  %i   Y det2= %i",evt_data.x_i[2], evt_data.y_i[2]);
  printf("\n  *EVT*i       X det3=  %i   Y det3= %i",evt_data.x_i[3], evt_data.y_i[3]);
  printf("\n  *EVT*i  Energy       : ");
  printf("\n  *EVT*i       En det0=  %li   Ep det0= %li",evt_data.e_i[0][0], evt_data.e_i[0][1]);
  printf("\n  *EVT*i       En det1=  %li   Ep det1= %li",evt_data.e_i[1][0], evt_data.e_i[1][1]);
  printf("\n  *EVT*i       En det2=  %li   Ep det2= %li",evt_data.e_i[2][0], evt_data.e_i[2][1]);
  printf("\n  *EVT*i       En det3=  %li   Ep det3= %li",evt_data.e_i[3][0], evt_data.e_i[3][1]);

  printf("\n  *EVT*d       N det0=  %i  (%i, %i)",evt_data.n_det_d[0], evt_data.n_side_d[0][0], evt_data.n_side_d[0][1]);
  printf("\n  *EVT*d       N det1=  %i  (%i, %i)",evt_data.n_det_d[1], evt_data.n_side_d[1][0], evt_data.n_side_d[1][1]);
  printf("\n  *EVT*d       N det2=  %i  (%i, %i)",evt_data.n_det_d[2], evt_data.n_side_d[2][0], evt_data.n_side_d[2][1]);
  printf("\n  *EVT*d       N det3=  %i  (%i, %i)",evt_data.n_det_d[3], evt_data.n_side_d[3][0], evt_data.n_side_d[3][1]);
  printf("\n  *EVT*d  Position     : ");
  printf("\n  *EVT*d       X det0=  %i   Y det0= %i",evt_data.x_d[0], evt_data.y_d[0]);
  printf("\n  *EVT*d       X det1=  %i   Y det1= %i",evt_data.x_d[1], evt_data.y_d[1]);
  printf("\n  *EVT*d       X det2=  %i   Y det2= %i",evt_data.x_d[2], evt_data.y_d[2]);
  printf("\n  *EVT*d       X det3=  %i   Y det3= %i",evt_data.x_d[3], evt_data.y_d[3]);
  printf("\n  *EVT*d  Energy       : ");
  printf("\n  *EVT*d       En det0=  %li   Ep det0= %li",evt_data.e_d[0][0], evt_data.e_d[0][1]);
  printf("\n  *EVT*d       En det1=  %li   Ep det1= %li",evt_data.e_d[1][0], evt_data.e_d[1][1]);
  printf("\n  *EVT*d       En det2=  %li   Ep det2= %li",evt_data.e_d[2][0], evt_data.e_d[2][1]);
  printf("\n  *EVT*d       En det3=  %li   Ep det3= %li",evt_data.e_d[3][0], evt_data.e_d[3][1]);

  printf("\n  *EVT*   Time         : ");
  printf("\n  *EVT*        t0=  %li      dt=  %li",evt_data.t0, evt_data.dt);
  printf("\n  *EVT*    t0_ext=  %lX",evt_data.t0_ext);

  if(evt_data.decay_flag)  printf("\n  *EVT*   Type:  decay \n");
  else printf("\n  *EVT*   Type:  implant \n");

}


void Analysis::Close(){
  if(GetBHistograms()) WriteHistograms();

  if(GetBRootTree()) out_root_tree->Write(); 

}


void Analysis::SetBDebug(bool flag){
  b_debug= flag;
}

void Analysis::SetBHistograms(bool flag){
  b_histograms= flag;
}

/*void Analysis::SetBPushData(bool flag){
 *  b_push_data= flag;
 }*/

void Analysis::SetBRootTree(bool flag){
  b_root_tree= flag;
}

// ***********************************************************
//                       Getters...
// ***********************************************************

double Analysis::GetEventTimeWindow(){ return event_time_window; }
bool Analysis::GetBDebug(){ return b_debug; }
bool Analysis::GetBHistograms(){ return b_histograms; }
bool Analysis::GetBRootTree(){ return b_root_tree; }
int Analysis::GetMultiplicity(){ return evt_data.multiplicity; }
//bool Analysis::GetBPushData(){ return b_push_data; }

// ***********************************************************
//                         Methods
// ***********************************************************

Analysis::Analysis(){

  b_debug = false;
  b_histograms= false;
  b_root_tree= false;
  b_pulser= false;

  event_count= 0;
  t_low_prev= 0;
  t_high_prev= 0;
  t_disc_prev= 0;

  event_time_window = 2500;
  dE_i_lim= 2000;
  dX_i_lim= 15;
  E_i_min= 300;

  dE_d_lim= 3000;
  dX_d_lim= 5;
  E_d_min= 150;
  E_d_max= 3000;

  evt_data.multiplicity= 0;
  for(int i=0;i<common::N_DSSD;i++){
    //
    e_sum_d[i][0]=0;
    e_sum_d[i][1]=0;
    //


    evt_data.n_det_i[i]=0;  
    evt_data.n_side_i[i][0]=0;  
    evt_data.n_side_i[i][1]=0;  
    evt_data.x_i[i]=-999;  
    evt_data.y_i[i]=-999;  
    evt_data.e_i[i][0]=-99999;  
    evt_data.e_i[i][1]=-99999;  

    evt_data.n_det_d[i]=0;  
    evt_data.n_side_d[i][0]=0;  
    evt_data.n_side_d[i][1]=0;  
    evt_data.x_d[i]=-999;  
    evt_data.y_d[i]=-999;  
    evt_data.e_d[i][0]=-99999;  
    evt_data.e_d[i][1]=-99999;  

    evt_data.dx_d[i]=-1;  
    evt_data.dy_d[i]=-1;  


    evt_data.t0= -99999;
    evt_data.t0_ext= -99999;
    evt_data.dt= 0; 
  }
  evt_data.decay_flag= 0;
  evt_data.implant_flag= 0;

  hit.t= -1;
  hit.t_ext= -1;
  hit.x= -1;
  hit.y= -1;
  hit.z= -1;
  hit.ex= -1;
  hit.ey= -1;
  hit.flag= -1;


}

