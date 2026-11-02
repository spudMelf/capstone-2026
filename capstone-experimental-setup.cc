/*
 * Best-current model of the planned cryostat-window experiment.
 *
 * The transmitter is a directional patch antenna on the 50 K side of the
 * cryostat, aligned through a low-loss window to a room-temperature patch
 * receiver. Both models use the same geometry, gains, and window loss.
 */

#include "capstone-simulation-common.h"

using namespace ns3;
using namespace capstone;

namespace
{

constexpr double CARRIER_FREQUENCY_GHZ = 7.5;
constexpr double DISTANCE_M = 1.0;
constexpr double TX_ENVIRONMENT_TEMPERATURE_K = 50.0;
constexpr double RX_ENVIRONMENT_TEMPERATURE_K = 290.0;
constexpr double WINDOW_SEPARATION_M = 0.15;
constexpr double WINDOW_INSERTION_LOSS_DB = 0.5;
constexpr double PATCH_ANTENNA_GAIN_DBI = 5.0;
constexpr double PATH_LOSS_EXPONENT = 2.0;
constexpr double REFERENCE_DISTANCE_M = 1.0;
constexpr double REFERENCE_LOSS_DB = 49.95;

Scenario
MakeBaseScenario()
{
    Scenario scenario;
    scenario.carrierFrequencyGHz = CARRIER_FREQUENCY_GHZ;
    scenario.distanceM = DISTANCE_M;
    scenario.txEnvironmentTemperatureK = TX_ENVIRONMENT_TEMPERATURE_K;
    scenario.rxEnvironmentTemperatureK = RX_ENVIRONMENT_TEMPERATURE_K;
    scenario.antennaType = "DirectionalPatch";
    scenario.antennaAlignment = "BoresightThroughWindow";
    scenario.txWindowSeparationM = WINDOW_SEPARATION_M;
    scenario.rxWindowSeparationM = WINDOW_SEPARATION_M;
    scenario.windowInsertionLossDb = WINDOW_INSERTION_LOSS_DB;
    scenario.txAntennaGainDbi = PATCH_ANTENNA_GAIN_DBI;
    scenario.rxAntennaGainDbi = PATCH_ANTENNA_GAIN_DBI;

    // The receiver is outside the cryostat, so its thermal-noise environment
    // remains room temperature even though the transmitter is on the 50 K side.
    scenario.noiseTemperatureK = RX_ENVIRONMENT_TEMPERATURE_K;
    return scenario;
}

Ptr<WindowInsertionLossModel>
MakeWindowLoss()
{
    Ptr<WindowInsertionLossModel> windowLoss = CreateObject<WindowInsertionLossModel>();
    windowLoss->SetLossDb(WINDOW_INSERTION_LOSS_DB);
    return windowLoss;
}

} // namespace

int
main()
{
    const std::string outputFile =
        "scratch/capstone/results/experimental-setup-results.csv";
    std::ofstream csv(outputFile, std::ios::trunc);
    NS_ABORT_MSG_IF(!csv.is_open(), "Could not open output file: " << outputFile);
    WriteHeader(csv);

    Scenario friisScenario = MakeBaseScenario();
    friisScenario.experimentId = "experimental_friis_d1m";
    friisScenario.propagationModel = "FriisPlusWindow";
    friisScenario.runNumber = 1;
    friisScenario.friisSystemLossLinear = 1.0;

    Ptr<FriisPropagationLossModel> friis = CreateObject<FriisPropagationLossModel>();
    friis->SetFrequency(CARRIER_FREQUENCY_GHZ * 1e9);
    friis->SetSystemLoss(1.0);
    friis->SetNext(MakeWindowLoss());

    const Results friisResults = RunScenario(friisScenario, friis);
    WriteRow(csv, friisScenario, friisResults);
    PrintSummary(friisScenario, friisResults);

    Scenario logDistanceScenario = MakeBaseScenario();
    logDistanceScenario.experimentId = "experimental_log_distance_n2_d1m";
    logDistanceScenario.propagationModel = "LogDistancePlusWindow";
    logDistanceScenario.runNumber = 2;
    logDistanceScenario.referenceDistanceM = REFERENCE_DISTANCE_M;
    logDistanceScenario.referenceLossDb = REFERENCE_LOSS_DB;
    logDistanceScenario.pathLossExponent = PATH_LOSS_EXPONENT;

    Ptr<LogDistancePropagationLossModel> logDistance =
        CreateObject<LogDistancePropagationLossModel>();
    logDistance->SetPathLossExponent(PATH_LOSS_EXPONENT);
    logDistance->SetReference(REFERENCE_DISTANCE_M, REFERENCE_LOSS_DB);
    logDistance->SetNext(MakeWindowLoss());

    const Results logDistanceResults = RunScenario(logDistanceScenario, logDistance);
    WriteRow(csv, logDistanceScenario, logDistanceResults);
    PrintSummary(logDistanceScenario, logDistanceResults);

    std::cout << "Experimental-setup comparison saved to " << outputFile << '\n';
    return 0;
}
