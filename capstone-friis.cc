/*
 * Friis distance sweep for the capstone wireless channel model.
 * Each scenario uses the same radio, traffic, noise, and receiver conditions.
 */

#include "capstone-simulation-common.h"

#include <array>
#include <filesystem>
#include <sstream>

using namespace ns3;
using namespace capstone;

int
main()
{
    constexpr std::array<double, 6> distancesM = {1.0, 1.5, 2.0, 3.0, 4.0, 5.0};
    constexpr double carrierFrequencyGHz = 7.5;
    constexpr double systemLossLinear = 1.0;
    const std::string outputFile = "scratch/capstone/results/friis-results.csv";

    // Rebuild the complete distance-sweep dataset on every execution.
    std::ofstream csv(outputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!csv.is_open(), "Could not open output file: " << outputFile);
    WriteHeader(csv);

    uint64_t runNumber = 1;
    for (const double distanceM : distancesM)
    {
        Scenario scenario;
        scenario.propagationModel = "Friis";
        scenario.runNumber = runNumber++;
        scenario.carrierFrequencyGHz = carrierFrequencyGHz;
        scenario.distanceM = distanceM;
        scenario.friisSystemLossLinear = systemLossLinear;

        std::ostringstream id;
        id << "friis_d" << distanceM << "m";
        scenario.experimentId = id.str();

        Ptr<FriisPropagationLossModel> propagationLoss =
            CreateObject<FriisPropagationLossModel>();
        propagationLoss->SetFrequency(carrierFrequencyGHz * 1e9);
        propagationLoss->SetSystemLoss(systemLossLinear);

        const Results results = RunScenario(scenario, propagationLoss);
        WriteRow(csv, scenario, results);
        PrintSummary(scenario, results);
    }

    std::cout << "Friis sweep saved to " << outputFile << '\n';
    return 0;
}
