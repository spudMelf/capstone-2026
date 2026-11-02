/*
 * Compare the native ns-3 single-element antenna radiation patterns for the
 * planned one-metre cryostat-window link.
 *
 * The four-pattern comparison is normalized to 5 dBi at boresight so it
 * isolates directional shape. The mixed-link transmit-power sweep compares
 * a 5 dBi parabolic transmitter with a 0 dBi isotropic transmitter. The packet
 * simulation uses SpectrumWifiPhy because the ns-3 AntennaModel classes are
 * applied by spectrum-based channels, not by the YansWifiPhy used earlier.
 */

#include "capstone-simulation-common.h"

#include "ns3/antenna-module.h"
#include "ns3/spectrum-module.h"

#include <array>
#include <sstream>
#include <vector>

using namespace ns3;
using namespace capstone;

namespace
{

constexpr double CARRIER_FREQUENCY_GHZ = 7.5;
constexpr double DISTANCE_M = 1.0;
constexpr double TX_POWER_DBM = 0.0;
constexpr double BORESIGHT_GAIN_DBI = 5.0;
constexpr double WINDOW_INSERTION_LOSS_DB = 0.5;
constexpr double PATCH_BEAMWIDTH_DEG = 90.0;
constexpr double MAX_PATTERN_ATTENUATION_DB = 25.0;
constexpr double TWO_RAY_ANTENNA_HEIGHT_M = 0.15;
constexpr double PREAMBLE_MIN_RSSI_DBM = -82.0;
constexpr double PREAMBLE_SNR_THRESHOLD_DB = 4.0;

const std::array<double, 7> POINTING_ERRORS_DEG = {0.0, 15.0, 30.0, 45.0, 60.0, 75.0, 90.0};
const std::array<double, 7> TX_POWER_SWEEP_DBM = {-20.0, -15.0, -10.0, -5.0, 0.0, 5.0, 10.0};
const std::array<double, 2> TX_POWER_POINTING_ERRORS_DEG = {0.0, 60.0};
const std::array<double, 22> MIXED_TX_POWER_SWEEP_DBM = {
    -45.0, -42.0, -40.0, -39.0, -38.0, -37.0, -36.0, -35.0,
    -34.0, -33.0, -32.0, -31.0, -30.0, -28.0, -25.0, -20.0,
    -15.0, -10.0, -5.0, 0.0, 5.0, 10.0};

/** Rotate any native ns-3 AntennaModel around the vertical axis. */
class AzimuthRotatedAntennaModel : public AntennaModel
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::capstone::AzimuthRotatedAntennaModel")
                .SetParent<AntennaModel>()
                .SetGroupName("Antenna")
                .AddConstructor<AzimuthRotatedAntennaModel>();
        return tid;
    }

    void SetBaseModel(Ptr<AntennaModel> baseModel)
    {
        m_baseModel = baseModel;
    }

    void SetYawDegrees(double yawDegrees)
    {
        m_yawRadians = DegreesToRadians(yawDegrees);
    }

    double GetGainDb(Angles angles) override
    {
        NS_ABORT_MSG_IF(!m_baseModel, "A base antenna model must be set");
        angles.SetAzimuth(WrapToPi(angles.GetAzimuth() - m_yawRadians));
        return m_baseModel->GetGainDb(angles);
    }

  private:
    Ptr<AntennaModel> m_baseModel;
    double m_yawRadians{0.0};
};

struct AntennaCase
{
    std::string name;
    double horizontalBeamwidthDeg;
    double verticalBeamwidthDeg;
    double maxAttenuationDb;
    double scalarGainDbi;
};

struct AntennaRunResults
{
    Results link;
    double txPatternGainDbi;
    double rxPatternGainDbi;
};

const std::array<AntennaCase, 4> ANTENNA_CASES = {
    AntennaCase{"Isotropic", 360.0, 360.0, 0.0, BORESIGHT_GAIN_DBI},
    AntennaCase{"Cosine", PATCH_BEAMWIDTH_DEG, PATCH_BEAMWIDTH_DEG,
                MAX_PATTERN_ATTENUATION_DB, 0.0},
    AntennaCase{"Parabolic", PATCH_BEAMWIDTH_DEG, 360.0,
                MAX_PATTERN_ATTENUATION_DB, BORESIGHT_GAIN_DBI},
    AntennaCase{"ThreeGppIndoor", 90.0, 90.0, 25.0, 0.0},
};

Ptr<AntennaModel>
MakeNativeAntenna(const AntennaCase& antennaCase)
{
    if (antennaCase.name == "Isotropic")
    {
        return CreateObject<IsotropicAntennaModel>();
    }
    if (antennaCase.name == "Cosine")
    {
        Ptr<CosineAntennaModel> antenna = CreateObject<CosineAntennaModel>();
        antenna->SetAttribute("HorizontalBeamwidth",
                              DoubleValue(antennaCase.horizontalBeamwidthDeg));
        antenna->SetAttribute("VerticalBeamwidth",
                              DoubleValue(antennaCase.verticalBeamwidthDeg));
        antenna->SetAttribute("MaxGain", DoubleValue(BORESIGHT_GAIN_DBI));
        return antenna;
    }
    if (antennaCase.name == "Parabolic")
    {
        Ptr<ParabolicAntennaModel> antenna = CreateObject<ParabolicAntennaModel>();
        antenna->SetBeamwidth(antennaCase.horizontalBeamwidthDeg);
        antenna->SetAttribute("MaxAttenuation",
                              DoubleValue(antennaCase.maxAttenuationDb));
        return antenna;
    }

    Ptr<ThreeGppAntennaModel> antenna = CreateObject<ThreeGppAntennaModel>();
    antenna->SetRadiationPattern(ThreeGppAntennaModel::RadiationPattern::INDOOR);
    return antenna;
}

Ptr<AntennaModel>
MakeOrientedAntenna(const AntennaCase& antennaCase, double yawDegrees)
{
    Ptr<AzimuthRotatedAntennaModel> antenna =
        CreateObject<AzimuthRotatedAntennaModel>();
    antenna->SetBaseModel(MakeNativeAntenna(antennaCase));
    antenna->SetYawDegrees(yawDegrees);
    return antenna;
}

Ptr<PropagationLossModel>
MakePropagationModel(const std::string& propagationCase)
{
    Ptr<PropagationLossModel> propagation;
    if (propagationCase == "FriisDirectLos")
    {
        Ptr<FriisPropagationLossModel> friis = CreateObject<FriisPropagationLossModel>();
        friis->SetFrequency(CARRIER_FREQUENCY_GHZ * 1e9);
        friis->SetSystemLoss(1.0);
        propagation = friis;
    }
    else
    {
        Ptr<TwoRayGroundPropagationLossModel> twoRay =
            CreateObject<TwoRayGroundPropagationLossModel>();
        twoRay->SetFrequency(CARRIER_FREQUENCY_GHZ * 1e9);
        twoRay->SetSystemLoss(1.0);
        twoRay->SetHeightAboveZ(TWO_RAY_ANTENNA_HEIGHT_M);
        propagation = twoRay;
    }

    Ptr<WindowInsertionLossModel> windowLoss = CreateObject<WindowInsertionLossModel>();
    windowLoss->SetLossDb(WINDOW_INSERTION_LOSS_DB);
    propagation->SetNext(windowLoss);
    return propagation;
}

Scenario
MakeScenario(const AntennaCase& antennaCase,
             const std::string& propagationCase,
             double pointingErrorDeg,
             uint64_t runNumber,
             double txPowerDbm = TX_POWER_DBM)
{
    Scenario scenario;
    std::ostringstream experimentId;
    experimentId << "antenna_" << antennaCase.name << "_error" << pointingErrorDeg;
    scenario.experimentId = experimentId.str();
    scenario.propagationModel = propagationCase;
    scenario.runNumber = runNumber;
    scenario.carrierFrequencyGHz = CARRIER_FREQUENCY_GHZ;
    scenario.distanceM = DISTANCE_M;
    scenario.txEnvironmentTemperatureK = 50.0;
    scenario.rxEnvironmentTemperatureK = 290.0;
    scenario.antennaType = antennaCase.name;
    scenario.antennaAlignment = pointingErrorDeg == 0.0 ? "Boresight" : "SymmetricYawError";
    scenario.txWindowSeparationM = TWO_RAY_ANTENNA_HEIGHT_M;
    scenario.rxWindowSeparationM = TWO_RAY_ANTENNA_HEIGHT_M;
    scenario.windowInsertionLossDb = WINDOW_INSERTION_LOSS_DB;
    scenario.txPowerDbm = txPowerDbm;
    scenario.txAntennaGainDbi = antennaCase.scalarGainDbi;
    scenario.rxAntennaGainDbi = antennaCase.scalarGainDbi;
    scenario.noiseTemperatureK = 290.0;
    return scenario;
}

AntennaRunResults
RunAntennaScenario(const Scenario& scenario,
                   Ptr<PropagationLossModel> propagationLoss,
                   Ptr<AntennaModel> txAntenna,
                   Ptr<AntennaModel> rxAntenna)
{
    RngSeedManager::SetSeed(scenario.randomSeed);
    RngSeedManager::SetRun(scenario.runNumber);
    Ipv4AddressGenerator::Reset();

    NodeContainer nodes;
    nodes.Create(2);

    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    positions->Add(Vector(scenario.distanceM, 0.0, 0.0));
    MobilityHelper mobility;
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantSpeedPropagationDelayModel> propagationDelay =
        CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<MultiModelSpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
    channel->AddPropagationLossModel(propagationLoss);
    channel->SetPropagationDelayModel(propagationDelay);

    SpectrumWifiPhyHelper phy;
    phy.SetChannel(channel);
    phy.Set("TxPowerStart", DoubleValue(scenario.txPowerDbm));
    phy.Set("TxPowerEnd", DoubleValue(scenario.txPowerDbm));
    phy.Set("TxGain", DoubleValue(scenario.txAntennaGainDbi));
    phy.Set("RxGain", DoubleValue(scenario.rxAntennaGainDbi));
    phy.Set("RxNoiseFigure", DoubleValue(scenario.receiverNoiseFigureDb));
    phy.Set("RxSensitivity", DoubleValue(scenario.receiverSensitivityDbm));
    phy.SetPreambleDetectionModel("ns3::ThresholdPreambleDetectionModel",
                                  "MinimumRssi",
                                  DoubleValue(PREAMBLE_MIN_RSSI_DBM),
                                  "Threshold",
                                  DoubleValue(PREAMBLE_SNR_THRESHOLD_DB));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(scenario.phyMode),
                                 "ControlMode",
                                 StringValue(scenario.phyMode));
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    Ptr<WifiNetDevice> txDevice = DynamicCast<WifiNetDevice>(devices.Get(0));
    Ptr<WifiNetDevice> rxDevice = DynamicCast<WifiNetDevice>(devices.Get(1));
    Ptr<SpectrumWifiPhy> txPhy = DynamicCast<SpectrumWifiPhy>(txDevice->GetPhy());
    Ptr<SpectrumWifiPhy> rxPhy = DynamicCast<SpectrumWifiPhy>(rxDevice->GetPhy());
    NS_ABORT_MSG_IF(!txPhy || !rxPhy, "SpectrumWifiPhy installation failed");
    txPhy->SetAntenna(txAntenna);
    rxPhy->SetAntenna(rxAntenna);

    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    constexpr uint16_t destinationPort = 5000;
    PacketSinkHelper sinkHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), destinationPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));

    OnOffHelper senderHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(interfaces.GetAddress(1), destinationPort));
    senderHelper.SetConstantRate(DataRate(static_cast<uint64_t>(scenario.offeredRateMbps * 1e6)),
                                 scenario.packetPayloadBytes);
    ApplicationContainer senderApp = senderHelper.Install(nodes.Get(0));

    constexpr double appStartS = 1.0;
    const double appStopS = appStartS + scenario.trafficDurationS;
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(appStopS + 1.0));
    senderApp.Start(Seconds(appStartS));
    senderApp.Stop(Seconds(appStopS));

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    Ptr<MobilityModel> txMobility = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> rxMobility = nodes.Get(1)->GetObject<MobilityModel>();
    const Angles txAngles(rxMobility->GetPosition(), txMobility->GetPosition());
    const Angles rxAngles(txMobility->GetPosition(), rxMobility->GetPosition());
    const double txPatternGainDbi = txAntenna->GetGainDb(txAngles);
    const double rxPatternGainDbi = rxAntenna->GetGainDb(rxAngles);
    const double propagatedRxPowerDbm = propagationLoss->CalcRxPower(
        scenario.txPowerDbm + scenario.txAntennaGainDbi + txPatternGainDbi,
        txMobility,
        rxMobility);
    const double predictedRxPowerDbm = propagatedRxPowerDbm +
                                       scenario.rxAntennaGainDbi + rxPatternGainDbi;
    const double pathLossDb = scenario.txPowerDbm + scenario.txAntennaGainDbi +
                              txPatternGainDbi + scenario.rxAntennaGainDbi +
                              rxPatternGainDbi - predictedRxPowerDbm;

    constexpr double boltzmannConstant = 1.380649e-23;
    const double thermalNoiseW = boltzmannConstant * scenario.noiseTemperatureK *
                                 scenario.channelBandwidthMHz * 1e6;
    const double noisePowerDbm =
        10.0 * std::log10(thermalNoiseW * 1000.0) + scenario.receiverNoiseFigureDb;
    const double snrDb = predictedRxPowerDbm - noisePowerDbm;
    const double propagationDelayNs =
        propagationDelay->GetDelay(txMobility, rxMobility).GetNanoSeconds();

    Simulator::Stop(Seconds(appStopS + 1.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    Time totalDelay = Seconds(0.0);
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    for (const auto& [flowId, stats] : flowMonitor->GetFlowStats())
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flowId);
        if (tuple.destinationPort == destinationPort)
        {
            txPackets += stats.txPackets;
            rxPackets += stats.rxPackets;
            rxBytes += stats.rxBytes;
            totalDelay += stats.delaySum;
        }
    }

    const double pdrPercent =
        txPackets > 0 ? 100.0 * static_cast<double>(rxPackets) / txPackets : 0.0;
    const double packetErrorProbability =
        txPackets > 0 ? static_cast<double>(txPackets - rxPackets) / txPackets : 0.0;
    const double throughputMbps =
        static_cast<double>(rxBytes) * 8.0 / scenario.trafficDurationS / 1e6;
    const double meanOneWayLatencyMs =
        rxPackets > 0 ? totalDelay.GetMilliSeconds() / static_cast<double>(rxPackets) : 0.0;

    Simulator::Destroy();

    Results results{pathLossDb,
                    predictedRxPowerDbm,
                    noisePowerDbm,
                    snrDb,
                    snrDb,
                    propagationDelayNs,
                    txPackets,
                    rxPackets,
                    pdrPercent,
                    packetErrorProbability,
                    throughputMbps,
                    meanOneWayLatencyMs};
    return {results, txPatternGainDbi, rxPatternGainDbi};
}

void
WriteAntennaHeader(std::ostream& csv)
{
    csv << "experiment_id,propagation_case,reflection_interpretation,antenna_model,"
           "pointing_error_deg,distance_m,carrier_frequency_ghz,boresight_gain_dbi,"
           "horizontal_3db_beamwidth_deg,vertical_3db_beamwidth_deg,max_pattern_attenuation_db,"
           "tx_pattern_gain_dbi,rx_pattern_gain_dbi,total_antenna_gain_db,tx_power_dbm,"
           "window_insertion_loss_db,path_loss_db,predicted_rx_power_dbm,noise_power_dbm,"
           "snr_db,tx_packets,rx_packets,pdr_percent,packet_error_probability,"
           "throughput_mbps,mean_one_way_latency_ms\n";
}

void
WriteAntennaRow(std::ostream& csv,
         const Scenario& scenario,
         const AntennaCase& antennaCase,
         double pointingErrorDeg,
         const AntennaRunResults& results)
{
    const bool twoRay = scenario.propagationModel == "TwoRayGroundExploratory";
    const double totalAntennaGain = scenario.txAntennaGainDbi + results.txPatternGainDbi +
                                    scenario.rxAntennaGainDbi + results.rxPatternGainDbi;
    csv << scenario.experimentId << ',' << scenario.propagationModel << ','
        << (twoRay ? "Ground-plane sensitivity; not a cryostat-wall model" : "Direct LOS only")
        << ',' << antennaCase.name << ',' << pointingErrorDeg << ',' << scenario.distanceM << ','
        << scenario.carrierFrequencyGHz << ',' << BORESIGHT_GAIN_DBI << ','
        << antennaCase.horizontalBeamwidthDeg << ',' << antennaCase.verticalBeamwidthDeg << ','
        << antennaCase.maxAttenuationDb << ',' << results.txPatternGainDbi << ','
        << results.rxPatternGainDbi << ',' << totalAntennaGain << ',' << scenario.txPowerDbm << ','
        << scenario.windowInsertionLossDb << ',' << results.link.pathLossDb << ','
        << results.link.predictedRxPowerDbm << ',' << results.link.noisePowerDbm << ','
        << results.link.snrDb << ',' << results.link.txPackets << ',' << results.link.rxPackets
        << ',' << results.link.pdrPercent << ',' << results.link.packetErrorProbability << ','
        << results.link.throughputMbps << ',' << results.link.meanOneWayLatencyMs << '\n';
}

void
WriteMixedAntennaHeader(std::ostream& csv)
{
    csv << "experiment_id,propagation_model,carrier_frequency_ghz,distance_m,tx_power_dbm,"
           "tx_antenna_model,tx_peak_gain_dbi,tx_horizontal_3db_beamwidth_deg,"
           "tx_max_pattern_attenuation_db,tx_pointing_error_deg,tx_pattern_gain_dbi,"
           "rx_antenna_model,rx_gain_dbi,rx_pointing_error_deg,rx_pattern_gain_dbi,"
           "receiver_sensitivity_dbm,preamble_min_rssi_dbm,preamble_snr_threshold_db,"
           "window_insertion_loss_db,path_loss_db,predicted_rx_power_dbm,noise_power_dbm,"
           "snr_db,tx_packets,rx_packets,pdr_percent,packet_error_probability,"
           "throughput_mbps,mean_one_way_latency_ms\n";
}

void
WriteMixedAntennaRow(std::ostream& csv,
                     const Scenario& scenario,
                     const AntennaCase& txAntennaCase,
                     const AntennaRunResults& results)
{
    csv << scenario.experimentId << ',' << scenario.propagationModel << ','
        << scenario.carrierFrequencyGHz << ',' << scenario.distanceM << ','
        << scenario.txPowerDbm << ',' << txAntennaCase.name << ','
        << scenario.txAntennaGainDbi << ','
        << txAntennaCase.horizontalBeamwidthDeg << ',' << txAntennaCase.maxAttenuationDb
        << ",0," << results.txPatternGainDbi << ",Isotropic,"
        << scenario.rxAntennaGainDbi << ",0," << results.rxPatternGainDbi << ','
        << scenario.receiverSensitivityDbm << ',' << PREAMBLE_MIN_RSSI_DBM << ','
        << PREAMBLE_SNR_THRESHOLD_DB << ','
        << scenario.windowInsertionLossDb << ',' << results.link.pathLossDb << ','
        << results.link.predictedRxPowerDbm << ',' << results.link.noisePowerDbm << ','
        << results.link.snrDb << ',' << results.link.txPackets << ',' << results.link.rxPackets
        << ',' << results.link.pdrPercent << ',' << results.link.packetErrorProbability << ','
        << results.link.throughputMbps << ',' << results.link.meanOneWayLatencyMs << '\n';
}

} // namespace

int
main()
{
    const std::string outputFile =
        "scratch/capstone/results/antenna-model-comparison-results.csv";
    const std::string txPowerOutputFile =
        "scratch/capstone/results/antenna-model-tx-power-results.csv";
    const std::string mixedAntennaOutputFile =
        "scratch/capstone/results/parabolic-tx-isotropic-rx-results.csv";
    const std::string mixedTxPowerOutputFile =
        "scratch/capstone/results/parabolic-vs-isotropic-tx-power-results.csv";
    std::ofstream csv(outputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!csv.is_open(), "Could not open output file: " << outputFile);
    WriteAntennaHeader(csv);
    std::ofstream txPowerCsv(txPowerOutputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!txPowerCsv.is_open(),
                    "Could not open output file: " << txPowerOutputFile);
    WriteAntennaHeader(txPowerCsv);
    std::ofstream mixedAntennaCsv(mixedAntennaOutputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!mixedAntennaCsv.is_open(),
                    "Could not open output file: " << mixedAntennaOutputFile);
    WriteMixedAntennaHeader(mixedAntennaCsv);
    std::ofstream mixedTxPowerCsv(mixedTxPowerOutputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!mixedTxPowerCsv.is_open(),
                    "Could not open output file: " << mixedTxPowerOutputFile);
    WriteMixedAntennaHeader(mixedTxPowerCsv);

    uint64_t runNumber = 1;
    for (const AntennaCase& antennaCase : ANTENNA_CASES)
    {
        for (double pointingErrorDeg : POINTING_ERRORS_DEG)
        {
            const Scenario scenario = MakeScenario(antennaCase,
                                                   "FriisDirectLos",
                                                   pointingErrorDeg,
                                                   runNumber++);
            Ptr<AntennaModel> txAntenna =
                MakeOrientedAntenna(antennaCase, pointingErrorDeg);
            Ptr<AntennaModel> rxAntenna =
                MakeOrientedAntenna(antennaCase, 180.0 - pointingErrorDeg);
            const AntennaRunResults results = RunAntennaScenario(
                scenario,
                MakePropagationModel(scenario.propagationModel),
                txAntenna,
                rxAntenna);
            WriteAntennaRow(csv, scenario, antennaCase, pointingErrorDeg, results);
            std::cout << antennaCase.name << " error=" << pointingErrorDeg
                      << " deg  Rx=" << results.link.predictedRxPowerDbm
                      << " dBm  PDR=" << results.link.pdrPercent << "%\n";
        }

        // Reflection sensitivity check at boresight. At this one-metre geometry
        // ns-3's TwoRayGround model is expected to use its short-range Friis branch.
        const Scenario reflectionScenario = MakeScenario(antennaCase,
                                                          "TwoRayGroundExploratory",
                                                          0.0,
                                                          runNumber++);
        const AntennaRunResults reflectionResults = RunAntennaScenario(
            reflectionScenario,
            MakePropagationModel(reflectionScenario.propagationModel),
            MakeOrientedAntenna(antennaCase, 0.0),
            MakeOrientedAntenna(antennaCase, 180.0));
        WriteAntennaRow(csv, reflectionScenario, antennaCase, 0.0, reflectionResults);
    }

    for (double pointingErrorDeg : TX_POWER_POINTING_ERRORS_DEG)
    {
        for (const AntennaCase& antennaCase : ANTENNA_CASES)
        {
            for (double txPowerDbm : TX_POWER_SWEEP_DBM)
            {
                Scenario scenario = MakeScenario(antennaCase,
                                                 "FriisDirectLos",
                                                 pointingErrorDeg,
                                                 runNumber++,
                                                 txPowerDbm);
                std::ostringstream experimentId;
                experimentId << "tx_power_" << antennaCase.name << "_error"
                             << pointingErrorDeg << "_tx" << txPowerDbm;
                scenario.experimentId = experimentId.str();
                const AntennaRunResults results = RunAntennaScenario(
                    scenario,
                    MakePropagationModel(scenario.propagationModel),
                    MakeOrientedAntenna(antennaCase, pointingErrorDeg),
                    MakeOrientedAntenna(antennaCase, 180.0 - pointingErrorDeg));
                WriteAntennaRow(txPowerCsv,
                                scenario,
                                antennaCase,
                                pointingErrorDeg,
                                results);
            }
        }
    }

    const AntennaCase& parabolicTxCase = ANTENNA_CASES[2];
    const AntennaCase& isotropicRxCase = ANTENNA_CASES[0];
    Scenario mixedScenario = MakeScenario(parabolicTxCase,
                                          "FriisDirectLos",
                                          0.0,
                                          runNumber++);
    mixedScenario.experimentId = "parabolic_tx_isotropic_rx_boresight_d1m";
    mixedScenario.antennaType = "ParabolicTx-IsotropicRx";
    mixedScenario.antennaAlignment = "Boresight";
    mixedScenario.txAntennaGainDbi = BORESIGHT_GAIN_DBI;
    mixedScenario.rxAntennaGainDbi = 0.0;
    const AntennaRunResults mixedResults = RunAntennaScenario(
        mixedScenario,
        MakePropagationModel(mixedScenario.propagationModel),
        MakeOrientedAntenna(parabolicTxCase, 0.0),
        MakeOrientedAntenna(isotropicRxCase, 180.0));
    WriteMixedAntennaRow(mixedAntennaCsv,
                         mixedScenario,
                         parabolicTxCase,
                         mixedResults);

    // Compare the selected parabolic transmitter against a 0 dBi isotropic
    // transmitter. The receiver remains isotropic in both cases. Distance is
    // held at the room-scale 1 m baseline while transmit power is swept.
    const std::array<const AntennaCase*, 2> mixedTxPowerCases = {
        &parabolicTxCase,
        &isotropicRxCase};
    for (const AntennaCase* txCase : mixedTxPowerCases)
    {
        const double txPeakGainDbi = txCase->name == "Parabolic" ? BORESIGHT_GAIN_DBI : 0.0;
        for (double txPowerDbm : MIXED_TX_POWER_SWEEP_DBM)
        {
            Scenario scenario = MakeScenario(*txCase,
                                             "FriisDirectLos",
                                             0.0,
                                             runNumber++,
                                             txPowerDbm);
            std::ostringstream experimentId;
            experimentId << txCase->name << "_tx_isotropic_rx_power" << txPowerDbm << "dbm";
            scenario.experimentId = experimentId.str();
            scenario.distanceM = DISTANCE_M;
            scenario.antennaType = txCase->name + "Tx-IsotropicRx";
            scenario.antennaAlignment = "Boresight";
            scenario.txAntennaGainDbi = txPeakGainDbi;
            scenario.rxAntennaGainDbi = 0.0;
            const AntennaRunResults results = RunAntennaScenario(
                scenario,
                MakePropagationModel(scenario.propagationModel),
                MakeOrientedAntenna(*txCase, 0.0),
                MakeOrientedAntenna(isotropicRxCase, 180.0));
            WriteMixedAntennaRow(mixedTxPowerCsv, scenario, *txCase, results);
            std::cout << txCase->name << " Tx, power=" << txPowerDbm
                      << " dBm  Rx=" << results.link.predictedRxPowerDbm
                      << " dBm  PDR=" << results.link.pdrPercent << "%\n";
        }
    }

    std::cout << "Antenna-model comparison saved to " << outputFile << '\n';
    std::cout << "Antenna transmit-power sweep saved to " << txPowerOutputFile << '\n';
    std::cout << "Parabolic-Tx/isotropic-Rx result saved to " << mixedAntennaOutputFile << '\n';
    std::cout << "Parabolic-vs-isotropic transmit-power sweep saved to "
              << mixedTxPowerOutputFile << '\n';
    std::cout << "Mixed link: Rx=" << mixedResults.link.predictedRxPowerDbm
              << " dBm  SNR=" << mixedResults.link.snrDb
              << " dB  PDR=" << mixedResults.link.pdrPercent << "%\n";
    return 0;
}
