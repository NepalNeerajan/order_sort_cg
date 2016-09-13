//http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Companion/cxx_crib/objects.html

// My code include.
#include "DataSource.h"
#include "Unpacker.h"
#include "Calibrator.h"
#include "Analysis.h"

// ROOT include.
#include <TApplication.h>
#include <TCanvas.h>
#include <TH1I.h>
#include <TROOT.h>
///gr #include <TGraph.h>

// C++ include.
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
//#include <vector>
//#include <sstream>

#include<signal.h>
#include"TSystem.h"
#include "kbhit.h"
////#include "MySignalHandler.cc"
#include <TSysEvtHandler.h>
//#include <TSignalHandler.h>
//#include <TSignalHandler.h>

void Usage(char *progname) {
    fprintf(stderr,"This will at some point become a useage instruction...");
    fprintf(stderr,"Usage\n%s \n\t-O (for online data analysis, source_option=2)\n",progname);
    fprintf(stderr,"\t-i [id=0] (DataSpy id for online data)\n");
    fprintf(stderr,"\t-F [RunName] (for data analysis from file, source_option=1)\n");
    fprintf(stderr,"\t-d [DataDir] (directory path for data file)\n");
    fprintf(stderr,"\t-x (enable data Xfer to remote DataSink, options in ./config directory)\n");
    fprintf(stderr,"\t-U [unpacker_option=0] (Unpacker output level; 0: no ouput, 1: histograms, 2: TTree, 3: histograms+TTree)\n");
    fprintf(stderr,"\t-C [calibrator_option=1] (Calibrator output level; 0: no ouput, 1: histograms, 2: TTree, 3: histograms+TTree)\n");
    fprintf(stderr,"\t-A [analysis_option=1] (Analysis output level; 0: no ouput, 1: histograms, 2: TTree, 3: histograms+TTree)\n");
    fprintf(stderr,"\t-w [time_window=3202] (time_window for event clustering)\n");
    fprintf(stderr,"\t-R (enable writing output to Root file)\n");
    fprintf(stderr,"\t-L [RLrun=-1, RLfirst, RLnum] (loop over list of MIDAS data files, overrides option -F if RLrun>0)\n");
    fprintf(stderr,"\t-v (verbose mode)\n");

    exit(1);
}

int main  (int argc, char **argv){
  
  int i,j;

  //  char RunNum[256]; //= NULL;
  std::string RunName;

  bool b_verbose=false;

  int time_window= 3202;
  //  bool b_run= false;

  int source_option=2; //1: file, 2: online

  int unpacker_option=0;

  int calibrator_option=1; //3: histograms+tree, 1: only histograms, 2: only tree

  int analysis_option=1; //3; //check implamantation of TTree bit!!!

  int id= 0;

  bool b_Xfer= false;

  std::string DataDir= "/scratch/cgriffin/NP1306/data/";//"/Disk/ds-sopa-group/np/RIKEN/May2015/"; 

  bool b_root_file= false;

  int RLrun= -1; int RLfirst= 0; int RLnum= 0;


  // Loop to read command line arguments
  if (argc >1) {

    std::cout << " command line arguments: " <<argc<< std::endl;

    for(i=1;i <argc;i++) {

      std::cout << argv[i] << std::endl;

      if ( (argv[i][0] == '-') || (argv[i][0] == '/') ) {
	
	switch( argv[i][1] ) {

	  //analyze online  data
        case 'O':  
	  source_option = 2;
	  break;

	  //analyze data from file
	case 'F': 
	  RunName=  argv[++i];
	  source_option = 1;
	  std::cout << "  F= " << RunName << std::endl;
	  break;

	case 'i':
	  id = atoi(argv[++i]);
	  break;

	case 'x':
	  b_Xfer = true;
	  break;

	case 'd':
	  DataDir=  argv[++i];
	  break;

	case 'U':
	  j = atoi(argv[++i]); std::cout << "  U= " << j << std::endl;
	  if(j>=0 && j<=3) unpacker_option= j;
	  else{
	    std::cout << " Invalid unpacker_option: -U " << j << std::endl;
	    Usage(argv[0]);
	  }
	  break;


	case 'C':
	  j = atoi(argv[++i]); std::cout << "  C= " << j << std::endl;
	  if(j>=0 && j<=3) calibrator_option= j;
	  else{
	    std::cout << " Invalid calibrator_option: -C " << j << std::endl;
	    Usage(argv[0]);
	  }
	  break;


	case 'A':
	  j = atoi(argv[++i]); std::cout << "  A= " << j << std::endl;
	  if(j>=0 && j<=3) analysis_option= j;
	  else{
	    std::cout << " Invalid analysis_option: -A " << j << std::endl;
	    Usage(argv[0]);
	  }
	  break;

	case 'w':
	  time_window = atoi(argv[++i]);
	  break;

	case 'R':
	  b_root_file = true;
	  break;

	case 'L':
	  RLrun = atoi(argv[++i]);
	  RLfirst = atoi(argv[++i]);
	  RLnum = atoi(argv[++i]);
	  source_option = 1;
	  break;


	case 'v':
	  b_verbose = true;
	  break;


	default:
	  Usage(argv[0]);
	  break;
	}
      }
    }
  }



  TApplication *rootapp= new TApplication("AIDAonline",&argc,argv);


  gSystem->AddSignalHandler( new TSignalHandler( kSigInterrupt ) );
  gSystem->AddSignalHandler( new TSignalHandler( kSigTermination ) );

  std::cout << "AIDA be like: in da F11!!" << std::endl;

  std::string FileNameData="";

  char *FileCalibParameters= (char*)"config/parameters_NP1306_R1300.txt";

  std::string FileNameRoot=""; //= "aida_sort_" + RunName +".root";
  TFile * fMain; //= new TFile(FileNameRoot.data(),"RECREATE");

  char RLname[64];

  //open Root output file (TTrees, histograms, etc...)
  if(b_root_file){
    if(source_option==2){
      FileNameRoot= "aida_sort_online.root";
    }
    else if(RLrun>0){
      sprintf(RLname,"R%d_list",RLrun);   //*********Change here for AIDA/R run prefixes******************//
      FileNameRoot= "aida_sort_"+ std::string(RLname) +".root";
    }
    else FileNameRoot= "aida_sort_" + RunName +".root";
    fMain= new TFile(FileNameRoot.data(),"RECREATE");
  }


  unsigned long n_entry=0;
  //  unsigned long n_write= 10000000;
  unsigned long n_update= 100000000;

  std::cout << " declaring my class objects"<<std::endl;
  DataSource midas_data;
  Unpacker unpacker_data;

  Calibrator calibrator_data;
  Analysis analysis_data;



  ///gr  double grX[10000]={0};
  ///gr  double grY[10000]={0};
  ///gr  int grN=0;
  ///gr  TGraph * grTS= new TGraph(1,grX,grY);
  ///gr  grTS->SetMarkerStyle(2);
  ///gr  grTS->SetTitle(";ts_{AIDA} [1/1e6];ts_{CORR. SCALER} [1/1e6]");
  ///gr  TCanvas * cTS= new TCanvas("cTS","cTS",130,130,800,600);
  ///gr  cTS->cd(); grTS->Draw("AP"); cTS->Update();

  std::cout << " initializing things"<<std::endl;

  unpacker_data.InitUnpacker(unpacker_option,FileCalibParameters);
  calibrator_data.InitCalibrator(calibrator_option,FileCalibParameters);
  analysis_data.InitAnalysis(analysis_option);

  analysis_data.SetEventTimeWindow(1.*time_window); 
  std::cout << " *** ANALYSIS: TIME WINDOW=  " << analysis_data.GetEventTimeWindow() << std::endl;


  if(b_verbose){
    std::cout << "------------------------------ DEBUG ENABLED------------------------"<<std::endl;
    unpacker_data.SetBDebug(true);
    calibrator_data.SetBDebug(true);
    analysis_data.SetBDebug(true);

  }
  else{
    unpacker_data.SetBDebug(false);
    calibrator_data.SetBDebug(false);
    analysis_data.SetBDebug(false);
  }



  std::cout<< " +++ Histograming:\n"
	   << "                   U: "<<unpacker_data.GetBHistograms() << "\n"    
	   << "                   C: "<<calibrator_data.GetBHistograms() << "\n"    
	   << "                   A: "<<analysis_data.GetBHistograms()  << "\n"<<std::endl;


  int Nloop=0;

  //loop over several input files;
  do{
    
    if(source_option==1){
      
      if(RLrun>0){ //if looping over a list of runs
	sprintf(RLname,"R%d_%d",RLrun,RLfirst+Nloop);    //*********Change here for AIDA/R run prefixes******************//
	FileNameData= DataDir + std::string(RLname);
      }
      else FileNameData= DataDir+RunName;

      std::cout << "\nSorting file "<<Nloop+1<<": "<< FileNameData << std::endl;
    }

    midas_data.InitDataSource(source_option, id, FileNameData, b_Xfer);
    if(b_verbose) midas_data.SetBDebug(true);
    else midas_data.SetBDebug(false);
    

    if(midas_data.GetBSourceOpen()){
      //    std::cout << "begin forever looop"<<std::endl;
      
      for(;;){
	
	//to pause loop and take a moment to reflect on the beauty of our planet and histograms 
	if(kbhit()==1){
	  int if_continue;
	  std::cout<<"online monitor is paused;"<<std::endl;
	  std::cout<<"press 0 <Enter> to stop, 1 <Enter> to restart, 2 <Enter> to clear histograms;"<<std::endl;
	  std::cin >> if_continue;
	  
	  char *choose;
	  if(if_continue==0){
	    choose=(char*)"stop";
	  }
	  else if(if_continue==1){
	    choose=(char*)"restart";
	  }
	  if(if_continue==2){
	    choose=(char*)"clear";
	  }
	  
	  std::cout<<"You chose "<<choose /*<<", please press ctrl+c to continue;"*/<<std::endl;
	  
	  if(if_continue == 0){
	    char my_ch;
	    std::cout<<"  0: exit AIDA sort (y/n)?"<<std::endl;
	    std::cin >> my_ch;
	    if(my_ch == 'y' || my_ch == 'Y'){
	      
	      //rootapp->Terminate();
	      break;
	    }
	    else if_continue=1;
	  }
	  if(if_continue==1){
	    unpacker_data.UpdateHistograms();
	    calibrator_data.UpdateHistograms();
	    analysis_data.UpdateHistograms();
	    std::cout<<"monitor restarted, press <Space> to pause;"<<std::endl;

  	    ///gr cTS->cd();  cTS->Clear();
  	    ///gr grTS->DrawGraph(grN,grX,grY,"AP");
  	    ///gr cTS->Update();
	  }
	  if(if_continue==2){
	    unpacker_data.ResetHistograms();
	    calibrator_data.ResetHistograms();
	    analysis_data.ResetHistograms();
	    std::cout<<"monitor reset, press <Space> to pause;"<<std::endl;
	  }
	}//end of statement controling keyboard halt of excecution: if(kbhit()==1)
	
	
	
	midas_data.Process();
	
	if(midas_data.GetBPushData()){
	  unpacker_data.Process(midas_data);
	  
	  if(unpacker_data.GetBPushData()){
	    calibrator_data.Process(unpacker_data);
	    
	    if(calibrator_data.GetBPushData()){
	      analysis_data.Process(midas_data, calibrator_data);
	    }

  	    ///gr if(grN<10000){
  	    ///gr   if(unpacker_data.GetInfoCode()==8 && unpacker_data.GetDataType()==2){
  	    ///gr    if(unpacker_data.GetFee64Id()==14){
  	    ///gr      grY[grN]= unpacker_data.GetCorrScaler()/1.e6;
  	    ///gr      grX[grN]= unpacker_data.GetTmStp()/1.e6; 
  	    ///gr      if( ((grN+1)%250) ==0 ) std::cout << grN+1 << ": "<< grX[grN] << std::endl;
  	    ///gr      grN++;
  	    ///gr    }
  	    ///gr   }
  	    ///gr  }

	    n_entry++;
	    
	    if( (n_entry%n_update)==0 ){
	      std::cout << n_entry << " entries unpacked..."<<std::endl;
	      unpacker_data.UpdateHistograms();
	      calibrator_data.UpdateHistograms();
	      analysis_data.UpdateHistograms();

  	    ///gr  cTS->cd(); cTS->Clear();
  	    ///gr  grTS->DrawGraph(grN,grX,grY,"AP");
  	    ///gr  cTS->Update();
	    }
	  }
	}
	
	//how to break from online?
	if(midas_data.GetBEndOfData()){
	  std::cout << "\n\n ---- Reached end of input file --- " << std::endl;
	  break;
	}
      }
    } // If SourceData.IsOpen()
    
    midas_data.Close();  
    
    Nloop++;
  }while(Nloop<RLnum); //if list of runs, loop over RLnum data files.
  
  if(b_root_file){
    
    std::cout << "\n   + SAVING HISTOGRAMS AND ROOT TREES TO FILE: " <<FileNameRoot<<std::endl;
    
    ///gr   cTS->Write();
    unpacker_data.Close();
    calibrator_data.Close();
    analysis_data.Close();
    
    fMain->Close();
  }
  
  rootapp->Terminate();
  rootapp->Run();

  std::cout << "\n ++++ And this happens after exiting from rootapp ++++"<<std::endl;

  return 0;
}
