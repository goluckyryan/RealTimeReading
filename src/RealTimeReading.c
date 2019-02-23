/******************************************************************************
*
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#include <CAENDigitizer.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <thread>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h> 
#include <vector>

#include "Functions.h"

#include "TROOT.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TGraph.h"
#include "TCutG.h"
#include "TMultiGraph.h"
#include "TApplication.h"
#include "TObjArray.h"
#include "TLegend.h"
#include "TRandom.h"

using namespace std;

//TODO 1) change eventbuild method
//TODO 2) add cutting method
//TODO 4) add rate graph
//TODO 3) Input Dynamic is only 0.5 or 2 Vpp, how to set?

//========== General Digitizer setting;
#define MaxNChannels 8

const float DCOFFSET = 0.2;
const bool PositivePulse = true;
const int RECORDLENGTH = 20000;   // Num of samples of the waveforms (only for waveform mode)
const int PreTriggerSize = 2000;
const int CHANNELMASK = 0xFF;   // Channel enable mask, 0x01, only frist channel, 0xff, all channel

const int CONINCIDENTTIME = 30000; // real time = CONINCIDENTTIME * 50 ns 

//========= Histogram
TCanvas * cCanvas = NULL;
TH1F * hE = NULL;
TH1F * htotE = NULL;
TH1F * hdE = NULL;
TH2F * hEdE = NULL; 
//======== Rate Graph
TMultiGraph * rateGraph = NULL;
TGraph * graphRate = NULL;
TGraph ** graphRateCut; //!

//======== TCutG
TString cutName;
TCutG* cutG; //!
TObjArray * cutList = NULL;
TString cutTag;
Bool_t isCutFileOpen;
int numCut;
vector<int> countFromCut;

//channel ID for dE and E
const int chE = 0;
const int chDE = 1;

/* ###########################################################################
*  Functions
*  ########################################################################### */
int* ReadChannelSetting(int ch, string fileName){

  int * para = new int[17];
  
  ifstream file_in;
  file_in.open(fileName.c_str(), ios::in);

  if( !file_in){
    printf("ch: %d ------ default setting.\n", ch);
    para[0] = 100;      // Trigger Threshold (in LSB)
    para[1] = 3000;     // Trapezoid Rise Time (ns) 
    para[2] = 900;      // Trapezoid Flat Top  (ns) 
    para[3] = 50000;    // Decay Time Constant (ns) 
    para[4] = 500;      // Flat top delay (peaking time) (ns) 
    para[5] = 4;        // Trigger Filter smoothing factor (number of samples to average for RC-CR2 filter) Options: 1; 2; 4; 8; 16; 32
    para[6] = 200;      // Input Signal Rise time (ns) 
    para[7] = 1200;     // Trigger Hold Off
    para[8] = 4;        // number of samples for baseline average calculation. Options: 1->16 samples; 2->64 samples; 3->256 samples; 4->1024 samples; 5->4096 samples; 6->16384 samples
    para[9] = 0;        // Peak mean (number of samples to average for trapezoid height calculation). Options: 0-> 1 sample; 1->4 samples; 2->16 samples; 3->64 samples
    para[10] = 2000;    // peak holdoff (ns)
    para[11] = 500;     // Baseline holdoff (ns)
    para[12] = 1.0;     // Energy Normalization Factor
    para[13] = 0;       // decimation (the input signal samples are averaged within this number of samples): 0 ->disabled; 1->2 samples; 2->4 samples; 3->8 samples
    para[14] = 0;       // decimation gain. Options: 0->DigitalGain=1; 1->DigitalGain=2 (only with decimation >= 2samples); 2->DigitalGain=4 (only with decimation >= 4samples); 3->DigitalGain=8( only with decimation = 8samples).
    para[15] = 0;       // Enable Rise time Discrimination. Options: 0->disabled; 1->enabled
    para[16] = 100;     // Rise Time Validation Window (ns)
  }else{
    printf("ch: %d ------ load from %s.\n", ch, fileName.c_str());
    string line;
    int count = 0;
    while( file_in.good()){
      getline(file_in, line);
      size_t pos = line.find("//");
      if( pos > 1 ){
        if( count > 16) break;
        para[count] = atoi(line.substr(0, pos).c_str());
        //printf("%d | %d \n", count, para[count]);
        count ++;
      }
    }
  }

  return para;
}

int ProgramDigitizer(int handle, DigitizerParams_t Params, CAEN_DGTZ_DPP_PHA_Params_t DPPParams){
    /* This function uses the CAENDigitizer API functions to perform the digitizer's initial configuration */
    int i, ret = 0;

    /* Reset the digitizer */
    ret |= CAEN_DGTZ_Reset(handle);

    if (ret) {
        printf("ERROR: can't reset the digitizer.\n");
        return -1;
    }
    ret |= CAEN_DGTZ_WriteRegister(handle, 0x8000, 0x01000114);  // Channel Control Reg (indiv trg, seq readout) ??

    /* Set the DPP acquisition mode
    This setting affects the modes Mixed and List (see CAEN_DGTZ_DPP_AcqMode_t definition for details)
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyOnly        Only energy (DPP-PHA) or charge (DPP-PSD/DPP-CI v2) is returned
    CAEN_DGTZ_DPP_SAVE_PARAM_TimeOnly        Only time is returned
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime    Both energy/charge and time are returned
    CAEN_DGTZ_DPP_SAVE_PARAM_None            No histogram data is returned */
    ret |= CAEN_DGTZ_SetDPPAcquisitionMode(handle, Params.AcqMode, CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);
    
    // Set the digitizer acquisition mode (CAEN_DGTZ_SW_CONTROLLED or CAEN_DGTZ_S_IN_CONTROLLED)
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);
    
    // Set the number of samples for each waveform
    ret |= CAEN_DGTZ_SetRecordLength(handle, Params.RecordLength);

    // Set the I/O level (CAEN_DGTZ_IOLevel_NIM or CAEN_DGTZ_IOLevel_TTL)
    ret |= CAEN_DGTZ_SetIOLevel(handle, Params.IOlev);

    /* Set the digitizer's behaviour when an external trigger arrives:

    CAEN_DGTZ_TRGMODE_DISABLED: do nothing
    CAEN_DGTZ_TRGMODE_EXTOUT_ONLY: generate the Trigger Output signal
    CAEN_DGTZ_TRGMODE_ACQ_ONLY = generate acquisition trigger
    CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT = generate both Trigger Output and acquisition trigger

    see CAENDigitizer user manual, chapter "Trigger configuration" for details */
    ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);

    // Set the enabled channels
    ret |= CAEN_DGTZ_SetChannelEnableMask(handle, Params.ChannelMask);

    // Set how many events to accumulate in the board memory before being available for readout
    ret |= CAEN_DGTZ_SetDPPEventAggregation(handle, Params.EventAggr, 0);
    
    /* Set the mode used to syncronize the acquisition between different boards.
    In this example the sync is disabled */
    ret |= CAEN_DGTZ_SetRunSynchronizationMode(handle, CAEN_DGTZ_RUN_SYNC_Disabled);
    
    // Set the DPP specific parameters for the channels in the given channelMask
    ret |= CAEN_DGTZ_SetDPPParameters(handle, Params.ChannelMask, &DPPParams);
    
    for(i=0; i<MaxNChannels; i++) {
        if (Params.ChannelMask & (1<<i)) {
            // Set a DC offset to the input signal to adapt it to digitizer's dynamic range
            //ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, 0x3333); // 20%
            ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, int( 0xffff * DCOFFSET ));
            
            // Set the Pre-Trigger size (in samples)
            ret |= CAEN_DGTZ_SetDPPPreTriggerSize(handle, i, PreTriggerSize);
            
            // Set the polarity for the given channel (CAEN_DGTZ_PulsePolarityPositive or CAEN_DGTZ_PulsePolarityNegative)
            ret |= CAEN_DGTZ_SetChannelPulsePolarity(handle, i, Params.PulsePolarity);
        }
    }

    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, ANALOG_TRACE_1, CAEN_DGTZ_DPP_VIRTUALPROBE_Delta2);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, ANALOG_TRACE_2, CAEN_DGTZ_DPP_VIRTUALPROBE_Input);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, DIGITAL_TRACE_1, CAEN_DGTZ_DPP_DIGITALPROBE_Peaking);

    if (ret) {
        printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");
        return ret;
    } else {
        return 0;
    }
}

void painCanvas(){
  //This function is running in a parrellel thread.
  //This continously update the Root system with user input
  //avoid frozen
  do{
    gSystem->ProcessEvents();
  }while(true);
}

/* ########################################################################### */
/* MAIN                                                                        */
/* ########################################################################### */
int main(int argc, char *argv[]){
    
  if( argc != 2 ) {
    printf("Please input boardID! \n");
    return -1;
  }
  
  const int boardID = atoi(argv[1]);

  TApplication app ("app", &argc, argv);
  
  /* The following variable is the type returned from most of CAENDigitizer
  library functions and is used to check if there was an error in function
  execution. For example:
  ret = CAEN_DGTZ_some_function(some_args);
  if(ret) printf("Some error"); */
  //CAEN_DGTZ_ErrorCode ;
  int ret;

  /* Buffers to store the data. The memory must be allocated using the appropriate
  CAENDigitizer API functions (see below), so they must not be initialized here
  NB: you must use the right type for different DPP analysis (in this case PHA) */
  char *buffer = NULL;                                 // readout buffer
  CAEN_DGTZ_DPP_PHA_Event_t       *Events[MaxNChannels];  // events buffer

  /* The following variables will store the digitizer configuration parameters */
  CAEN_DGTZ_DPP_PHA_Params_t DPPParams;
  DigitizerParams_t Params;

  /* Arrays for data analysis */
  uint64_t PrevTime[MaxNChannels];
  uint64_t ExtendedTT[MaxNChannels];
  int ECnt[MaxNChannels];
  int TrgCnt[MaxNChannels];
  int PurCnt[MaxNChannels];

  /* The following variable will be used to get an handler for the digitizer. The
  handler will be used for most of CAENDigitizer functions to identify the board */
  int handle;

  /* Other variables */
  int i, ch, ev;
  int Quit=0;
  int AcqRun = 0;
  uint32_t AllocatedSize, BufferSize;
  int Nb=0;
  int MajorNumber;
  uint64_t CurrentTime, PrevRateTime, ElapsedTime;
  uint64_t StartTime, StopTime;
  uint32_t NumEvents[MaxNChannels];
  CAEN_DGTZ_BoardInfo_t           BoardInfo;
  uint32_t temp;

  /* *************************************************************************************** */
  /* Set Parameters                                                                          */
  /* *************************************************************************************** */
  memset(&Params, 0, sizeof(DigitizerParams_t));
  memset(&DPPParams, 0, sizeof(CAEN_DGTZ_DPP_PHA_Params_t));
  /****************************\
  * Communication Parameters   *
  \****************************/
  Params.LinkType = CAEN_DGTZ_USB;  // Link Type
  Params.VMEBaseAddress = 0;  // For direct USB connection, VMEBaseAddress must be 0
  Params.IOlev = CAEN_DGTZ_IOLevel_NIM;
  /****************************\
  *  Acquisition parameters    *
  \****************************/
  //Params.AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_Mixed;          // CAEN_DGTZ_DPP_ACQ_MODE_List or CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope
  Params.AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List;             // CAEN_DGTZ_DPP_ACQ_MODE_List or CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope
  Params.RecordLength = RECORDLENGTH;                       // Num of samples of the waveforms (only for Oscilloscope mode)
  Params.ChannelMask = CHANNELMASK;                         // Channel enable mask, 0x01, only frist channel, 0xff, all channel
  Params.EventAggr = 1;                                     // number of events in one aggregate (0=automatic), number of event acculated for read-off
  if( PositivePulse ) {
    Params.PulsePolarity = CAEN_DGTZ_PulsePolarityPositive; // Pulse Polarity (this parameter can be individual)
  }else{
    Params.PulsePolarity = CAEN_DGTZ_PulsePolarityNegative; 
  }
  /****************************\
  *      DPP parameters        * 
  \****************************/
  for(ch=0; ch<MaxNChannels; ch++) {
    int* para = ReadChannelSetting(ch, "setting_" + to_string(ch) + ".txt");
    DPPParams.thr[ch] = para[0]; 
    DPPParams.k[ch] = para[1];  
    DPPParams.m[ch] = para[2];  
    DPPParams.M[ch] = para[3];   
    DPPParams.ftd[ch] = para[4];  
    DPPParams.a[ch] = para[5];    
    DPPParams.b[ch] = para[6];    
    DPPParams.trgho[ch] = para[7];  
    DPPParams.nsbl[ch] = para[8];  
    DPPParams.nspk[ch] = para[9];    
    DPPParams.pkho[ch] = para[10]; 
    DPPParams.blho[ch] = para[11]; 
    DPPParams.enf[ch] = para[12]; 
    DPPParams.decimation[ch] = para[13]; 
    DPPParams.dgain[ch] = para[14];    
    DPPParams.trgwin[ch] = para[15]; 
    DPPParams.twwdt[ch] = para[16]; 
  }
    
  /* *************************************************************************************** */
  /* Open the digitizer and read board information                                           */
  /* *************************************************************************************** */
  CAEN_DGTZ_ErrorCode ret1 = CAEN_DGTZ_OpenDigitizer(Params.LinkType, boardID, 0, Params.VMEBaseAddress, &handle);
  if (ret1 != 0) {
    printf("Can't open digitizer\n");
    return 0;
  }

  /* Once we have the handler to the digitizer, we use it to call the other functions */
  ret1 = CAEN_DGTZ_GetInfo(handle, &BoardInfo);
  if (ret1 != 0) {
    printf("Can't read board info\n");
    return 0;
  }
  printf("\nConnected to CAEN Digitizer Model %s, recognized as board %d\n", BoardInfo.ModelName, boardID);
  printf("Number of Channels : %d\n", BoardInfo.Channels);
  printf("SerialNumber : %d\n", BoardInfo.SerialNumber);
  printf("ROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
  printf("AMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);

  /* Check firmware revision (only DPP firmwares can be used with this Demo) */
  sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
  if (MajorNumber != V1730_DPP_PHA_CODE) {
    printf("This digitizer has not a DPP-PHA firmware\n");
    return 0;
  }

  /* *************************************************************************************** */
  /* Program the digitizer (see function ProgramDigitizer)                                   */
  /* *************************************************************************************** */
  ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer(handle, Params, DPPParams);
  if (ret != 0) {
    printf("Failed to program the digitizer\n");
    return 0;
  }

  /* WARNING: The mallocs MUST be done after the digitizer programming,
  because the following functions needs to know the digitizer configuration
  to allocate the right memory amount */
  /* Allocate memory for the readout buffer */
  ret = CAEN_DGTZ_MallocReadoutBuffer(handle, &buffer, &AllocatedSize);
  /* Allocate memory for the events */
  ret |= CAEN_DGTZ_MallocDPPEvents(handle, reinterpret_cast<void**>(&Events), &AllocatedSize) ;     
  
  if (ret != 0) {
    printf("Can't allocate memory buffers\n");
    CAEN_DGTZ_SWStopAcquisition(handle);
    CAEN_DGTZ_CloseDigitizer(handle);
    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    CAEN_DGTZ_FreeDPPEvents(handle, reinterpret_cast<void**>(&Events));
    return 0;
  }
  
  /* *************************************************************************************** */
  /* ROOT TREE                                                                           */
  /* *************************************************************************************** */
  
  vector<ULong64_t> rawTimeStamp;
  vector<UInt_t> rawEnergy;
  vector<int> rawChannel;
  
  TFile * fileout = new TFile("tree.root", "RECREATE");
  TTree * tree = new TTree("tree", "tree");
  
  ULong64_t timeStamp[8];
  UInt_t energy[8];
  int channel[8];
  
  tree->Branch("ch", channel, "channel[8]/I");
  tree->Branch("e", energy, "energy[8]/i");
  tree->Branch("t", timeStamp, "timeStamp[8]/l");
  
  tree->Write("tree", TObject::kOverwrite); 
  
  fileout->Close();
  
  //=== Raw tree
  TFile * fileRaw = new TFile("raw.root", "RECREATE");
  TTree * rawTree = new TTree("rawtree", "rawtree");
  
  ULong64_t t_r;
  UInt_t e_r;
  int ch_r;
  
  rawTree->Branch("ch", &ch_r, "channel/I");
  rawTree->Branch("e", &e_r, "energy/i");
  rawTree->Branch("t", &t_r, "timeStamp/l");
  
  //==== Drawing 
  gStyle->SetOptStat("neiou");
  cCanvas = new TCanvas("cCanvas", "RAISOR isotopes production", 1200, 800);
  cCanvas->Divide(1,2);
  cCanvas->cd(1); gPad->Divide(4,1);
  
  hE    = new TH1F(   "hE", "E ; count ; E [ch]",          500, 0, 5000);
  htotE = new TH1F("htotE", "total E ; count ; totE [ch]", 500, 0, 5000);
  hdE   = new TH1F(  "hdE", "dE ; count ; dE [ch]",        500, 0, 5000);
  hEdE  = new TH2F( "hEdE", "dE - totE ; dE [ch ; totalE [ch]", 500, 0, 5000, 500, 0, 5000);  
  
  rateGraph = new TMultiGraph();
  rateGraph->SetTitle("Instantaneous Beam rate [pps]; Time [sec]; Rate [pps]");
 
  graphRate = new TGraph();
  graphRate->SetTitle("Total Rate [pps]");
  graphRate->SetMarkerColor(4);
  graphRate->SetMarkerStyle(20);
  graphRate->SetMarkerSize(1);
  
  rateGraph->Add(graphRate);
  
  TLegend * legend = new TLegend( 0.6, 0.2, 0.9, 0.4); 
  legend->AddEntry(graphRate, "Total Beam rate");
  
  //check is there any cut.root
  TFile * fCut = new TFile("cutsFile.root");  // open file
  isCutFileOpen = fCut->IsOpen(); 
  numCut = 0 ;
  if( isCutFileOpen ){
    cutList = (TObjArray *) fCut->FindObjectAny("cutList");
    numCut = cutList->GetEntries();
    printf("=========== found %d cutG \n", numCut);
    cutG = new TCutG();
    graphRateCut = new TGraph * [numCut];
    for(int i = 0; i < numCut ; i++){
      printf(" cut name : %s \n", cutList->At(i)->GetName());
      countFromCut.push_back(0);
        
      graphRateCut[i] = new TGraph();
      graphRateCut[i]->SetMarkerColor(i+1);
      graphRateCut[i]->SetMarkerStyle(20+i);
      graphRateCut[i]->SetMarkerSize(1);
      rateGraph->Add(graphRateCut[i]);
      legend->AddEntry(graphRateCut[i], cutList->At(i)->GetName());
    }
  }

  thread th(painCanvas); // using loop keep root responding

  /* *************************************************************************************** */
  /* Readout Loop                                                                            */
  /* *************************************************************************************** */

  for (ch = 0; ch < MaxNChannels; ch++) {
    TrgCnt[ch] = 0;
    ECnt[ch] = 0;
    PrevTime[ch] = 0;
    ExtendedTT[ch] = 0;
    PurCnt[ch] = 0;
  }
  PrevRateTime = get_time();
  AcqRun = 0;
  PrintInterface();
  int evCount = 0;
  int graphIndex = 0;
  printf("Type a command: ");
  
  uint32_t initClock[8] = {0,0,0,0,0,0,0,0};
    
    while(!Quit) {
        // Check keyboard
        if(kbhit()) {
            char c;
            c = getch();
            if (c == 'q') {
                printf("=========== bye bye ==============\n");
                Quit = 1;
            }
            if (c == 's')  {
                // Start Acquisition
                // NB: the acquisition for each board starts when the following line is executed
                // so in general the acquisition does NOT starts syncronously for different boards
                if( graphIndex == 0 ) {
                  StartTime = get_time();
                  evCount = 0;
                }
                CAEN_DGTZ_SWStartAcquisition(handle);
                printf("Acquisition Started for Board %d\n", boardID);
                AcqRun = 1;
                
                //system("root reader.C+");
            }
            if (c == 'a')  {
                // Stop Acquisition
                CAEN_DGTZ_SWStopAcquisition(handle); 
                StopTime = get_time();  
                printf("\n====== Acquisition STOPPED for Board %d\n", boardID);
                printf("---------- Duration : %lu msec\n", StopTime - StartTime);
                PrintInterface();
                AcqRun = 0;
                
            }
            if( c == 'c' && cutList == NULL ){
                /*
                // pause and make cuts
                CAEN_DGTZ_SWStopAcquisition(handle); 
                StopTime = get_time();  
                printf("\n====== Acquisition STOPPED for Board %d\n", boardID);
                printf("---------- Duration : %lu msec\n", StopTime - StartTime);
                AcqRun = 0;
                
                //make cuts
                if( !cCanvas->GetShowToolBar() ) cCanvas->ToggleToolBar();
                
                TFile * cutFile = new TFile("cutsFile.root", "recreate");
                TCutG * cutTemp = NULL ;
                cutList = new TObjArray();
                
                numCut = 1;
                do{
                  cCanvas->cd(1); gPad->cd(1);
                  gPad->WaitPrimitive();
                  
                  cutTemp = (TCutG*) gROOT->FindObject("CUTG");
                  
                  if( cutTemp == NULL ){
                    printf(" cannot find TCutG object \n");
                    numCut -- ;
                    break;
                  }
                  
                  TString name; name.Form("cut%d", numCut);
                  cutTemp->SetName(name);
                  cutTemp->SetLineColor(numCut);
                  cutList->Add(cutTemp);
                  
                  printf(" cut-%d \n", numCut);
                  numCut++;
                }while( cutTemp != NULL );
                
                cutList->Write("cutList", TObject::kSingleKey);
                printf("====> saved %d cuts into cutsFile.root\n", numCut);

                if( numCut >= 1 ){
                  countFromCut.clear();
                  graphRateCut = new TGraph * [numCut];
                  countFromCut.push_back(0);
                  for(int i = 0; i < numCut ; i++){
                    countFromCut.push_back(0);
                    graphRateCut[i] = new TGraph();
                    graphRateCut[i]->SetMarkerColor(i+1);
                    graphRateCut[i]->SetMarkerStyle(20+i);
                    graphRateCut[i]->SetMarkerSize(1);
                    rateGraph->Add(graphRateCut[i]);
                    legend->AddEntry(graphRateCut[i], cutList->At(i)->GetName());
                  }
                }*/
            }
        }
        if (!AcqRun) {
            Sleep(10); // 10 mili-sec
            continue;
        }
    
        /* Calculate throughput and trigger rate (every second) */
        CurrentTime = get_time();
        ElapsedTime = CurrentTime - PrevRateTime; /* milliseconds */
        int updatePeriod = 1000;
        int countEventBuilt = 0;
        if (ElapsedTime > updatePeriod) {
            system(CLEARSCR);
            PrintInterface();
            printf("\n======== Tree, Histograms and Table update every ~%.2f sec\n", updatePeriod/1000.);
            printf("Time Elapsed = %lu msec\n", CurrentTime - StartTime);
            printf("Readout Rate = %.5f MB\n", (float)Nb/((float)ElapsedTime*1048.576f));
            printf("Total number of Event = %d \n", evCount);
            printf("\nBoard %d:\n",boardID);
            for(i=0; i<MaxNChannels; i++) {
                if (TrgCnt[i]>0){
                    printf("\tCh %d:\tTrgRate=%.2f Hz\tPileUpRate=%.2f%%\n", i, (float)TrgCnt[i]/(float)ElapsedTime *1000., (float)PurCnt[i]*100/(float)TrgCnt[i]);
                }else{
                    printf("\tCh %d:\tNo Data\n", i);
                }
                TrgCnt[i]=0;
                PurCnt[i]=0;
            }
            Nb = 0;
            PrevRateTime = CurrentTime;
            printf("\n");
            
            //sort event from tree and append to exist root
            //printf("---- append file \n");
            TFile * fileAppend = new TFile("tree.root", "UPDATE");
            tree = (TTree*) fileAppend->Get("tree");
            
            //UInt_t x[2];
            //ULong64_t t[2];
            //int ch[2];
            tree->SetBranchAddress("e", energy);
            tree->SetBranchAddress("t", timeStamp);
            tree->SetBranchAddress("ch", channel);
            
            int n = rawChannel.size();
            printf(" number of data to sort %d \n", n);
            
            countEventBuilt = 0;
            if(isCutFileOpen){
              for( int i = 0 ; i < numCut; i++ ){
                countFromCut[i] = 0;
              }
            }
            for( int i = 0; i < n-1; i++){
                //printf("---------------------------------- %d, %llu, %d \n", rawChannel[i], rawTimeStamp[i], rawEnergy[i]);
                for( int j = i+1; j < n ; j ++){
                    if( rawChannel[i] == rawChannel[j] ) continue;
                    
                    int timediff = (int) (rawTimeStamp[i] - rawTimeStamp[j]) ;
                    if( TMath::Abs( timediff ) < CONINCIDENTTIME ) { // 6000 ns time diff
                        //printf("---- %d, %llu, %d \n", rawChannel[j], rawTimeStamp[j], rawEnergy[j]);
                        for( int k = 0 ; k < 8 ; k++){
                            channel[k] = -1;
                            energy[k] = 0;
                            timeStamp[k] = 0;
                        }
                        
                        channel[rawChannel[i]] = rawChannel[i];
                        energy[rawChannel[i]] = rawEnergy[i];
                        timeStamp[rawChannel[i]] = rawTimeStamp[i];

                        channel[rawChannel[j]] = rawChannel[j];
                        energy[rawChannel[j]] = rawEnergy[j];
                        timeStamp[rawChannel[j]] = rawTimeStamp[j];
                        
                        countEventBuilt ++;
                        tree->Fill();
                        
                        float deltaE = energy[chDE] ;
                        float totalE = energy[chDE] + energy[chE];
                        
                        htotE->Fill(totalE); // x, y
                        hEdE->Fill(totalE, deltaE); // x, y
                        
                        if(isCutFileOpen){
                          cutList = (TObjArray *) fCut->FindObjectAny("cutList");
                          for( int i = 0 ; i < numCut; i++ ){
                            cutG = (TCutG *)cutList->At(i) ;
                            if( cutG->IsInside(totalE, deltaE)){
                              countFromCut[i] += 1;
                            }
                          }
                        }
                        
                        break;
                    }
                }
            }
            
            printf(" number of data sorted %d, Rate(all) : %f pps \n", countEventBuilt, countEventBuilt*1.0/ElapsedTime*1e3 );
            
            tree->Write("tree", TObject::kOverwrite); 
    
            fileAppend->Close();
            
            cCanvas->cd(1); gPad->cd(1); hEdE->Draw("colz");
            cCanvas->cd(1); gPad->cd(2); hE->Draw();
            cCanvas->cd(1); gPad->cd(3); hdE->Draw();
            cCanvas->cd(1); gPad->cd(4); htotE->Draw();
            //filling rate graph
            graphIndex ++;
            graphRate->SetPoint(graphIndex, (CurrentTime - StartTime)/1e3, countEventBuilt*1.0/ElapsedTime*1e3);
            if(isCutFileOpen){
              cutList = (TObjArray *) fCut->FindObjectAny("cutList");
              for( int i = 0 ; i < numCut; i++ ){
                  graphRateCut[i]->SetPoint(graphIndex, (CurrentTime - StartTime)/1e3, countFromCut[i]*1.0/ElapsedTime*1e3);
                  cutG = (TCutG *)cutList->At(i) ;
                  printf("                           Rate(%s) : %f pps \n", cutG->GetName(), countFromCut[i]*1.0/ElapsedTime*1e3 );
                  cCanvas->cd(1); gPad->cd(1); cutG->Draw("same");
              }
            }
            cCanvas->cd(2); rateGraph->Draw("AP"); legend->Draw();
            cCanvas->Update();
            
            //clear vectors
            rawChannel.clear();
            rawEnergy.clear();
            rawTimeStamp.clear();
            
        }
        
        /* Read data from the board */
        ret = CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
        if (BufferSize == 0)
            continue;
            
        //printf("%d, %d \n", BufferSize, AllocatedSize);

        Nb += BufferSize;
        //ret = DataConsistencyCheck((uint32_t *)buffer, BufferSize/4);
        ret |= (CAEN_DGTZ_ErrorCode) CAEN_DGTZ_GetDPPEvents(handle, buffer, BufferSize, reinterpret_cast<void**>(&Events), NumEvents);
        if (ret) {
            printf("Data Error: %d\n", ret);
            CAEN_DGTZ_SWStopAcquisition(handle);
            CAEN_DGTZ_CloseDigitizer(handle);
            CAEN_DGTZ_FreeReadoutBuffer(&buffer);
            CAEN_DGTZ_FreeDPPEvents(handle, reinterpret_cast<void**>(&Events));
            return 0;
        }
        
        /* Analyze data */
        for (ch = 0; ch < MaxNChannels; ch++) {
            if (!(Params.ChannelMask & (1<<ch)))
                continue;
            
            /* Update display */
            for (ev = 0; ev < NumEvents[ch]; ev++) {
                TrgCnt[ch]++;
                /* Time Tag */
                if (Events[ch][ev].TimeTag < PrevTime[ch]) ExtendedTT[ch]++;
                PrevTime[ch] = Events[ch][ev].TimeTag;
                /* Energy */
                if (Events[ch][ev].Energy > 0) {
                    ECnt[ch]++;
                } else { /* PileUp */
                    PurCnt[ch]++;
                }
                
                if( initClock[ch] == 0 ) initClock[ch] = Events[ch][ev].Extras2;
                ULong64_t baseClock = (((ULong64_t) Events[ch][ev].Extras2) ^ initClock[ch] ) << 15;
                ULong64_t timetag = (ULong64_t) Events[ch][ev].TimeTag;
                timetag += baseClock ; 
                
                evCount ++;
                
                ch_r = ch;
                e_r = Events[ch][ev].Energy + int(gRandom->Gaus(0, 100));
                t_r = timetag;
                rawTree->Fill();
                
                rawChannel.push_back(ch);
                rawEnergy.push_back(e_r);
                rawTimeStamp.push_back(timetag);
                
                //printf("ch : %d , energy: %d \n", ch, e_r);
                if( chDE == ch )  hdE->Fill(e_r); 
                if( chE == ch )  hE->Fill(e_r); 
                
            } // loop on events
        } // loop on channels
        fileRaw->cd();
        rawTree->Write("rawtree", TObject::kOverwrite); 
        
    } // End of readout loop

  rawTree->Write("rawtree", TObject::kOverwrite); 
  fileRaw->Close();

  /* stop the acquisition, close the device and free the buffers */
  CAEN_DGTZ_SWStopAcquisition(handle);
  CAEN_DGTZ_CloseDigitizer(handle);
  CAEN_DGTZ_FreeReadoutBuffer(&buffer);
  CAEN_DGTZ_FreeDPPEvents(handle, reinterpret_cast<void**>(&Events));
  printf("\n");
  return ret;
}
    
