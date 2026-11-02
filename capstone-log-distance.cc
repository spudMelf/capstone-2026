/*
 * Log-distance sweep for the capstone wireless channel model.
 * The sweep varies both the path-loss exponent and receiver distance.
 */

#include "capstone-simulation-common.h"

#include <array>
#include <sstream>

using namespace ns3;
using namespace capstone;

int
main()
{
    constexpr std::array<double, 6> distancesM = {1.0, 1.5, 2.0, 3.0, 4.0, 5.0};
    constexpr std::array<double, 5> pathLossExponents = {2.0, 3.0, 4.0, 5.0, 6.0};
    constexpr double referenceDistanceM = 1.0;
    constexpr double referenceLossDb = 49.95;
    const std::string outputFile = "scratch/capstone/results/log-distance-results.csv";

    // Rebuild the complete exponent-and-distance sweep on every execution.
    std::ofstream csv(outputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!csv.is_open(), "Could not open output file: " << outputFile);
    WriteHeader(csv);

    uint64_t runNumber = 1;
    for (const double pathLossExponent : pathLossExponents)
    {
        for (const double distanceM : distancesM)
        {
            Scenario scenario;
            scenario.propagationModel = "LogDistance";
            scenario.runNumber = runNumber++;
            scenario.distanceM = distanceM;
            scenario.referenceDistanceM = referenceDistanceM;
            scenario.referenceLossDb = referenceLossDb;
            scenario.pathLossExponent = pathLossExponent;

            std::ostringstream id;
            id << "log_distance_n" << pathLossExponent << "_d" << distanceM << "m";
            scenario.experimentId = id.str();

            Ptr<LogDistancePropagationLossModel> propagationLoss =
                CreateObject<LogDistancePropagationLossModel>();
            propagationLoss->SetPathLossExponent(pathLossExponent);
            propagationLoss->SetReference(referenceDistanceM, referenceLossDb);

            const Results results = RunScenario(scenario, propagationLoss);
            WriteRow(csv, scenario, results);
            PrintSummary(scenario, results);
        }
    }

    std::cout << "Log-distance sweep saved to " << outputFile << '\n';
    return 0;
}
