//compile g++ -pthread thread.c -o thread `root-config --cflags --glibs`


#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <thread>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h> 
#include <vector>

#include "TROOT.h"
#include "TSystem.h"
#include "TFile.h"
#include "TStyle.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TApplication.h"
#include <TH2F.h>
#include <TCutG.h>
#include <TString.h>
#include <TObjArray.h>

using namespace std;

TFile * cutFile = NULL ;
TCutG * cut = NULL;
TObjArray * cutList = NULL;

int main(int argc, char* argv[] ){
  
  if( argc != 3 ) {
    printf("Please input channel for dE and E. \n");
    printf("./CutCreator dE E \n");
    return 0;
  }
  
  int chDE = atoi(argv[1]);
  int chEE = atoi(argv[2]);

  TApplication app ("app", &argc, argv);

  // load tree and plot 
  TFile * fileIn = new TFile("tree.root");
  TTree * tree = (TTree*) fileIn->FindObjectAny("tree");
  gStyle->SetOptStat("");
  
  TCanvas * cCutCreator = new TCanvas("cCutCreator", "TCutG Creator", 0, 0, 400, 400);
  if( !cCutCreator->GetShowToolBar() ) cCutCreator->ToggleToolBar();

  TH2F * hEdE = new TH2F("hEdE", "dE - E ; dE [ch] ; E [ch]", 500, 0, 10000, 500, 0, 10000);
  
  TString expression;
  expression.Form("e[%d]:e[%d] + e[%d]>>hEdE", chDE, chDE, chEE);

  tree->Draw(expression, "", "colz");
  gSystem->ProcessEvents();
  //thread th(painCanvas); // using loop keep root responding
  
  // make cuts
  cutFile = new TFile("cutsFile.root", "recreate");
  cutList = new TObjArray();

  int count = 1;
  do{
    printf("======== make a graphic cut on the plot (double click on plot to stop) : " );
    gSystem->ProcessEvents();
    gPad->WaitPrimitive();

    cut = (TCutG*) gROOT->FindObject("CUTG");

    if( cut == NULL) {
      printf(" break \n");
      count --;
      break;
    }

    TString name; name.Form("cut%d", count);
    cut->SetName(name);
    cut->SetLineColor(count);
    cutList->Add(cut);
    
    printf(" cut-%d \n", count);			
    count++;
  
  }while( cut != NULL );

  cutList->Write("cutList", TObject::kSingleKey);

  printf("====> saved %d cuts into rdtCuts.root\n", count);

  gROOT->ProcessLine(".q");

  app.Run();
  
  return 0;
  
}
