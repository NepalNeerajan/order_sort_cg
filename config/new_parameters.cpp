#include <bitset>
#include <fstream>
#include <iostream>
//#include <stdio.h>
#include <sstream>
#include <string>


using namespace std;







int main(){

  int n_fee= 33;
  int n_ch= 64;

  string s_dssd= "map_dssd";
  string s_side= "map_side";
  string s_polarity= "adc_polarity";


  int m_dssd[33]= {-1,
		   3,2,3,2, /* 1:4 */
		   -1,1,2,3, /* 5:8 */
		   -1,1,1,-1, /* 9:12 */
		   3,2,-1,1, /* 13:16 */
		   -1,-1,-1,-1, /* 17:20 */
		   -1,-1,-1,-1, /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,0,-1,-1 /* 29:32 */};


  int m_side[33]= {-1,
		   1,1,0,0, /* 1:4 */
		   -1,1,0,0, /* 5:8 */
		   -1,1,0,-1, /* 9:12 */
		   1,1,-1,0, /* 13:16 */
		   -1,-1,-1,-1, /* 17:20 */
		   -1,-1,-1,-1, /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,0,-1,-1 /* 29:32 */};

  int m_strip[33]= {-1,
		   2,2,1,1, /* 1:4 */
		   -1,1,2,2, /* 5:8 */
		   -1,2,1,-1, /* 9:12 */
		   1,1,-1,2, /* 13:16 */
		   -1,-1,-1,-1, /* 17:20 */
		   -1,-1,-1,-1, /* 21:24 */		   
		   -1,-1,-1,-1, /* 25:28 */
		   -1,0,-1,-1 /* 29:32 */};



  int i, j;

  cout << "#map fee64 -> DSSD" << endl;
  for(i=0; i<n_fee; i++){
    cout << s_dssd << "  " <<i <<"  "<< m_dssd[i] << endl;
  }

  cout << "\n#map fee64 -> side (n:0 p:1)" << endl;
  for(i=0; i<n_fee; i++){
    cout << s_side << "  " <<i <<"  "<< m_side[i] << endl;
  }

  
  cout << "\n#map strip: 1: left/down side, 2: right/top side (from beam view)" << endl;
  for(i=0; i<n_fee; i++){
    cout << "map_strip  " <<i <<"  "<< m_strip[i] << endl;
  }


  cout << "\n#adc polarity: -1 for p-side, +1 for n-side (polarity of signal inverted by pre-amp)"<< endl;
  for(i=0; i<n_fee; i++){
    cout << "adc_polarity  "<<i <<"  ";
    if(m_side[i]== 0) cout <<  1 << endl; //n-side: positive adc
    else if (m_side[i]== 1) cout << -1 << endl; //p-side: negative adc
    else cout << 0 << endl;
    //for(j=0; j<n_ch; j++){

    //}
  }

  cout << endl;
  for(i=0; i<n_fee; i++){
    cout << "b_ch_enabled  " << i << "  1  0"<<endl; //disable ch=1 in all
    cout << "b_ch_enabled  " << i << "  55  0"<<endl; //disable ch=1 in all
  }
  cout << "b_ch_enabled  4  58  0"<<endl;
  cout << "b_ch_enabled  4  62  0"<<endl;
  cout << "b_ch_enabled  7  58  0"<<endl;
  cout << "b_ch_enabled  7  62  0"<<endl;

  //run R51_5
  cout <<"#run R51_5"<<endl;
  cout << "b_ch_enabled  3  0  0"<<endl;
  cout << "b_ch_enabled  4  0  0"<<endl;
  cout << "b_ch_enabled  4  61  0"<<endl;
  cout << "b_ch_enabled  7  61  0"<<endl;
  cout << "b_ch_enabled  8  0  0"<<endl;
  cout << "b_ch_enabled  11  0  0"<<endl;


  return 0;
  
}

