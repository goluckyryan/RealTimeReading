#ifndef HELIOSTARGET
#define HELIOSTARGET

#include "../Class/GenericPlane.h"

class HeliosTarget : public GenericPlane{
  RQ_OBJECT("HelioTarget");
public:

  HeliosTarget();
  ~HeliosTarget();
  
  void SetOthersHistograms();
  void SetCanvasDivision();
  
  void Fill(vector<UInt_t> energy);
  void Fill(vector<vector<UInt_t>> energy);
  
  void Draw();

  void ClearHistograms();

private:
  
  TH1F * hX;
  TH1F * hY;
  TH2F * hXY;

  TH1F * hXg;
  TH1F * hYg;
  TH2F * hXYg;
  
  TH1F * hX1, * hX2, * hY1, * hY2;
  
  int chX1, chX2; // yellow, Red
  int chY1, chY2; // Blue, White
  
};

HeliosTarget::HeliosTarget(){
    
  //=========== ClassName and ClassID is for class identification in BoxScoreXY
  className = "HeliosTarget";
  classID = 1;
  location = "target";  //this is for database tag
  
  //=========== Channel Mask and rangeDE and rangeE is for GenericPlane
  ChannelMask = 0xb6; /// Channel enable mask, 0x01, only frist channel, 0xff, all channel
  
  rangeDE[0] =     0; /// min range for dE
  rangeDE[1] = 80000; /// max range for dE
  rangeE[0] =      0; /// min range for E
  rangeE[1] =  60000; /// max range for E
  rangeTime =    500; /// range for Tdiff, nano-sec
  
  //chdE = 1;  chdEGain = 0; 
  chE = 7;   chEGain = 1.0;
  mode = 5; ///default channel Gain is equal
  
  NChannelForRealEvent = 5;
  
  //=========== custom histograms for HelioTarget
  hX = NULL;
  hY = NULL;
  hXY = NULL;

  hXg = NULL;
  hYg = NULL;
  hXYg = NULL;
  
  hHit = NULL;
  
  hX1 = NULL;
  hX2 = NULL;
  hY1 = NULL;
  hY2 = NULL;
  
  chX1 = 1;
  chX2 = 5;
  
  chY1 = 2;
  chY2 = 4;
  
  GenericPlane::SetChannelMask(1,0,1,1, 0,1,1,0);
  
}

HeliosTarget::~HeliosTarget(){
  
  delete hX;
  delete hY;
  delete hXY;
  
  delete hXg;
  delete hYg;
  delete hXYg;
  
  delete hHit;
  
  delete hX1;
  delete hX2;
  delete hY1;
  delete hY2;
  
}

void HeliosTarget::SetOthersHistograms(){
  
  int bin = 200;
  float labelSize = 0.08;
  
  float xMin = -0.9;
  float xMax =  0.9;
  float yMin = -0.9;
  float yMax =  0.9;
  
  hX = new TH1F("hX", "X; X[ch]; count", bin, xMin, xMax);
  hY = new TH1F("hY", "Y; Y[ch]; count", bin, yMin, yMax);  
  hXY  = new TH2F("hXY", "X-Y; X[ch]; Y[ch]", bin, xMin, xMax, bin, yMin, yMax);

  hXg = new TH1F("hXg", "X (cut1); X[ch]; count", bin, xMin, xMax);
  hYg = new TH1F("hYg", "Y (cut1); Y[ch]; count", bin, yMin, yMax);
  hXYg = new TH2F("hXYg", "X-Y (cut1); X[ch]; Y[ch]", bin, xMin, xMax, bin, yMin, yMax);
  
  hX->GetXaxis()->SetLabelSize(labelSize);
  hX->GetYaxis()->SetLabelSize(labelSize);
  
  hY->GetXaxis()->SetLabelSize(labelSize);
  hY->GetYaxis()->SetLabelSize(labelSize);
  
  hXY->GetXaxis()->SetLabelSize(labelSize);
  hXY->GetYaxis()->SetLabelSize(labelSize);

  hXg->SetLineColor(2);
  hYg->SetLineColor(2);
  hXYg->SetLineColor(2);

  hXY->SetMinimum(1);
  hXYg->SetMinimum(1);
  
  hX1 = new TH1F("hX1", Form("X1 (ch=%d)", chX1), bin, 1000, 26000);
  hX2 = new TH1F("hX2", Form("X2 (ch=%d)", chX2), bin, 1000, 26000);
  hY1 = new TH1F("hY1", Form("Y1 (ch=%d)", chY1), bin, 1000, 26000);
  hY2 = new TH1F("hY2", Form("Y2 (ch=%d)", chY2), bin, 1000, 26000);
  
  hdE->SetTitle("raw dE(Y1+Y2)");;
  
  isHistogramSet = true;
  
  printf(" Histogram setted. \n");
  
  mode = 5;
  
}

void HeliosTarget::SetCanvasDivision(){
 
  //GenericPlane::SetCanvasDivision();
  
  fCanvas->Divide(1,2);
  fCanvas->cd(1)->Divide(2,1); 
  fCanvas->cd(1)->cd(1)->SetLogz();
  fCanvas->cd(1)->cd(2)->Divide(2,2);
  
  
  fCanvas->cd(2)->Divide(2,1); 
  fCanvas->cd(2)->cd(1)->Divide(2,2);
  fCanvas->cd(2)->cd(2)->Divide(2,2);

}

void HeliosTarget::Draw(){
  
  if ( !isHistogramSet ) return;
  
  //fCanvas->cd(1)->cd(1); hdEtotE->Draw("colz");
  fCanvas->cd(1)->cd(1); hdEE->Draw("colz");

  if( numCut > 0 ){
    for( int i = 0; i < numCut; i++){
      cutG = (TCutG *) cutList->At(i);
      cutG->Draw("same");
    }
  }

  fCanvas->cd(1)->cd(2)->cd(1); hE->Draw();
  fCanvas->cd(1)->cd(2)->cd(2); hdE->Draw();
  fCanvas->cd(1)->cd(2)->cd(4); hTDiff->Draw(); line->Draw();
  fCanvas->cd(1)->cd(2)->cd(3); rateGraph->Draw("AP"); legend->Draw();
  
  fCanvas->cd(2)->cd(2)->cd(1); hX1->Draw("");
  fCanvas->cd(2)->cd(2)->cd(2); hX2->Draw("");
  fCanvas->cd(2)->cd(2)->cd(3); hY1->Draw("");
  fCanvas->cd(2)->cd(2)->cd(4); hY2->Draw("");
  
  fCanvas->cd(2)->cd(1)->cd(2); hHit->Draw("HIST");
  fCanvas->cd(2)->cd(1)->cd(3); gStyle->SetOptStat("neiour"); hX->Draw("");
  if( numCut > 0  ) hXg->Draw("same");
  fCanvas->cd(2)->cd(1)->cd(4); gStyle->SetOptStat("neiour"); hY->Draw("");
  if( numCut > 0  ) hYg->Draw("same");
  fCanvas->cd(2)->cd(1)->cd(1); 
  fCanvas->cd(2)->cd(1)->cd(1)->SetGrid(); 
  gStyle->SetOptStat("neiou"); hXY->Draw("colz"); 
  if( numCut > 0  ) hXYg->Draw("box same");
  fCanvas->cd(2)->cd(1)->cd(1)->SetLogz();
  
  fCanvas->Modified();
  fCanvas->Update();
  gSystem->ProcessEvents();
  
  mode = 5;
  
}


void HeliosTarget::Fill(vector<UInt_t> energy){
  
  //GenericPlane::Fill(energy);
  
  if ( !isHistogramSet ) return;
  
  int E = energy[chE] ;//+ gRandom->Gaus(0, 500);
  int dE = energy[chY1] + energy[chY2] ;//+ gRandom->Gaus(0, 500);
  
  float X = ((float)energy[chX1] - (float)energy[chX2])*1.0/(energy[chX1] + energy[chX2]);
  float Y = ((float)energy[chY1] - (float)energy[chY2])*1.0/(energy[chY1] + energy[chY2]);

  hX1->Fill(energy[chX1]);
  hX2->Fill(energy[chX2]);
  hY1->Fill(energy[chY1]);
  hY2->Fill(energy[chY2]);

  if( X != 0.0 ) hX->Fill(X);
  if( Y != 0.0 ) hY->Fill(Y);
  hXY->Fill(X, Y);
  
  hE->Fill(E);
  if( energy[chX1] !=0 && energy[chX2] !=0 && energy[chY1] !=0 && energy[chY2] !=0 ) {
    hdE->Fill(dE);
    hdEE->Fill(E, dE);
  }
  
  float totalE = dE * chdEGain + E * chEGain;
  hdEtotE->Fill(totalE, dE);
  
  if( numCut > 0  ){
    for( int i = 0; i < numCut; i++){
      cutG = (TCutG *) cutList->At(i);
      if( cutG->IsInside(E, dE)){
        countOfCut[i] += 1;
        if( i == 0 && X!= 0.0 && Y!=0.0) {
          hXYg->Fill(X,Y);
          hXg->Fill(X);
          hYg->Fill(Y);
        }
      }
    }
  }
  
}


void HeliosTarget::Fill(vector<vector<UInt_t>> Energy){
  
  for( int i = 0 ; i < Energy.size() ; i++){
    vector<UInt_t> energy = Energy[i];
    Fill(energy);
  }
  
  
}

void HeliosTarget::ClearHistograms(){
  
  GenericPlane::ClearHistograms();
  
  hX1->Reset();
  hX2->Reset();
  hY1->Reset();
  hY2->Reset();
  
  hX->Reset();
  hY->Reset();
  hXY->Reset();
  
  hXg->Reset();
  hYg->Reset();
  hXYg->Reset();
  
  hHit->Reset();
  
}

#endif
