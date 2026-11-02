build_exec(
  EXECNAME capstone-log-distance
  SOURCE_FILES capstone-log-distance.cc
  LIBRARIES_TO_LINK ${libapplications}
                    ${libcore}
                    ${libflow-monitor}
                    ${libinternet}
                    ${libmobility}
                    ${libnetwork}
                    ${libpropagation}
                    ${libwifi}
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/capstone
)

build_exec(
  EXECNAME capstone-friis
  SOURCE_FILES capstone-friis.cc
  LIBRARIES_TO_LINK ${libapplications}
                    ${libcore}
                    ${libflow-monitor}
                    ${libinternet}
                    ${libmobility}
                    ${libnetwork}
                    ${libpropagation}
                    ${libwifi}
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/capstone
)

build_exec(
  EXECNAME capstone-tx-power-sweep
  SOURCE_FILES capstone-tx-power-sweep.cc
  LIBRARIES_TO_LINK ${libapplications}
                    ${libcore}
                    ${libflow-monitor}
                    ${libinternet}
                    ${libmobility}
                    ${libnetwork}
                    ${libpropagation}
                    ${libwifi}
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/capstone
)

build_exec(
  EXECNAME capstone-experimental-setup
  SOURCE_FILES capstone-experimental-setup.cc
  LIBRARIES_TO_LINK ${libapplications}
                    ${libcore}
                    ${libflow-monitor}
                    ${libinternet}
                    ${libmobility}
                    ${libnetwork}
                    ${libpropagation}
                    ${libwifi}
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/capstone
)

build_exec(
  EXECNAME capstone-antenna-model-comparison
  SOURCE_FILES capstone-antenna-model-comparison.cc
  LIBRARIES_TO_LINK ${libantenna}
                    ${libapplications}
                    ${libcore}
                    ${libflow-monitor}
                    ${libinternet}
                    ${libmobility}
                    ${libnetwork}
                    ${libpropagation}
                    ${libspectrum}
                    ${libwifi}
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/capstone
)
