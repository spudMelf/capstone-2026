# Log distance channel simulation walkthrough

This guide follows `capstone-log-distance.cc`. Blank lines, braces, comments,
and wrapped continuation lines are grouped with the statements they describe.

## Purpose and dependencies

- **Lines 1–9:** State the two-node topology and explain that the initial
  log-distance channel is a baseline to be replaced with measured data later.
- **Line 11:** Imports application helpers for generating and receiving traffic.
- **Line 12:** Imports the simulator clock, attributes, and core ns-3 types.
- **Line 13:** Imports FlowMonitor for packet, byte, and delay measurements.
- **Line 14:** Imports the IPv4 networking stack.
- **Line 15:** Imports positions and mobility models.
- **Line 16:** Imports fundamental nodes, devices, packets, and addresses.
- **Line 17:** Imports radio-propagation models.
- **Line 18:** Imports Wi-Fi PHY, MAC, and channel classes.
- **Lines 20–21:** Import standard C++ file output and number formatting.
- **Line 23:** Allows `NodeContainer` instead of `ns3::NodeContainer`.
- **Line 25:** Names the program's ns-3 logging component.
- **Lines 27–29:** Define the program entry point. It takes no command-line
  parameters because this experiment now uses fixed values.

## Fixed experiment values

- **Line 32:** Fixes transmitter–receiver separation at **1 m**.
- **Line 33:** Runs application traffic for **10 s**, long enough to obtain
  stable average packet metrics without making the simulation slow.
- **Line 34:** Uses **7.5 GHz** as the representative center of the antenna's
  7–8 GHz operating band.
- **Line 35:** Sets transmit power to **0 dBm**, or 1 mW. This is a sensible
  low-power baseline until the intended radio hardware is selected.
- **Line 36:** Uses a path-loss exponent of **2.0**, the ideal free-space trend.
  This is a neutral baseline, not yet a claim about the cryogenic enclosure.
- **Line 37:** Defines **1 m** as the distance at which reference loss is stated.
- **Line 38:** Uses **49.95 dB** reference loss, approximately the free-space
  loss at 7.5 GHz over 1 m.
- **Line 39:** Offers traffic at **1 Mb/s**, comfortably below the fixed 6 Mb/s
  PHY rate so congestion does not dominate channel behavior.
- **Line 40:** Uses **1,024-byte** UDP payloads, a common mid-sized test packet.
- **Line 41:** Selects the CSV output location.

These are controlled baseline assumptions. Distance is known; transmit power
should eventually come from the chosen radio, while reference loss and the
path-loss exponent should come from measurements.

## Nodes and physical positions

- **Line 44:** Creates a container for simulated nodes.
- **Line 45:** Creates two nodes: node 0 transmits and node 1 receives.
- **Line 48:** Creates an allocator for explicit three-dimensional positions.
- **Line 49:** Places the transmitter at `(0, 0, 0)`.
- **Line 50:** Places the receiver at `(1, 0, 0)`, using `distanceM`.
- **Line 51:** Creates the mobility configuration helper.
- **Line 52:** Supplies the two positions to that helper.
- **Line 53:** Selects a stationary-node model.
- **Line 54:** Installs the positions and mobility models on both nodes.

## Propagation channel—the capstone seam

- **Lines 58–59:** Create the baseline log-distance loss model.
- **Line 60:** Set its exponent `n`. Additional loss beyond the reference is
  `10 n log10(d/d0)` dB.
- **Line 61:** Set reference distance `d0` and reference loss `L0`.
- **Line 63:** Create the Wi-Fi channel that carries radio transmissions.
- **Line 64:** Put the propagation-loss object into that channel. Wi-Fi now uses
  it when determining received signal power and packet success.
- **Line 65:** Add propagation delay based on distance and the speed of light.

Lines 58–61 are the main replacement point for the eventual measurement-driven
channel. The topology, traffic, metrics, and output can stay the same.

## Wi-Fi radio and MAC

- **Lines 67–70:** Explain that ns-3's 802.11a OFDM PHY is being used as a
  packet/error-model abstraction, not as a claim that 7.5 GHz is Wi-Fi.
- **Line 71:** Creates the Wi-Fi physical-layer helper.
- **Line 72:** Connects both radios to the channel.
- **Lines 73–74:** Fix the radio to one transmit-power level of 0 dBm.
- **Line 76:** Creates the high-level Wi-Fi configuration helper.
- **Line 77:** Selects 802.11a as the closest built-in fixed-rate OFDM model.
- **Lines 78–82:** Hold data and control frames at 6 Mb/s. Disabling automatic
  rate changes prevents rate adaptation from obscuring channel effects.
- **Line 85:** Creates the Wi-Fi MAC configuration helper.
- **Line 86:** Selects ad hoc mode, so no access point is required.
- **Line 87:** Installs complete Wi-Fi network devices on both nodes.

## Internet stack and traffic

- **Lines 90–91:** Create and install IPv4, UDP, TCP, and routing support.
- **Lines 92–94:** Define subnet `10.1.1.0/24`, assign one address to each radio,
  and retain those addresses.
- **Line 97:** Selects UDP destination port 5000.
- **Lines 98–100:** Configure a UDP receiver listening on port 5000.
- **Line 101:** Install the receiver on node 1.
- **Lines 103–105:** Configure a UDP sender targeting node 1.
- **Line 106:** Make it send at the fixed 1 Mb/s rate using 1,024-byte payloads.
- **Line 107:** Install the sender on node 0.
- **Lines 110–111:** Start traffic at simulated time 1 s and calculate its fixed
  10-second stop time.
- **Lines 112–113:** Keep the receiver active for the full experiment.
- **Lines 114–115:** Run the sender only during the traffic interval.

## Simulation and measurements

- **Lines 118–119:** Install FlowMonitor probes on all nodes.
- **Lines 122–123:** Retrieve the transmitter and receiver position objects.
- **Lines 124–125:** Ask the actual propagation model used by Wi-Fi to calculate
  predicted received power.
- **Line 127:** Schedule the end of the simulation.
- **Line 128:** Run ns-3's discrete-event loop.
- **Line 131:** Mark any packets still outstanding after the run as lost.
- **Lines 132–135:** Prepare counters for packets, bytes, and delay.
- **Lines 136–137:** Retrieve the IPv4 flow classifier.
- **Lines 139–149:** Inspect all monitored flows, select traffic destined for
  UDP port 5000, and accumulate its statistics.
- **Lines 151–152:** Calculate packet-delivery ratio as received/sent × 100.
- **Lines 153–154:** Calculate packet error probability as failed/sent.
- **Lines 155–156:** Convert received bytes over 10 s into Mb/s.
- **Lines 157–158:** Calculate average one-way packet delay in milliseconds.

## Reporting and shutdown

- **Lines 161–171:** Print conditions and results with three decimal places.
- **Line 174:** Open the CSV file, replacing the preceding run's output.
- **Line 175:** Stop with an explanation if the file cannot be opened.
- **Lines 176–178:** Write the CSV headings, including packet error probability.
- **Lines 179–182:** Write the current run's values and results.
- **Line 184:** Release ns-3 simulation objects and reset global state.
- **Line 185:** Return success to the shell.

## Comparison with the Friis simulation

`capstone-friis.cc` keeps the same nodes, positions, transmit power, OFDM
abstraction, applications, timing, and metrics. It replaces lines 58–61 with a
`FriisPropagationLossModel` configured directly for 7.5 GHz. This ensures that
differences between the two result files come from propagation modeling rather
than unrelated simulation settings.

## Run the fixed experiments

From `/Users/eamon/ns3/ns-3.47`:

```bash
./ns3 run capstone-friis
./ns3 run capstone-log-distance
```
