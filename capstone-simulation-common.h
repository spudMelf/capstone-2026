#ifndef CAPSTONE_SIMULATION_COMMON_H
#define CAPSTONE_SIMULATION_COMMON_H

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>

namespace capstone
{

using namespace ns3;

constexpr double NOT_APPLICABLE = std::numeric_limits<double>::quiet_NaN();

class WindowInsertionLossModel : public PropagationLossModel
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::capstone::WindowInsertionLossModel")
                .SetParent<PropagationLossModel>()
                .SetGroupName("Propagation")
                .AddConstructor<WindowInsertionLossModel>()
                .AddAttribute("Loss",
                              "Insertion loss of the cryostat window in dB",
                              DoubleValue(0.0),
                              MakeDoubleAccessor(&WindowInsertionLossModel::m_lossDb),
                              MakeDoubleChecker<double>(0.0));
        return tid;
    }

    void SetLossDb(double lossDb)
    {
        m_lossDb = lossDb;
    }

  private:
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel>,
                         Ptr<MobilityModel>) const override
    {
        return txPowerDbm - m_lossDb;
    }

    int64_t DoAssignStreams(int64_t) override
    {
        return 0;
    }

    double m_lossDb{0.0};
};

struct Scenario
{
    std::string experimentId;
    std::string propagationModel;
    uint64_t runNumber;
    uint32_t randomSeed{1};
    std::string ns3Version{"3.47"};

    double carrierFrequencyGHz{7.5};
    double distanceM{1.0};
    double txEnvironmentTemperatureK{290.0};
    double rxEnvironmentTemperatureK{290.0};
    std::string antennaType{"Isotropic"};
    std::string antennaAlignment{"NotSpecified"};
    double txWindowSeparationM{NOT_APPLICABLE};
    double rxWindowSeparationM{NOT_APPLICABLE};
    double windowInsertionLossDb{0.0};
    double txPowerDbm{0.0};
    double txAntennaGainDbi{0.0};
    double rxAntennaGainDbi{0.0};
    double channelBandwidthMHz{20.0};
    double receiverNoiseFigureDb{7.0};
    double noiseTemperatureK{290.0};
    double receiverSensitivityDbm{-101.0};

    double referenceDistanceM{NOT_APPLICABLE};
    double referenceLossDb{NOT_APPLICABLE};
    double pathLossExponent{NOT_APPLICABLE};
    double friisSystemLossLinear{NOT_APPLICABLE};

    std::string mobilityModel{"ConstantPositionMobilityModel"};
    double rxSpeedMps{0.0};
    std::string phyMode{"OfdmRate6Mbps"};
    double phyDataRateMbps{6.0};
    double offeredRateMbps{1.0};
    uint32_t packetPayloadBytes{1024};
    double trafficDurationS{10.0};
    bool interferenceEnabled{false};
    uint32_t interfererCount{0};
};

struct Results
{
    double pathLossDb;
    double predictedRxPowerDbm;
    double noisePowerDbm;
    double snrDb;
    double sinrDb;
    double propagationDelayNs;
    uint64_t txPackets;
    uint64_t rxPackets;
    double pdrPercent;
    double packetErrorProbability;
    double throughputMbps;
    double meanOneWayLatencyMs;
};

inline Results
RunScenario(const Scenario& scenario, Ptr<PropagationLossModel> propagationLoss)
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
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    channel->SetPropagationLossModel(propagationLoss);
    channel->SetPropagationDelayModel(propagationDelay);

    YansWifiPhyHelper phy;
    phy.SetChannel(channel);
    phy.Set("TxPowerStart", DoubleValue(scenario.txPowerDbm));
    phy.Set("TxPowerEnd", DoubleValue(scenario.txPowerDbm));
    phy.Set("TxGain", DoubleValue(scenario.txAntennaGainDbi));
    phy.Set("RxGain", DoubleValue(scenario.rxAntennaGainDbi));
    phy.Set("RxNoiseFigure", DoubleValue(scenario.receiverNoiseFigureDb));
    phy.Set("RxSensitivity", DoubleValue(scenario.receiverSensitivityDbm));

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
    const double propagatedRxPowerDbm = propagationLoss->CalcRxPower(
        scenario.txPowerDbm + scenario.txAntennaGainDbi,
        txMobility,
        rxMobility);
    const double predictedRxPowerDbm = propagatedRxPowerDbm + scenario.rxAntennaGainDbi;
    const double pathLossDb = scenario.txPowerDbm + scenario.txAntennaGainDbi +
                              scenario.rxAntennaGainDbi - predictedRxPowerDbm;

    constexpr double boltzmannConstant = 1.380649e-23;
    const double channelBandwidthHz = scenario.channelBandwidthMHz * 1e6;
    const double thermalNoiseW =
        boltzmannConstant * scenario.noiseTemperatureK * channelBandwidthHz;
    const double noisePowerDbm =
        10.0 * std::log10(thermalNoiseW * 1000.0) + scenario.receiverNoiseFigureDb;
    const double snrDb = predictedRxPowerDbm - noisePowerDbm;
    const double sinrDb = snrDb;
    const double propagationDelayNs =
        propagationDelay->GetDelay(txMobility, rxMobility).GetSeconds() * 1e9;

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

    return {pathLossDb,
            predictedRxPowerDbm,
            noisePowerDbm,
            snrDb,
            sinrDb,
            propagationDelayNs,
            txPackets,
            rxPackets,
            pdrPercent,
            packetErrorProbability,
            throughputMbps,
            meanOneWayLatencyMs};
}

inline void
WriteHeader(std::ostream& csv)
{
    csv << "experiment_id,propagation_model,run_number,random_seed,ns3_version,"
           "carrier_frequency_ghz,distance_m,tx_environment_temperature_k,"
           "rx_environment_temperature_k,antenna_type,antenna_alignment,"
           "tx_window_separation_m,rx_window_separation_m,window_insertion_loss_db,"
           "tx_x_m,tx_y_m,tx_z_m,rx_x_m,rx_y_m,rx_z_m,"
           "mobility_model,rx_speed_mps,tx_power_dbm,tx_antenna_gain_dbi,"
           "rx_antenna_gain_dbi,channel_bandwidth_mhz,receiver_noise_figure_db,"
           "noise_temperature_k,receiver_sensitivity_dbm,reference_distance_m,"
           "reference_loss_db,path_loss_exponent,friis_system_loss_linear,phy_mode,"
           "phy_data_rate_mbps,offered_rate_mbps,packet_payload_bytes,traffic_duration_s,"
           "interference_enabled,interferer_count,path_loss_db,predicted_rx_power_dbm,"
           "noise_power_dbm,snr_db,interference_power_dbm,sinr_db,propagation_delay_ns,"
           "tx_packets,rx_packets,pdr_percent,packet_error_probability,throughput_mbps,"
           "mean_one_way_latency_ms\n";
}

inline void
WriteOptional(std::ostream& csv, double value)
{
    if (!std::isnan(value))
    {
        csv << value;
    }
}

inline void
WriteRow(std::ostream& csv, const Scenario& scenario, const Results& results)
{
    csv << scenario.experimentId << ',' << scenario.propagationModel << ','
        << scenario.runNumber << ',' << scenario.randomSeed << ',' << scenario.ns3Version << ','
        << scenario.carrierFrequencyGHz << ',' << scenario.distanceM << ','
        << scenario.txEnvironmentTemperatureK << ',' << scenario.rxEnvironmentTemperatureK << ','
        << scenario.antennaType << ',' << scenario.antennaAlignment << ',';
    WriteOptional(csv, scenario.txWindowSeparationM);
    csv << ',';
    WriteOptional(csv, scenario.rxWindowSeparationM);
    csv << ',' << scenario.windowInsertionLossDb << ",0,0,0,"
        << scenario.distanceM << ",0,0," << scenario.mobilityModel << ',' << scenario.rxSpeedMps
        << ',' << scenario.txPowerDbm << ',' << scenario.txAntennaGainDbi << ','
        << scenario.rxAntennaGainDbi << ',' << scenario.channelBandwidthMHz << ','
        << scenario.receiverNoiseFigureDb << ',' << scenario.noiseTemperatureK << ','
        << scenario.receiverSensitivityDbm << ',';
    WriteOptional(csv, scenario.referenceDistanceM);
    csv << ',';
    WriteOptional(csv, scenario.referenceLossDb);
    csv << ',';
    WriteOptional(csv, scenario.pathLossExponent);
    csv << ',';
    WriteOptional(csv, scenario.friisSystemLossLinear);
    csv << ',' << scenario.phyMode << ',' << scenario.phyDataRateMbps << ','
        << scenario.offeredRateMbps << ',' << scenario.packetPayloadBytes << ','
        << scenario.trafficDurationS << ',' << std::boolalpha << scenario.interferenceEnabled << ','
        << scenario.interfererCount << ',' << results.pathLossDb << ','
        << results.predictedRxPowerDbm << ',' << results.noisePowerDbm << ',' << results.snrDb
        << ",," << results.sinrDb << ',' << results.propagationDelayNs << ',' << results.txPackets
        << ',' << results.rxPackets << ',' << results.pdrPercent << ','
        << results.packetErrorProbability << ',' << results.throughputMbps << ','
        << results.meanOneWayLatencyMs << '\n';
}

inline void
PrintSummary(const Scenario& scenario, const Results& results)
{
    std::cout << std::fixed << std::setprecision(3) << scenario.propagationModel;
    if (!std::isnan(scenario.pathLossExponent))
    {
        std::cout << " n=" << scenario.pathLossExponent;
    }
    std::cout << "  d=" << scenario.distanceM << " m"
              << "  loss=" << results.pathLossDb << " dB"
              << "  Rx=" << results.predictedRxPowerDbm << " dBm"
              << "  SNR=" << results.snrDb << " dB"
              << "  PDR=" << results.pdrPercent << "%\n";
}

} // namespace capstone

#endif
