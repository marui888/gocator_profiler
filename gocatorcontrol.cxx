#include "gocatorcontrol.h"

// Configures Gocator 20x0 to use an attached encoder.
// Currently set to trigger in both directions.
// encoderResolution - encoder's resolution (mm/tick)
// triggerThreshold - # ticks to trigger encoder
void GocatorControl::configureEncoder(double encoderResolution, double triggerThreshold) {
    resolution = encoderResolution;
    travel_threshold = triggerThreshold;
    std::string SetTriggerResponse = getResponseString("Go2System_SetTriggerSource", 
                                           Go2System_SetTriggerSource(sys.getSystem(), GO2_TRIGGER_SOURCE_ENCODER));
    if (verbose) {
        std::cout << SetTriggerResponse << std::endl;
    }
 
    std::string SetResolutionResponse = getResponseString("Go2System_SetTravelResolution", 
                                              Go2System_SetTravelResolution(sys.getSystem(), resolution));
    if (verbose) {
        std::cout << SetResolutionResponse << std::endl;
    }

    std::string DisableTriggerGateResponse = getResponseString("Go2System_EnableTriggerGate", 
                                                   Go2System_EnableTriggerGate(sys.getSystem(), GO2_FALSE));
    if (verbose) {
        std::cout << DisableTriggerGateResponse << std::endl;
    }

    std::string SetEncoderPeriodResponse = getResponseString("Go2System_SetEncoderPeriod", 
                                                 Go2System_SetEncoderPeriod(sys.getSystem(), travel_threshold));
    if (verbose) {
        std::cout << SetEncoderPeriodResponse << std::endl;
    }

    std::string SetTriggerModeResponse = getResponseString("Go2System_SetEncoderTriggerMode", 
                                               Go2System_SetEncoderTriggerMode(sys.getSystem(), GO2_ENCODER_TRIGGER_MODE_BIDIRECTIONAL));
    if (verbose) {
        std::cout << SetTriggerModeResponse << std::endl;
    }
}
    
// Records range profiles to disk as comma-delimited ASCII.
void GocatorControl::recordProfile(std::string& outputFilename) {
    std::ofstream fidout;
    fidout.open(outputFilename.c_str(), std::ios_base::app);
    if (fidout.is_open()) {
        fidout << "# File format: X Position [mm], Y Position [mm], Z Range [mm]" << std::endl;
        Go2ProfileData data = GO2_NULL;
        Go2Data dataItem;
        Go2Int64 encoderCounter;
        unsigned int itemCount = 0;
        std::string StartResponse = getResponseString("Go2System_Start",Go2System_Start(sys.getSystem()));
        if (verbose) {
            std::cout << StartResponse << std::endl;
        }
        Go2Status returnCode = Go2System_ConnectData(sys.getSystem(), GO2_NULL, GO2_NULL);
        if (verbose) {
            std::cout << getResponseString("Go2System_ConnectData", returnCode) << std::endl;
        }
        if (returnCode != GO2_OK) {
            // todo-implement halt
            std::cerr << "Initialization failed, program halted." << std::endl;
        }
        while(true) {
            Go2Status returnCode = Go2System_ReceiveData(sys.getSystem(), RECEIVE_TIMEOUT, &data);
            if (returnCode == GO2_OK) {
                itemCount = Go2Data_ItemCount(data);
                for (unsigned int j=0; j<itemCount; j++) {
                    dataItem = Go2Data_ItemAt(data, j);
                    short* profileData = Go2ProfileData_Ranges(dataItem);
                    unsigned int profilePointCount = Go2ProfileData_Width(dataItem);
                    double XResolution = Go2ProfileData_XResolution(dataItem);
                    double ZResolution = Go2ProfileData_ZResolution(dataItem);
                    double XOffset = Go2ProfileData_XOffset(dataItem);
                    double ZOffset = Go2ProfileData_ZOffset(dataItem);
                    std::cout << getResponseString("Go2System_GetEncoder", Go2System_GetEncoder(sys.getSystem(), &encoderCounter)) << std::endl; // number of ticks of encoder
                    for(unsigned int arrayIndex=0;arrayIndex<profilePointCount; ++arrayIndex) {
                        if (profileData[arrayIndex] != INVALID_RANGE_16BIT) {
                            fidout << XOffset+XResolution*arrayIndex << "," << encoderCounter*resolution << "," << ZOffset+ZResolution*profileData[arrayIndex] << std::endl;
                        } else {
                            std::cout << "Invalid reading, skipped." << std::endl;
                        }
                    }
                }
            }
        }
        fidout.close();
    } else {
        std::cerr << "Unable to open/write to output file '" << outputFilename << ".'" << std::endl;
    }
}