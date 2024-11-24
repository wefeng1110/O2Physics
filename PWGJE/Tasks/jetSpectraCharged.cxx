// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

// Charged-particle jet spectra task
//
/// \author Wenhui Feng <wenhui.feng@cern.ch>
/// \author Aimeric Landou <aimeric.landou@cern.ch>
/// \author Nima Zardoshti <nima.zardoshti@cern.ch>

#include <cmath>
#include <TRandom3.h>

#include "Framework/ASoA.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/AnalysisTask.h"
#include "Framework/O2DatabasePDGPlugin.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/runDataProcessing.h"

#include "Common/Core/TrackSelection.h"
#include "Common/Core/TrackSelectionDefaults.h"

#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"

#include "PWGJE/Core/FastJetUtilities.h"
#include "PWGJE/Core/JetFinder.h"
#include "PWGJE/Core/JetFindingUtilities.h"
#include "PWGJE/DataModel/Jet.h"

#include "PWGJE/Core/JetDerivedDataUtilities.h"

#include "EventFiltering/filterTables.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

struct JetSpectraChargedTask {

  HistogramRegistry registry;

  Configurable<float> selectedJetsRadius{"selectedJetsRadius", 0.4, "resolution parameter for histograms without radius"};
  Configurable<std::string> eventSelections{"eventSelections", "sel8", "choose event selection"};
  Configurable<float> vertexZCut{"vertexZCut", 10.0f, "Accepted z-vertex range"};
  Configurable<float> centralityMin{"centralityMin", -999.0, "minimum centrality"};
  Configurable<float> centralityMax{"centralityMax", 999.0, "maximum centrality"};
  Configurable<std::vector<double>> jetRadii{"jetRadii", std::vector<double>{0.4}, "jet resolution parameters"};
  Configurable<float> trackEtaMin{"trackEtaMin", -0.9, "minimum eta acceptance for tracks"};
  Configurable<float> trackEtaMax{"trackEtaMax", 0.9, "maximum eta acceptance for tracks"};
  Configurable<float> trackPtMin{"trackPtMin", 0.15, "minimum pT acceptance for tracks"};
  Configurable<float> trackPtMax{"trackPtMax", 100.0, "maximum pT acceptance for tracks"};
  Configurable<std::string> trackSelections{"trackSelections", "globalTracks", "set track selections"};
  Configurable<float> pTHatMaxMCD{"pTHatMaxMCD", 999.0, "maximum fraction of hard scattering for jet acceptance in detector MC"};
  Configurable<float> pTHatMaxMCP{"pTHatMaxMCP", 999.0, "maximum fraction of hard scattering for jet acceptance in particle MC"};
  Configurable<float> pTHatExponent{"pTHatExponent", 6.0, "exponent of the event weight for the calculation of pTHat"};
  Configurable<double> jetPtMax{"jetPtMax", 200., "set jet pT bin max"};
  Configurable<float> jetEtaMin{"jetEtaMin", -99.0, "minimum jet pseudorapidity"};
  Configurable<float> jetEtaMax{"jetEtaMax", 99.0, "maximum jet pseudorapidity"};
  Configurable<int> nBinsEta{"nBinsEta", 200, "number of bins for eta axes"};
  Configurable<float> jetAreaFractionMin{"jetAreaFractionMin", -99.0, "used to make a cut on the jet areas"};
  Configurable<float> leadingConstituentPtMin{"leadingConstituentPtMin", -99.0, "minimum pT selection on jet constituent"};
  Configurable<float> leadingConstituentPtMax{"leadingConstituentPtMax", 9999.0, "maximum pT selection on jet constituent"};
  Configurable<float> randomConeR{"randomConeR", 0.4, "size of random Cone for estimating background fluctuations"};
  Configurable<float> randomConeLeadJetDeltaR{"randomConeLeadJetDeltaR", -99.0, "min distance between leading jet axis and random cone (RC) axis; if negative, min distance is set to automatic value of R_leadJet+R_RC "};
  Configurable<bool> checkMcCollisionIsMatched{"checkMcCollisionIsMatched", false, "0: count whole MCcollisions, 1: select MCcollisions which only have their correspond collisions"};
  Configurable<int> trackOccupancyInTimeRangeMax{"trackOccupancyInTimeRangeMax", 999999, "maximum occupancy of tracks in neighbouring collisions in a given time range; only applied to reconstructed collisions (data and mcd jets), not mc collisions (mcp jets)"};
  Configurable<int> trackOccupancyInTimeRangeMin{"trackOccupancyInTimeRangeMin", -999999, "minimum occupancy of tracks in neighbouring collisions in a given time range; only applied to reconstructed collisions (data and mcd jets), not mc collisions (mcp jets)"};

  int eventSelection = -1;
  int trackSelection = -1;

  std::vector<double> jetPtBins;
  std::vector<double> jetPtBinsRhoAreaSub;

  void init(o2::framework::InitContext&)
  {
    eventSelection = jetderiveddatautilities::initialiseEventSelection(static_cast<std::string>(eventSelections));
    trackSelection = jetderiveddatautilities::initialiseTrackSelection(static_cast<std::string>(trackSelections));

    auto jetPtTemp = 0.0;
    jetPtBins.push_back(jetPtTemp);
    jetPtBinsRhoAreaSub.push_back(jetPtTemp);
    while (jetPtTemp < jetPtMax) {
      if (jetPtTemp < 100.0) {
        jetPtTemp += 1.0;
        jetPtBins.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(-jetPtTemp);
      } else if (jetPtTemp < 200.0) {
        jetPtTemp += 5.0;
        jetPtBins.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(-jetPtTemp);
      } else {
        jetPtTemp += 10.0;
        jetPtBins.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(jetPtTemp);
        jetPtBinsRhoAreaSub.push_back(-jetPtTemp);
      }
    }
    std::sort(jetPtBinsRhoAreaSub.begin(), jetPtBinsRhoAreaSub.end());

    AxisSpec jetPtAxis = {jetPtBins, "#it{p}_{T} (GeV/#it{c})"};
    AxisSpec jetPtAxisRhoAreaSub = {jetPtBinsRhoAreaSub, "#it{p}_{T} (GeV/#it{c})"};

    AxisSpec jetEtaAxis = {nBinsEta, jetEtaMin, jetEtaMax, "#eta"};
    AxisSpec trackEtaAxis = {nBinsEta, trackEtaMin, trackEtaMax, "#eta"};

    if (doprocessJetsData || doprocessJetsMCD || doprocessJetsMCDWeighted) {
      registry.add("h_jet_pt", "jet pT;#it{p}_{T,jet} (GeV/#it{c});entries", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_jet_eta", "jet #eta;#eta_{jet};entries", {HistType::kTH1F, {jetEtaAxis}});
      registry.add("h_jet_phi", "jet #varphi;#varphi_{jet};entries", {HistType::kTH1F, {{160, -1.0, 7.}}});
      registry.add("h2_centrality_jet_pt", "centrality vs #it{p}_{T,jet}; centrality; #it{p}_{T,jet} (GeV/#it{c})", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetPtAxis}});
      registry.add("h2_centrality_jet_eta", "centrality vs #eta_{jet}; centrality; #eta_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetEtaAxis}});
      registry.add("h2_centrality_jet_phi", "centrality vs #varphi_{jet}; centrality; #varphi_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {160, -1.0, 7.}}});
      registry.add("h2_centrality_jet_ntracks", "centrality vs N_{jet tracks}; centrality; N_{jet tracks}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_jet_area", "jet #it{p}_{T,jet} vs. Area_{jet}; #it{p}_{T,jet} (GeV/#it{c}); Area_{jet}", {HistType::kTH2F, {jetPtAxis, {150., 0., 1.5}}});
      registry.add("h2_jet_pt_jet_ntracks", "jet #it{p}_{T,jet} vs. N_{jet tracks}; #it{p}_{T,jet} (GeV/#it{c}); N_{jet, tracks}", {HistType::kTH2F, {jetPtAxis, {200., -0.5, 199.5}}});
      registry.add("h2_jet_pt_tracks_pt", "#it{p}_{T,jet} vs. constituent #it{p}_{T, tracks}; #it{p}_{T,jet} (GeV/#it{c}); #it{p}_{T, tracks} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, {200., 0., 200.}}});
      registry.add("h2_jet_pt_tracks_eta", "#it{p}_{T,jet} vs. constituent #eta_{tracks}; #it{p}_{T,jet} (GeV/#it{c}); #eta_{tracks}",{HistType::kTH2F, {jetPtAxis, trackEtaAxis}});
      registry.add("h2_jet_pt_tracks_phi", "#it{p}_{T,jet} vs. constituent #varphi_{tracks}; #it{p}_{T,jet} (GeV/#it{c}); #varphi_{tracks}", {HistType::kTH2F, {jetPtAxis, {160, -1.0, 7.}}});
      registry.add("h3_jet_pt_jet_eta_jet_phi", "jet #it{p}_{T,jet} vs. #eta_{jet} vs. #varphi_{jet}; #it{p}_{T,jet} (GeV/#it{c}); #eta_{jet}; #varphi_{jet}", {HistType::kTH3F, {jetPtAxis, jetEtaAxis, {160, -1.0, 7.}}});
      registry.add("h_jet_phat", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
      registry.add("h_jet_ptcut", "p_{T} cut;p_{T,jet} (GeV/#it{c});N;entries", {HistType::kTH2F, {{300, 0, 300}, {20, 0, 5}}});
      registry.add("h_jet_phat_weighted", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
    }

    if (doprocessJetsRhoAreaSubData || doprocessJetsRhoAreaSubMCD) {

      registry.add("h_jet_pt_rhoareasubtracted", "jet pT;#it{p}_{T,jet} (GeV/#it{c});entries", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h2_centrality_jet_pt_rhoareasubtracted", "centrality vs #it{p}_{T,jet}; centrality; #it{p}_{T,jet} (GeV/#it{c})", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetPtAxisRhoAreaSub}});
      registry.add("h2_centrality_jet_eta_rhoareasubtracted", "centrality vs #eta_{jet}; centrality; #eta_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetEtaAxis}});
      registry.add("h2_centrality_jet_phi_rhoareasubtracted", "centrality vs #varphi_{jet}; centrality; #varphi_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {160, -1.0, 7.}}});
      registry.add("h2_centrality_jet_ntracks_rhoareasubtracted", "centrality vs N_{jet tracks}; centrality; N_{jet tracks}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_jet_area_rhoareasubtracted", "jet #it{p}_{T,jet} vs. Area_{jet}; #it{p}_{T,jet} (GeV/#it{c}); Area_{jet}", {HistType::kTH2F, {jetPtAxis, {150., 0., 1.5}}});
      registry.add("h2_jet_pt_jet_ntracks_rhoareasubtracted", "jet #it{p}_{T,jet} vs. N_{jet tracks}; #it{p}_{T,jet} (GeV/#it{c}); N_{jet, tracks}", {HistType::kTH2F, {jetPtAxis, {200., -0.5, 199.5}}});
      registry.add("h2_jet_pt_jet_corr_pt_rhoareasubtracted", "jet #it{p}_{T,jet} vs. #it{p}_{T,corr}; #it{p}_{T,jet} (GeV/#it{c});  #it{p}_{T,corr} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, jetPtAxis}});
      registry.add("h3_jet_pt_jet_eta_jet_phi_rhoareasubtracted", "jet #it{p}_{T,jet} vs. #eta_{jet} vs. #varphi_{jet}; #it{p}_{T,jet} (GeV/#it{c}); #eta_{jet}; #varphi_{jet}", {HistType::kTH3F, {jetPtAxis, jetEtaAxis, {160, -1.0, 7.}}});
      registry.add("h3_centrality_occupancy_jet_pt_rhoareasubtracted", "centrality; occupancy; #it{p}_{T,jet} (GeV/#it{c})", {HistType::kTH3F, {{120, -10.0, 110.0}, {60, 0, 30000}, jetPtAxisRhoAreaSub}});
    }

    if (doprocessEvtWiseConstSubJetsData || doprocessEvtWiseConstSubJetsMCD) {
      registry.add("h_jet_pt_eventwiseconstituentsubtracted", "jet pT;#it{p}_{T,jet} (GeV/#it{c});entries", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_jet_eta_eventwiseconstituentsubtracted", "jet #eta;#eta_{jet};entries", {HistType::kTH1F, {jetEtaAxis}});
      registry.add("h_jet_phi_eventwiseconstituentsubtracted", "jet #varphi;#varphi_{jet};entries", {HistType::kTH1F, {{160, -1.0, 7.}}});
      registry.add("h_jet_ntracks_eventwiseconstituentsubtracted", "jet N tracks;N_{jet tracks};entries", {HistType::kTH1F, {{200, -0.5, 199.5}}});
      registry.add("h2_centrality_jet_pt_eventwiseconstituentsubtracted", "centrality vs #it{p}_{T,jet}; centrality; #it{p}_{T,jet} (GeV/#it{c})", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetPtAxis}});
      registry.add("h2_centrality_jet_eta_eventwiseconstituentsubtracted", "centrality vs #eta_{jet}; centrality; #eta_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, jetEtaAxis}});
      registry.add("h2_centrality_jet_phi_eventwiseconstituentsubtracted", "centrality vs #varphi_{jet}; centrality; #varphi_{jet}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {160, -1.0, 7.}}});
      registry.add("h2_centrality_jet_ntracks_eventwiseconstituentsubtracted", "centrality vs N_{jet tracks}; centrality; N_{jet tracks}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {200, -0.5, 199.5}}});
    }

    if (doprocessRho) {
      registry.add("h2_centrality_ntracks", "; centrality; N_{tracks};", {HistType::kTH2F, {{1100, 0., 110.0}, {10000, 0.0, 10000.0}}});
      registry.add("h2_ntracks_rho", "; N_{tracks}; #it{rho} (GeV/area);", {HistType::kTH2F, {{10000, 0.0, 10000.0}, {400, 0.0, 400.0}}});
      registry.add("h2_ntracks_rhom", "; N_{tracks}; #it{rho}_{m} (GeV/area);", {HistType::kTH2F, {{10000, 0.0, 10000.0}, {100, 0.0, 100.0}}});
      registry.add("h2_centrality_rho", "; centrality; #it{rho} (GeV/area);", {HistType::kTH2F, {{1100, 0., 110.}, {400, 0., 400.0}}});
      registry.add("h2_centrality_rhom", ";centrality; #it{rho}_{m} (GeV/area)", {HistType::kTH2F, {{1100, 0., 110.}, {100, 0., 100.0}}});
    }

    if (doprocessRandomConeData || doprocessRandomConeMCD) {
      registry.add("h2_centrality_rhorandomcone", "; centrality; #it{p}_{T,random cone} - #it{area, random cone} * #it{rho} (GeV/c);", {HistType::kTH2F, {{1100, 0., 110.}, {800, -400.0, 400.0}}});
      registry.add("h2_centrality_rhorandomconerandomtrackdirection", "; centrality; #it{p}_{T,random cone} - #it{area, random cone} * #it{rho} (GeV/c);", {HistType::kTH2F, {{1100, 0., 110.}, {800, -400.0, 400.0}}});
      registry.add("h2_centrality_rhorandomconewithoutleadingjet", "; centrality; #it{p}_{T,random cone} - #it{area, random cone} * #it{rho} (GeV/c);", {HistType::kTH2F, {{1100, 0., 110.}, {800, -400.0, 400.0}}});
      registry.add("h2_centrality_rhorandomconerandomtrackdirectionwithoutoneleadingjets", "; centrality; #it{p}_{T,random cone} - #it{area, random cone} * #it{rho} (GeV/c);", {HistType::kTH2F, {{1100, 0., 110.}, {800, -400.0, 400.0}}});
      registry.add("h2_centrality_rhorandomconerandomtrackdirectionwithouttwoleadingjets", "; centrality; #it{p}_{T,random cone} - #it{area, random cone} * #it{rho} (GeV/c);", {HistType::kTH2F, {{1100, 0., 110.}, {800, -400.0, 400.0}}});
    }

    if (doprocessJetsMCP || doprocessJetsMCPWeighted) {
      registry.add("h_jet_pt_part", "jet pT;#it{p}_{T,jet}^{part}(GeV/#it{c});entries", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_jet_eta_part", "jet #eta;#eta_{jet}^{part};entries", {HistType::kTH1F, {jetEtaAxis}});
      registry.add("h_jet_phi_part", "jet #varphi;#varphi_{jet}^{part};entries", {HistType::kTH1F, {{160, -1.0, 7.}}});
      registry.add("h_jet_ntracks_part", "jet N tracks;N_{jet tracks}^{part};entries", {HistType::kTH1F, {{200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_part_tracks_pt_part", "#it{p}_{T,jet}^{part} vs. constituent #it{p}_{T, tracks}; #it{p}_{T,jet}^{part} (GeV/#it{c}); #it{p}_{T, tracks} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, {200., 0., 200.}}});
      registry.add("h2_jet_pt_part_tracks_eta_part", "#it{p}_{T,jet}^{part} vs. constituent #eta_{tracks}; #it{p}_{T,jet}^{part} (GeV/#it{c}); #eta_{tracks}",{HistType::kTH2F, {jetPtAxis, trackEtaAxis}});
      registry.add("h2_jet_pt_part_tracks_phi_part", "#it{p}_{T,jet}^{part} vs. constituent #varphi_{tracks}; #it{p}_{T,jet}^{part} (GeV/#it{c}); #varphi_{tracks}", {HistType::kTH2F, {jetPtAxis, {160, -1.0, 7.}}});
      registry.add("h_jet_phat_part", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
      registry.add("h_jet_ptcut_part", "p_{T} cut;p_{T,jet}^{part} (GeV/#it{c});N;entries", {HistType::kTH2F, {{300, 0, 300}, {20, 0, 5}}});
      registry.add("h_jet_phat_part_weighted", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
    }

    if (doprocessJetsMCPMCDMatched || doprocessJetsMCPMCDMatchedWeighted || doprocessJetsSubMatched) {
      registry.add("h2_jet_pt_tag_jet_pt_base_matchedgeo", "#it{p}_{T,jet}^{tag} (GeV/#it{c});#it{p}_{T,jet}^{base} (GeV/#it{c})", {HistType::kTH2F, {{300, 0., 300.}, {300, 0., 300.}}});
      registry.add("h2_jet_eta_tag_jet_eta_base_matchedgeo", "#eta_{jet}^{tag};#eta_{jet}^{base}", {HistType::kTH2F, {jetEtaAxis, jetEtaAxis}});
      registry.add("h2_jet_phi_tag_jet_phi_base_matchedgeo", "#varphi_{jet}^{tag};#varphi_{jet}^{base}", {HistType::kTH2F, {{160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h2_jet_ntracks_tag_jet_ntracks_base_matchedgeo", "N_{jet tracks}^{tag};N_{jet tracks}^{base}", {HistType::kTH2F, {{200, -0.5, 199.5}, {200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_tag_jet_pt_base_diff_matchedgeo", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#it{p}_{T,jet}^{tag} (GeV/#it{c}) - #it{p}_{T,jet}^{base} (GeV/#it{c})) / #it{p}_{T,jet}^{tag} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 2.0}}});
      registry.add("h2_jet_pt_tag_jet_eta_base_diff_matchedgeo", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#eta_{jet}^{tag} - #eta_{jet}^{base}) / #eta_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h2_jet_pt_tag_jet_phi_base_diff_matchedgeo", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#varphi_{jet}^{tag} - #varphi_{jet}^{base}) / #varphi_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedgeo", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #eta_{jet}^{tag}; #eta_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, jetEtaAxis, jetEtaAxis}});
      registry.add("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedgeo", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #varphi_{jet}^{tag}; #varphi_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, {160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedgeo", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); N_{jet tracks}^{tag}; N_{jet tracks}^{base}", {HistType::kTH3F, {jetPtAxis, {200, -0.5, 199.5}, {200, -0.5, 199.5}}});

      registry.add("h2_jet_pt_tag_jet_pt_base_matchedpt", "#it{p}_{T,jet}^{tag} (GeV/#it{c});#it{p}_{T,jet}^{base} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, jetPtAxis}});
      registry.add("h2_jet_eta_tag_jet_eta_base_matchedpt", "#eta_{jet}^{tag};#eta_{jet}^{base}", {HistType::kTH2F, {jetEtaAxis, jetEtaAxis}});
      registry.add("h2_jet_phi_tag_jet_phi_base_matchedpt", "#varphi_{jet}^{tag};#varphi_{jet}^{base}", {HistType::kTH2F, {{160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h2_jet_ntracks_tag_jet_ntracks_base_matchedpt", "N_{jet tracks}^{tag};N_{jet tracks}^{base}", {HistType::kTH2F, {{200, -0.5, 199.5}, {200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_tag_jet_pt_base_diff_matchedpt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#it{p}_{T,jet}^{tag} (GeV/#it{c}) - #it{p}_{T,jet}^{base} (GeV/#it{c})) / #it{p}_{T,jet}^{tag} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h2_jet_pt_tag_jet_eta_base_diff_matchedpt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#eta_{jet}^{tag} - #eta_{jet}^{base}) / #eta_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h2_jet_pt_tag_jet_phi_base_diff_matchedpt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#varphi_{jet}^{tag} - #varphi_{jet}^{base}) / #varphi_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedpt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #eta_{jet}^{tag}; #eta_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, jetEtaAxis, jetEtaAxis}});
      registry.add("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedpt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #varphi_{jet}^{tag}; #varphi_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, {160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedpt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); N_{jet tracks}^{tag}; N_{jet tracks}^{base}", {HistType::kTH3F, {jetPtAxis, {200, -0.5, 199.5}, {200, -0.5, 199.5}}});

      registry.add("h2_jet_pt_tag_jet_pt_base_matchedgeopt", "#it{p}_{T,jet}^{tag} (GeV/#it{c});#it{p}_{T,jet}^{base} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, jetPtAxis}});
      registry.add("h2_jet_eta_tag_jet_eta_base_matchedgeopt", "#eta_{jet}^{tag};#eta_{jet}^{base}", {HistType::kTH2F, {jetEtaAxis, jetEtaAxis}});
      registry.add("h2_jet_phi_tag_jet_phi_base_matchedgeopt", "#varphi_{jet}^{tag};#varphi_{jet}^{base}", {HistType::kTH2F, {{160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h2_jet_ntracks_tag_jet_ntracks_base_matchedgeopt", "N_{jet tracks}^{tag};N_{jet tracks}^{base}", {HistType::kTH2F, {{200, -0.5, 199.5}, {200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_tag_jet_pt_base_diff_matchedgeopt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#it{p}_{T,jet}^{tag} (GeV/#it{c}) - #it{p}_{T,jet}^{base} (GeV/#it{c})) / #it{p}_{T,jet}^{tag} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h2_jet_pt_tag_jet_eta_base_diff_matchedgeopt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#eta_{jet}^{tag} - #eta_{jet}^{base}) / #eta_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h2_jet_pt_tag_jet_phi_base_diff_matchedgeopt", "#it{p}_{T,jet}^{tag} (GeV/#it{c}); (#varphi_{jet}^{tag} - #varphi_{jet}^{base}) / #varphi_{jet}^{tag}", {HistType::kTH2F, {jetPtAxis, {1000, -5.0, 5.0}}});
      registry.add("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedgeopt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #eta_{jet}^{tag}; #eta_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, jetEtaAxis, jetEtaAxis}});
      registry.add("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedgeopt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); #varphi_{jet}^{tag}; #varphi_{jet}^{base}", {HistType::kTH3F, {jetPtAxis, {160, -1.0, 7.}, {160, -1.0, 7.}}});
      registry.add("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedgeopt", ";#it{p}_{T,jet}^{tag} (GeV/#it{c}); N_{jet tracks}^{tag}; N_{jet tracks}^{base}", {HistType::kTH3F, {jetPtAxis, {200, -0.5, 199.5}, {200, -0.5, 199.5}}});
      registry.add("h3_ptcut_jet_pt_tag_jet_pt_base_matchedgeo", "N;#it{p}_{T,jet}^{tag} (GeV/#it{c});#it{p}_{T,jet}^{base} (GeV/#it{c})", {HistType::kTH3F, {{20, 0., 5.}, {300, 0., 300.}, {300, 0., 300.}}});
    }
    if (doprocessCollisions) {
      registry.add("h_collisions", "event status;event status;entries", {HistType::kTH1F, {{4, 0.0, 4.0}}});
      registry.add("h2_centrality_collisions", "centrality vs collisions; centrality; collisions", {HistType::kTH2F, {{1200, -10.0, 110.0}, {4, 0.0, 4.0}}});
      registry.add("h_collisions_vertexZ", "position of collision ;#it{Z} (cm)", {HistType::kTH1F, {{300, -15.0, 15.0}}});
       if (doprocessCollisionsWeighted) {
        registry.add("h_collisions_weighted", "event status;event status;entries", {HistType::kTH1F, {{4, 0.0, 4.0}}});
      }
    }

    if (doprocessTracks || doprocessTracksWeighted) {
      registry.add("h2_centrality_track_pt", "centrality vs track pT ; centrality; #it{p}_{T,track} (GeV/#it{c})", {HistType::kTH2F, {{1200, -10.0, 110.0}, {200, 0., 200.}}});
      registry.add("h2_centrality_track_eta", "centrality vs track #eta; centrality; #eta_{track}", {HistType::kTH2F, {{1200, -10.0, 110.0}, trackEtaAxis}});
      registry.add("h2_centrality_track_phi", "centrality vs track #varphi ; centrality;; #varphi_{track}", {HistType::kTH2F, {{1200, -10.0, 110.0}, {160, -1.0, 7.}}});
      registry.add("h2_track_pt_track_dcaxy", "track pT vs track DCA_{xy}; #it{p}_{T,track} (GeV/#it{c}); track DCA_{xy}", {HistType::kTH2F, {{20, 0., 100.}, {200, -0.15, 0.15}}});
      registry.add("h3_track_pt_track_eta_track_phi", "track pT vs track #eta vs track #varphi; #it{p}_{T,track} (GeV/#it{c}); #eta_{track}; #varphi_{track}", {HistType::kTH3F, {{200, 0., 200.}, trackEtaAxis, {160, -1.0, 7.}}});
      registry.add("h3_centrality_occupancy_track_pt", "centrality vs occupancy vs track #it{p}_{T}; centrality; occupancy #it{p}_{T,track} (GeV/#it{c})", {HistType::kTH3F, {{1200, -10.0, 110.0}, {60, 0, 30000}, {200, 0., 200.}}});
    }
    if (doprocessTracksSub) {
      registry.add("h3_centrality_track_pt_track_phi_eventwiseconstituentsubtracted", "centrality vs track pT vs track #varphi; centrality; #it{p}_{T,track} (GeV/#it{c}); #varphi_{track}", {HistType::kTH3F, {{1200, -10.0, 110.0}, {200, 0., 200.}, {160, -1.0, 7.}}});
      registry.add("h3_centrality_track_pt_track_eta_eventwiseconstituentsubtracted", "centrality vs track pT vs track #eta; centrality; #it{p}_{T,track} (GeV/#it{c}); #eta_{track}", {HistType::kTH3F, {{1200, -10.0, 110.0}, {200, 0., 200.}, trackEtaAxis}});
      registry.add("h3_track_pt_track_eta_track_phi_eventwiseconstituentsubtracted", "track pT vs track #eta vs track #varphi; #it{p}_{T,track} (GeV/#it{c}); #eta_{track}; #varphi_{track}", {HistType::kTH3F, {{200, 0., 200.}, trackEtaAxis, {160, -1.0, 7.}}});
    }

    if (doprocessMCCollisionsWeighted) {
      AxisSpec weightAxis = {{VARIABLE_WIDTH, 1e-13, 1e-12, 1e-11, 1e-10, 1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1, 1.0, 10.0}, "weights"};
      registry.add("h_collision_eventweight_part", "event weight;event weight;entries", {HistType::kTH1F, {weightAxis}});
    }
  }

  Filter trackCuts = (aod::jtrack::pt >= trackPtMin && aod::jtrack::pt < trackPtMax && aod::jtrack::eta > trackEtaMin && aod::jtrack::eta < trackEtaMax);
  Filter trackSubCuts = (aod::jtracksub::pt >= trackPtMin && aod::jtracksub::pt < trackPtMax && aod::jtracksub::eta > trackEtaMin && aod::jtracksub::eta < trackEtaMax);
  Filter eventCuts = (nabs(aod::jcollision::posZ) < vertexZCut && aod::jcollision::centrality >= centralityMin && aod::jcollision::centrality < centralityMax);
  Filter particlecuts = (aod::jmcparticle::pt >= trackPtMin && aod::jmcparticle::pt < trackPtMax && aod::jmcparticle::eta > trackEtaMin && aod::jmcparticle::eta < trackEtaMax);
  
  PresliceUnsorted<soa::Filtered<aod::JetCollisionsMCD>> CollisionsPerMCPCollision = aod::jmccollisionlb::mcCollisionId;
  // Preslice<aod::JetTracksMCD> tracksPerJCollision = o2::aod::jtracks::collisionId
  
  template <typename TTracks, typename TJets>
  bool isAcceptedJet(TJets const& jet)
  {

    if (jetAreaFractionMin > -98.0) {
      if (jet.area() < jetAreaFractionMin * M_PI * (jet.r() / 100.0) * (jet.r() / 100.0)) {
        return false;
      }
    }
    bool checkConstituentPt = true;
    bool checkConstituentMinPt = (leadingConstituentPtMin > -98.0);
    bool checkConstituentMaxPt = (leadingConstituentPtMax < 9998.0);
    if (!checkConstituentMinPt && !checkConstituentMaxPt) {
      checkConstituentPt = false;
    }

    if (checkConstituentPt) {
      bool isMinLeadingConstituent = !checkConstituentMinPt;
      bool isMaxLeadingConstituent = true;

      for (const auto& constituent : jet.template tracks_as<TTracks>()) {
        double pt = constituent.pt();

        if (checkConstituentMinPt && pt >= leadingConstituentPtMin) {
          isMinLeadingConstituent = true;
        }
        if (checkConstituentMaxPt && pt > leadingConstituentPtMax) {
          isMaxLeadingConstituent = false;
        }
      }
      return isMinLeadingConstituent && isMaxLeadingConstituent;
    }

    return true;
  }

  template <typename TTracks, typename TJets>
  bool trackIsInJet(TTracks const& track, TJets const& jet)
  {
    for (auto const& constituentId : jet.tracksIds()) {
      if (constituentId == track.globalIndex()) {
        return true;
      }
    }
    return false;
  }

  template <typename TJets>
  void fillHistograms(TJets const& jet, float centrality, float occupancy, float weight = 1.0)
  {

    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCD * pTHat) {
      return;
    }
    registry.fill(HIST("h_jet_phat"), pTHat);
    registry.fill(HIST("h_jet_phat_weighted"), pTHat, weight);

    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      registry.fill(HIST("h_jet_pt"), jet.pt(), weight);
      registry.fill(HIST("h_jet_eta"), jet.eta(), weight);
      registry.fill(HIST("h_jet_phi"), jet.phi(), weight);
      registry.fill(HIST("h2_centrality_jet_pt"), centrality, jet.pt(), weight);
      registry.fill(HIST("h2_centrality_jet_eta"), centrality, jet.eta(), weight);
      registry.fill(HIST("h2_centrality_jet_phi"), centrality, jet.phi(), weight);
      registry.fill(HIST("h2_centrality_jet_ntracks"), centrality, jet.tracksIds().size(), weight);
      registry.fill(HIST("h2_jet_pt_jet_area"), jet.pt(), jet.area(), weight);
      registry.fill(HIST("h2_jet_pt_jet_ntracks"), jet.pt(), jet.tracksIds(.size(), weight);
      registry.fill(HIST("h3_jet_pt_jet_eta_jet_phi"), jet.pt(), jet.eta(), jet.phi(), weight);
    }


    for (auto& constituent : jet.template tracks_as<aod::JetTracks>()) {

      registry.fill(HIST("h2_jet_pt_track_pt"), jet.pt(), constituent.pt(), weight);
      registry.fill(HIST("h2_jet_pt_track_eta"), jet.pt(), constituent.eta(), weight);
      registry.fill(HIST("h2_jet_pt_track_phi"), jet.pt(), constituent.phi(), weight);
    }
  }

  template <typename TJets>
  void fillRhoAreaSubtractedHistograms(TJets const& jet, float centrality, float occupancy, float rho, float weight = 1.0)
  {
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      registry.fill(HIST("h_jet_pt_rhoareasubtracted"), jet.pt() - (rho * jet.area()), weight);
      registry.fill(HIST("h2_centrality_jet_pt_rhoareasubtracted"), centrality, jet.pt() - (rho * jet.area()), weight);
      registry.fill(HIST("h2_jet_pt_jet_corr_pt_rhoareasubtracted"), jet.pt(), jet.pt() - (rho * jet.area()), weight);
      registry.fill(HIST("h3_jet_pt_jet_eta_jet_phi_rhoareasubtracted"), jet.pt() - (rho * jet.area()), jet.eta(). jet.phi(), weight);
      registry.fill(HIST("h3_centrality_occupancy_jet_pt_rhoareasubtracted"), centrality, occupancy, jet.pt() - (rho * jet.area()), weight);
      if (jet.pt() - (rho * jet.area()) > 0) {
        registry.fill(HIST("h2_centrality_jet_eta_rhoareasubtracted"), centrality, jet.eta(), weight);
        registry.fill(HIST("h2_centrality_jet_phi_rhoareasubtracted"), centrality, jet.phi(), weight);
        registry.fill(HIST("h2_centrality_jet_ntracks_rhoareasubtracted"), centrality, jet.tracksIds().size(), weight);
        registry.fill(HIST("h2_jet_pt_jet_area_rhoareasubtracted"), jet.pt() - (rho * jet.area()), jet.area(), weight);
        registry.fill(HIST("h2_jet_pt_jet_ntracks_rhoareasubtracted"), jet.pt() - (rho * jet.area()), jet.tracksIds().size(), weight);
      }
    }
  }

  template <typename TJets>
  void fillEventWiseConstituentSubtractedHistograms(TJets const& jet, float centrality, float weight = 1.0)
  {
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      registry.fill(HIST("h_jet_pt_eventwiseconstituentsubtracted"), jet.pt(), weight);
      registry.fill(HIST("h_jet_eta_eventwiseconstituentsubtracted"), jet.eta(), weight);
      registry.fill(HIST("h_jet_phi_eventwiseconstituentsubtracted"), jet.phi(), weight);
      registry.fill(HIST("h_jet_ntracks_eventwiseconstituentsubtracted"), jet.tracksIds().size(), weight);
      registry.fill(HIST("h2_centrality_jet_pt_eventwiseconstituentsubtracted"), centrality, jet.pt(), weight);
      registry.fill(HIST("h2_centrality_jet_eta_eventwiseconstituentsubtracted"), centrality, jet.eta(), weight);
      registry.fill(HIST("h2_centrality_jet_phi_eventwiseconstituentsubtracted"), centrality, jet.phi(), weight);
      registry.fill(HIST("h2_centrality_jet_ntracks_eventwiseconstituentsubtracted"), centrality, jet.tracksIds().size(), weight);
    }


    for (auto& constituent : jet.template tracks_as<aod::JetTracksSub>()) {

      registry.fill(HIST("h3_jet_r_jet_pt_track_pt_eventwiseconstituentsubtracted"), jet.r() / 100.0, jet.pt(), constituent.pt(), weight);
      registry.fill(HIST("h3_jet_r_jet_pt_track_eta_eventwiseconstituentsubtracted"), jet.r() / 100.0, jet.pt(), constituent.eta(), weight);
      registry.fill(HIST("h3_jet_r_jet_pt_track_phi_eventwiseconstituentsubtracted"), jet.r() / 100.0, jet.pt(), constituent.phi(), weight);
    }
  }

  template <typename TJets>
  void fillMCPHistograms(TJets const& jet, float weight = 1.0)
  {

    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCP * pTHat) {
      return;
    }
    registry.fill(HIST("h_jet_phat_part"), pTHat);
    registry.fill(HIST("h_jet_phat_part_weighted"), pTHat, weight);

    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      registry.fill(HIST("h_jet_pt_part"), jet.pt(), weight);
      registry.fill(HIST("h_jet_eta_part"), jet.eta(), weight);
      registry.fill(HIST("h_jet_phi_part"), jet.phi(), weight);
      registry.fill(HIST("h_jet_ntracks_part"), jet.tracksIds().size(), weight);
      
      for (auto& constituent : jet.template tracks_as<aod::JetParticles>()) {
        registry.fill(HIST("h2_jet_pt_part_tracks_pt_part"), jet.pt(), constituent.pt(), weight);
        registry.fill(HIST("h2_jet_pt_part_tracks_eta_part"), jet.pt(), constituent.eta(), weight);
        registry.fill(HIST("h2_jet_pt_part_tracks_phi_part"), jet.pt(), constituent.phi(), weight);
      }
    }
  }

 template <typename TJetsBase, typename TJetsTag>
  void fillMatchedHistograms(TJetsBase const& jetBase, float weight = 1.0)
  {
    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jetBase.pt() > pTHatMaxMCD * pTHat) {
      return;
    }

    if (jetBase.has_matchedJetGeo()) {
      for (auto& jetTag : jetBase.template matchedJetGeo_as<std::decay_t<TJetsTag>>()) {
        if (jetTag.pt() > pTHatMaxMCP * pTHat){
          continue;
        }
        if (jetBase.r() == round(selectedJetsRadius * 100.0f)) {
          registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_matchedgeo"), jetTag.pt(), jetBase.pt(), weight);
          registry.fill(HIST("h2_jet_eta_tag_jet_eta_base_matchedgeo"), jetTag.eta(), jetBase.eta(), weight);
          registry.fill(HIST("h2_jet_phi_tag_jet_phi_base_matchedgeo"), jetTag.phi(), jetBase.phi(), weight);
          registry.fill(HIST("h2_jet_ntracks_tag_jet_ntracks_base_matchedgeo"), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
          registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_diff_matchedgeo"), jetTag.pt(), (jetTag.pt() - jetBase.pt()) / jetTag.pt(), weight);
          registry.fill(HIST("h2_jet_pt_tag_jet_eta_base_diff_matchedgeo"), jetTag.pt(), (jetTag.eta() - jetBase.eta()) / jetTag.eta(), weight);
          registry.fill(HIST("h2_jet_pt_tag_jet_phi_base_diff_matchedgeo"), jetTag.pt(), (jetTag.phi() - jetBase.phi()) / jetTag.phi(), weight);
          registry.fill(HIST("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedgeo"), jetTag.pt(), jetTag.eta(), jetBase.eta(), weight);
          registry.fill(HIST("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedgeo"), jetTag.pt(), jetTag.phi(), jetBase.phi(), weight);
          registry.fill(HIST("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedgeo"), jetTag.pt(), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
          for (int N = 1; N < 21; N++) {
            if (jetBase.pt() < N * 0.25 * pTHat && jetTag.pt() < N * 0.25 * pTHat) {
              registry.fill(HIST("h3_ptcut_jet_pt_tag_jet_pt_base_matchedgeo"), N * 0.25, jetTag.pt(), jetBase.pt(), weight);
              }
            }
        }
       }
    }
    if (jetBase.has_matchedJetPt()) {
      for (auto& jetTag : jetBase.template matchedJetPt_as<std::decay_t<TJetsTag>>()) {
        if (jetTag.pt() > pTHatMaxMCP * pTHat) {
          continue;
        }
        if (jetBase.r() == round(selectedJetsRadius * 100.0f)) {
            registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_matchedpt"), jetTag.pt(), jetBase.pt(), weight);
            registry.fill(HIST("h2_jet_eta_tag_jet_eta_base_matchedpt"), jetTag.eta(), jetBase.eta(), weight);
            registry.fill(HIST("h2_jet_phi_tag_jet_phi_base_matchedpt"), jetTag.phi(), jetBase.phi(), weight);
            registry.fill(HIST("h2_jet_ntracks_tag_jet_ntracks_base_matchedpt"), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_diff_matchedpt"), jetTag.pt(), (jetTag.pt() - jetBase.pt()) / jetTag.pt(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_eta_base_diff_matchedpt"), jetTag.pt(), (jetTag.eta() - jetBase.eta()) / jetTag.eta(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_phi_base_diff_matchedpt"), jetTag.pt(), (jetTag.phi() - jetBase.phi()) / jetTag.phi(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedpt"), jetTag.pt(), jetTag.eta(), jetBase.eta(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedpt"), jetTag.pt(), jetTag.phi(), jetBase.phi(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedpt"), jetTag.pt(), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
        }
      }
    }

    if (jetBase.has_matchedJetGeo() && jetBase.has_matchedJetPt()) {

      for (auto& jetTag : jetBase.template matchedJetGeo_as<std::decay_t<TJetsTag>>()) {
        if (jetTag.pt() > pTHatMaxMCP * pTHat) {
          continue;
        }
        if (jetBase.r() == round(selectedJetsRadius * 100.0f)) {
            if (jetBase.template matchedJetGeo_first_as<std::decay_t<TJetsTag>>().globalIndex() == jetBase.template matchedJetPt_first_as<std::decay_t<TJetsTag>>().globalIndex()) { // not a good way to do this
            registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_matchedgeopt"), jetTag.pt(), jetBase.pt(), weight);
            registry.fill(HIST("h2_jet_eta_tag_jet_eta_base_matchedgeopt"), jetTag.eta(), jetBase.eta(), weight);
            registry.fill(HIST("h2_jet_phi_tag_jet_phi_base_matchedgeopt"), jetTag.phi(), jetBase.phi(), weight);
            registry.fill(HIST("h2_jet_ntracks_tag_jet_ntracks_base_matchedgeopt"), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_pt_base_diff_matchedgeopt"), jetTag.pt(), (jetTag.pt() - jetBase.pt()) / jetTag.pt(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_eta_base_diff_matchedgeopt"), jetTag.pt(), (jetTag.eta() - jetBase.eta()) / jetTag.eta(), weight);
            registry.fill(HIST("h2_jet_pt_tag_jet_phi_base_diff_matchedgeopt"), jetTag.pt(), (jetTag.phi() - jetBase.phi()) / jetTag.phi(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_eta_tag_jet_eta_base_matchedgeopt"), jetTag.pt(), jetTag.eta(), jetBase.eta(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_phi_tag_jet_phi_base_matchedgeopt"), jetTag.pt(), jetTag.phi(), jetBase.phi(), weight);
            registry.fill(HIST("h3_jet_pt_tag_jet_ntracks_tag_jet_ntracks_base_matchedgeopt"), jetTag.pt(), jetTag.tracksIds().size(), jetBase.tracksIds().size(), weight);
          }
        }
      }
    }
  }

  template <typename TCollisions, typename TTracks>
  void fillTrackHistograms(TCollisions const& collision, TTracks const& tracks, float weight = 1.0)
  {
    for (auto const& track : tracks) {
      if (!jetderiveddatautilities::selectTrack(track, trackSelection)) {
        continue;
      }
      registry.fill(HIST("h2_centrality_track_pt"), collision.centrality(), track.pt(), weight);
      registry.fill(HIST("h2_centrality_track_eta"), collision.centrality(), track.eta(), weight);
      registry.fill(HIST("h2_centrality_track_phi"), collision.centrality(), track.phi(), weight);
      registry.fill(HIST("h2_track_pt_track_dcaxy"), track.pt(), track.dcaXY(), weight);
      registry.fill(HIST("h3_track_pt_track_eta_track_phi"), track.pt(), track.eta(), track.phi(), weight);
      registry.fill(HIST("h3_centrality_occupancy_track_pt"), collision.centrality(), collision.trackOccupancyInTimeRange(), track.pt(), weight);
    }
  }

  template <typename TCollisions, typename TJets, typename TTracks>
  void fillrandomCone(TCollisions const& collision, TJets const& jets, TTracks const& tracks)
  {
      // fill nothing for now 
  }

  void processJetsData(soa::Filtered<aod::JetCollisions>::iterator const& collision, soa::Join<aod::ChargedJets, aod::ChargedJetConstituents> const& jets, aod::JetTracks const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillHistograms(jet, collision.centrality(), collision.trackOccupancyInTimeRange());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsData, "jet finder QA data", false);

  void processJetsRhoAreaSubData(soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator const& collision,
                                 soa::Join<aod::ChargedJets, aod::ChargedJetConstituents> const& jets,
                                 aod::JetTracks const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillRhoAreaSubtractedHistograms(jet, collision.centrality(), collision.trackOccupancyInTimeRange(), collision.rho());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsRhoAreaSubData, "jet finder QA for rho-area subtracted jets", false);

  void processJetsRhoAreaSubMCD(soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator const& collision,
                                soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents> const& jets,
                                aod::JetTracks const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillRhoAreaSubtractedHistograms(jet, collision.centrality(), collision.trackOccupancyInTimeRange(), collision.rho());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsRhoAreaSubMCD, "jet finder QA for rho-area subtracted mcd jets", false);

  void processEvtWiseConstSubJetsData(soa::Filtered<aod::JetCollisions>::iterator const& collision, soa::Join<aod::ChargedEventWiseSubtractedJets, aod::ChargedEventWiseSubtractedJetConstituents> const& jets, aod::JetTracksSub const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracksSub>(jet)) {
        continue;
      }
      fillEventWiseConstituentSubtractedHistograms(jet, collision.centrality());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processEvtWiseConstSubJetsData, "jet finder QA for eventwise constituent-subtracted jets data", false);

  void processEvtWiseConstSubJetsMCD(soa::Filtered<aod::JetCollisions>::iterator const& collision, soa::Join<aod::ChargedMCDetectorLevelEventWiseSubtractedJets, aod::ChargedMCDetectorLevelEventWiseSubtractedJetConstituents> const& jets, aod::JetTracksSub const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracksSub>(jet)) {
        continue;
      }
      fillEventWiseConstituentSubtractedHistograms(jet, collision.centrality());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processEvtWiseConstSubJetsMCD, "jet finder QA for eventwise constituent-subtracted mcd jets", false);

  void processJetsSubMatched(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                             soa::Join<aod::ChargedJets, aod::ChargedJetConstituents, aod::ChargedJetsMatchedToChargedEventWiseSubtractedJets> const& jets,
                             soa::Join<aod::ChargedEventWiseSubtractedJets, aod::ChargedEventWiseSubtractedJetConstituents, aod::ChargedEventWiseSubtractedJetsMatchedToChargedJets> const&,
                             aod::JetTracks const&, aod::JetTracksSub const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (const auto& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillMatchedHistograms<soa::Join<aod::ChargedJets, aod::ChargedJetConstituents, aod::ChargedJetsMatchedToChargedEventWiseSubtractedJets>::iterator, soa::Join<aod::ChargedEventWiseSubtractedJets, aod::ChargedEventWiseSubtractedJetConstituents, aod::ChargedEventWiseSubtractedJetsMatchedToChargedJets>>(jet);
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsSubMatched, "jet finder QA matched unsubtracted and constituent subtracted jets", false);

  void processJetsMCD(soa::Filtered<aod::JetCollisions>::iterator const& collision, soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents> const& jets, aod::JetTracks const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillHistograms(jet, collision.centrality(), collision.trackOccupancyInTimeRange());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCD, "jet finder QA mcd", false);

  void processJetsMCDWeighted(soa::Filtered<aod::JetCollisions>::iterator const& collision, soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetEventWeights> const& jets, aod::JetTracks const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      double pTHat = 10. / (std::pow(jet.eventWeight(), 1.0 / pTHatExponent));
      for (int N = 1; N < 21; N++) {
        if (jet.pt() < N * 0.25 * pTHat && jet.r() == round(selectedJetsRadius * 100.0f)) {
          registry.fill(HIST("h_jet_ptcut"), jet.pt(), N * 0.25, jet.eventWeight());
        }
      }
      fillHistograms(jet, collision.centrality(), collision.trackOccupancyInTimeRange(), jet.eventWeight());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCDWeighted, "jet finder QA mcd with weighted events", false);

  void processJetsMCP(soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents>::iterator const& jet, aod::JetParticles const&, aod::JetMcCollisions const&, soa::Filtered<aod::JetCollisionsMCD> const& collisions)
  {
    if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
      return;
    }
    if (!isAcceptedJet<aod::JetParticles>(jet)) {
      return;
    }
    if (checkMcCollisionIsMatched) {
      auto collisionspermcpjet = collisions.sliceBy(CollisionsPerMCPCollision, jet.mcCollisionId());
      if (collisionspermcpjet.size() >= 1 && jetderiveddatautilities::selectCollision(collisionspermcpjet.begin(), eventSelection)) {
        fillMCPHistograms(jet);
      }
    } else {
      fillMCPHistograms(jet);
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCP, "jet finder QA mcp", false);

  void processJetsMCPWeighted(soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetEventWeights>::iterator const& jet, aod::JetParticles const&, aod::JetMcCollisions const&, soa::Filtered<aod::JetCollisionsMCD> const& collisions)
  {
    if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
      return;
    }
    if (!isAcceptedJet<aod::JetParticles>(jet)) {
      return;
    }
    double pTHat = 10. / (std::pow(jet.eventWeight(), 1.0 / pTHatExponent));
    for (int N = 1; N < 21; N++) {
      if (jet.pt() < N * 0.25 * pTHat && jet.r() == round(selectedJetsRadius * 100.0f)) {
        registry.fill(HIST("h_jet_ptcut_part"), jet.pt(), N * 0.25, jet.eventWeight());
      }
    }
    if (checkMcCollisionIsMatched) {
      auto collisionspermcpjet = collisions.sliceBy(CollisionsPerMCPCollision, jet.mcCollisionId());
      if (collisionspermcpjet.size() >= 1) {
        fillMCPHistograms(jet, jet.eventWeight());
      }
    } else {
      fillMCPHistograms(jet, jet.eventWeight());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCPWeighted, "jet finder QA mcp with weighted events", false);

  void processJetsMCPMCDMatched(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                                soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetsMatchedToChargedMCParticleLevelJets> const& mcdjets,
                                soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetsMatchedToChargedMCDetectorLevelJets> const&,
                                aod::JetTracks const&, aod::JetParticles const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (const auto& mcdjet : mcdjets) {
      if (!jetfindingutilities::isInEtaAcceptance(mcdjet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(mcdjet)) {
        continue;
      }
      fillMatchedHistograms<soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetsMatchedToChargedMCParticleLevelJets>::iterator, soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetsMatchedToChargedMCDetectorLevelJets>>(mcdjet);
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCPMCDMatched, "jet finder QA matched mcp and mcd", false);

  void processJetsMCPMCDMatchedWeighted(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                                        soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetsMatchedToChargedMCParticleLevelJets, aod::ChargedMCDetectorLevelJetEventWeights> const& mcdjets,
                                        soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetsMatchedToChargedMCDetectorLevelJets, aod::ChargedMCParticleLevelJetEventWeights> const&,
                                        aod::JetTracks const&, aod::JetParticles const&)
  {
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (const auto& mcdjet : mcdjets) {
      if (!jetfindingutilities::isInEtaAcceptance(mcdjet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(mcdjet)) {
        continue;
      }
      fillMatchedHistograms<soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetsMatchedToChargedMCParticleLevelJets, aod::ChargedMCDetectorLevelJetEventWeights>::iterator, soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetsMatchedToChargedMCDetectorLevelJets, aod::ChargedMCParticleLevelJetEventWeights>>(mcdjet, mcdjet.eventWeight());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processJetsMCPMCDMatchedWeighted, "jet finder QA matched mcp and mcd with weighted events", false);

  void processMCCollisionsWeighted(aod::JetMcCollision const& collision)
  {
    registry.fill(HIST("h_collision_eventweight_part"), collision.weight());
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processMCCollisionsWeighted, "collision QA for weighted events", false);

  void processCollisions(soa::Filtered<aod::JetCollisions>::iterator const& collision)
  {
    registry.fill(HIST("h_collisions"), 0.5);
    registry.fill(HIST("h2_centrality_collisions"), collision.centrality(), 0.5);
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    registry.fill(HIST("h_collisions"), 1.5);
    registry.fill(HIST("h2_centrality_collisions"), collision.centrality(), 1.5);
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    registry.fill(HIST("h_collisions"), 2.5);
    registry.fill(HIST("h2_centrality_collisions"), collision.centrality(), 2.5);
    registry.fill(HIST("h_collisions_vertexZ"), collision.posZ());
      
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processCollisions, "collision QA for events", true);
  
  void processCollisionsWeighted(soa::Join<aod::JetCollisions, aod::JMcCollisionLbs>::iterator const& collision,
                                 aod::JetMcCollisions const&)
  {
    float eventWeight = collision.mcCollision().weight();
    registry.fill(HIST("h_collisions"), 0.5);
    registry.fill(HIST("h_collisions_weighted"), 0.5, eventWeight);
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    registry.fill(HIST("h_collisions"), 1.5);
    registry.fill(HIST("h_collisions_weighted"), 1.5, eventWeight);
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    registry.fill(HIST("h_collisions"), 2.5);
    registry.fill(HIST("h_collisions_weighted"), 2.5, eventWeight);
    registry.fill(HIST("h_collisions_vertexZ"), collision.posZ(), eventWeight);
      
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processCollisionsWeighted, "collision for weighted events", true);

  void processmcparticles(aod::JetMcCollision const& mcCollision,
                          soa::SmallGroups<aod::JetCollisionsMCD> const& collisions,
                          soa::Filtered<aod::JetParticles> const& mcparticles)
  {
      registry.fill(HIST("h_mccollisions_part"), 0.5);
      registry.fill(HIST("h2_centrality_mccollisions_part"), collisions.begin(), 0.5);
      if(!abs(mcCollision.posZ() < vertexZCut)){
        return
      }
      if(!abs(collisions.size() < 1)){
        return
      }

      bool hasSel8Coll = false;
      bool centralityCheck = false;
      if(jetderiveddatautilities::selectionCollision(collisions.begin(), eventselection)){
        hasSel8Coll = true;
      }
      if(!check){
      }
      registry.fill(HIST("h_mccollisions_part"), 1.5);
      registry.fill(HIST("h2_centrality_mccollisions_part"), collisions.begin(), 1.5);
      fillparticleHistograms(collision.begin(), mcparticles);
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processmcparticles, "QA for charged mc particles", false);


  void processTracks(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                     soa::Filtered<soa::Join<aod::JetTracks, aod::JTrackExtras>> const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    fillTrackHistograms(collision, tracks);
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processTracks, "QA for charged tracks", false);

  void processTracksWeighted(soa::Join<aod::JetCollisions, aod::JMcCollisionLbs>::iterator const& collision,
                             aod::JetMcCollisions const&,
                             soa::Filtered<soa::Join<aod::JetTracks, aod::JTrackExtras>> const& tracks)
  {
    float eventWeight = collision.mcCollision().weight();
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    fillTrackHistograms(collision, tracks, eventWeight);
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processTracksWeighted, "QA for charged tracks weighted", false);

  void processTracksSub(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                        soa::Filtered<aod::JetTracksSub> const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& track : tracks) {
      registry.fill(HIST("h3_centrality_track_pt_track_phi_eventwiseconstituentsubtracted"), collision.centrality(), track.pt(), track.phi());
      registry.fill(HIST("h3_centrality_track_pt_track_eta_eventwiseconstituentsubtracted"), collision.centrality(), track.pt(), track.eta());
      registry.fill(HIST("h3_track_pt_track_eta_track_phi_eventwiseconstituentsubtracted"), track.pt(), track.eta(), track.phi());
    }
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processTracksSub, "QA for charged event-wise embedded subtracted tracks", false);

  void processRho(soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator const& collision, soa::Filtered<aod::JetTracks> const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelection)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    int nTracks = 0;
    for (auto const& track : tracks) {
      if (jetderiveddatautilities::selectTrack(track, trackSelection)) {
        nTracks++;
      }
    }
    registry.fill(HIST("h2_centrality_ntracks"), collision.centrality(), nTracks);
    registry.fill(HIST("h2_ntracks_rho"), nTracks, collision.rho());
    registry.fill(HIST("h2_ntracks_rhom"), nTracks, collision.rhoM());
    registry.fill(HIST("h2_centrality_rho"), collision.centrality(), collision.rho());
    registry.fill(HIST("h2_centrality_rhom"), collision.centrality(), collision.rhoM());
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processRho, "QA for rho-area subtracted jets", false);

  void processRandomConeData(soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator const& collision, soa::Join<aod::ChargedJets, aod::ChargedJetConstituents> const& jets, soa::Filtered<aod::JetTracks> const& tracks)
  {
    fillrandomCone(collision, jets, tracks);
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processRandomConeData, "QA for random cone estimation of background fluctuations in data", false);

  void processRandomConeMCD(soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator const& collision, soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents> const& jets, soa::Filtered<aod::JetTracks> const& tracks)
  {
    fillrandomCone(collision, jets, tracks);
  }
  PROCESS_SWITCH(JetSpectraChargedTask, processRandomConeMCD, "QA for random cone estimation of background fluctuations in mcd", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc) { return WorkflowSpec{adaptAnalysisTask<JetSpectraChargedTask>(cfgc, TaskName{"jet-charged-spectra"})}; }
