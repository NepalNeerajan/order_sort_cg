#ifndef _Common_H
#define _Common_H


namespace common{


  const int N_CHANNEL  = 64; //# channels per FEE64 module (will not change! so hard coded in some parts of code)

  //modules in AIDA setup
  const int N_FEE64 = 24;
  //number of DSSD in detector stack
  const int N_DSSD = 6;
  
  const int ADC_ZERO = 32768; //2**15 

}

#endif
