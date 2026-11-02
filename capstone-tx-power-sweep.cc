/*
 * Transmit-power sweeps for the Friis and n=4 log-distance channel models.
 * Distance and all other radio, traffic, noise, and receiver settings are fixed.
 */

#include "capstone-simulation-common.h"

#include <array>
#include <sstream>

using namespace ns3;
using namespace capstone;

int
main()
{
    constexpr std::array<double, 7> txPowersDbm = {-20.0, -15.0, -10.0, -5.0,
                                                   0.0,   5.0,   10.0};
    constexpr double distanceM = 5.0;
    constexpr double carrierFrequencyGHz = 7.5;
    constexpr double systemLossLinear = 1.0;
    constexpr double pathLossExponent = 4.0;
    constexpr double referenceDistanceM = 1.0;
    constexpr double referenceLossDb = 49.95;
    const std::string friisOutputFile =
        "scratch/capstone/results/friis-tx-power-results.csv";
    const std::string logDistanceOutputFile =
        "scratch/capstone/results/log-distance-n4-tx-power-results.csv";

    std::ofstream friisCsv(friisOutputFile, std::ios::trunc);
    std::ofstream logDistanceCsv(logDistanceOutputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!friisCsv.is_open(), "Could not open output file: " << friisOutputFile);
    NS_ABORT_MSG_IF(!logDistanceCsv.is_open(),
                    "Could not open output file: " << logDistanceOutputFile);
    WriteHeader(friisCsv);
    WriteHeader(logDistanceCsv);

    uint64_t runNumber = 1;
    for (const double txPowerDbm : txPowersDbm)
    {
        Scenario scenario;
        scenario.propagationModel = "Friis";
        scenario.runNumber = runNumber++;
        scenario.carrierFrequencyGHz = carrierFrequencyGHz;
        scenario.distanceM = distanceM;
        scenario.txPowerDbm = txPowerDbm;
        scenario.friisSystemLossLinear = systemLossLinear;

        std::ostringstream id;
        id << "friis_tx" << txPowerDbm << "dBm_d" << distanceM << "m";
        scenario.experimentId = id.str();

        Ptr<FriisPropagationLossModel> propagationLoss =
            CreateObject<FriisPropagationLossModel>();
        propagationLoss->SetFrequency(carrierFrequencyGHz * 1e9);
        propagationLoss->SetSystemLoss(systemLossLinear);

        const Results results = RunScenario(scenario, propagationLoss);
        WriteRow(friisCsv, scenario, results);
        PrintSummary(scenario, results);
    }

    runNumber = 1;
    for (const double txPowerDbm : txPowersDbm)
    {
        Scenario scenario;
        scenario.propagationModel = "LogDistance";
        scenario.runNumber = runNumber++;
        scenario.distanceM = distanceM;
        scenario.txPowerDbm = txPowerDbm;
        scenario.referenceDistanceM = referenceDistanceM;
        scenario.referenceLossDb = referenceLossDb;
        scenario.pathLossExponent = pathLossExponent;

        std::ostringstream id;
        id << "log_distance_n" << pathLossExponent << "_tx" << txPowerDbm
           << "dBm_d" << distanceM << "m";
        scenario.experimentId = id.str();

        Ptr<LogDistancePropagationLossModel> propagationLoss =
            CreateObject<LogDistancePropagationLossModel>();
        propagationLoss->SetPathLossExponent(pathLossExponent);
        propagationLoss->SetReference(referenceDistanceM, referenceLossDb);

        const Results results = RunScenario(scenario, propagationLoss);
        WriteRow(logDistanceCsv, scenario, results);
        PrintSummary(scenario, results);
    }

    std::cout << "Friis transmit-power sweep saved to " << friisOutputFile << '\n';
    std::cout << "Log-distance n=4 transmit-power sweep saved to "
              << logDistanceOutputFile << '\n';
    return 0;
}
