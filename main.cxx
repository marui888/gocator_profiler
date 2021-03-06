/* gocator_encoder - sample console application to use Gocator 20x0 laser profilers

Chris R. Coughlin (TRI/Austin, Inc.)
*/
extern "C" {
    #include "Go2.h"
}
#include "gocatorsystem.h"
#include "gocatorcontrol.h"
#include "gocatorconfigurator.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace opts = boost::program_options;
namespace filesystem = boost::filesystem;

// Interrupts and joins specified thread on user input
// TODO - replace user input with timeout as per PM's suggestion - ?
void wait(boost::thread& threadToClose) {
    char character;
    std::cout << "Press any key + Enter to stop recording." << std::endl;
    std::cin >> character;
    threadToClose.interrupt();    
    threadToClose.join();
}
// Thread wrapper to run profile recording
void recordProfile(GocatorControl& control, std::string& outputFilename, std::string& commentString) {
    boost::thread thd(boost::bind(&GocatorControl::recordProfile, control, outputFilename, commentString));
    wait(thd);
}
void recordProfile(GocatorControl& control, std::string& outputFilename) {
    boost::thread thd(boost::bind(&GocatorControl::recordProfile, control, outputFilename));
    wait(thd);
}


// Turn the laser on to allow positioning before the profiling
void target(GocatorControl& control) {
    control.targetOn();
    char character2;
    std::cout << "Move the laser into position.  Press any key + Enter when complete." << std::endl;
    std::cin >> character2;
    control.targetOff();
}

// Usage: gocator_encoder [--output outputfile] [--config configfile]
// If not specified, writes X,Y,Z data to file 'profile.csv' in current folder.
int main(int argc, char* argv[]) {
    std::cout << "Gocator 20x0 Profiler" << std::endl;
    std::cout << "Chris R. Coughlin (TRI/Austin, Inc.)" << std::endl;
    opts::options_description opt_desc("Available options");
    opt_desc.add_options()
        ("output,o", opts::value<std::string>()->default_value("profile.csv"), "output file for profile data")
        ("config,c", opts::value<std::string>()->default_value("gocator_encoder.cfg"), "configuration file")
        ("target,t", "enable laser for targeting prior to profiling")
        ("message,m", opts::value<std::string>(), "set comments for data output header")
        ("help,h", "display basic help information")
        ("verbose,v", "display additional messages")
    ;

    // Parse command line
    opts::variables_map cmdline;
    opts::store(opts::parse_command_line(argc, argv, opt_desc), cmdline);
    opts::notify(cmdline);
    if (cmdline.count("help")) {
        std::cout << opt_desc << std::endl;
        return 1;
    }
    bool verbose = false;
    if (cmdline.count("verbose")) {
        verbose = true;
    }
    std::string outputFilename;
    if (cmdline.count("output")) {
        outputFilename = cmdline["output"].as<std::string>();
    }
    if (verbose) {
        std::cout << "Saving profile data to '" << outputFilename << "'" << std::endl;
    }

    std::string configFilename;
    if (cmdline.count("config")) {
        configFilename = cmdline["config"].as<std::string>();
    }
    if (!filesystem::exists(configFilename.c_str())) {
        std::cerr << "<< Unable to find configuration file '" << configFilename << ",' aborting >>" << std::endl;
        throw std::runtime_error("Configuration file not found");
    }
    if (verbose) {
        std::cout << "Additional config read from '" << configFilename << "'\n\n" << std::endl;  
    }
    try {
        // Startup and login
        GocatorSystem gocator(verbose);
        GocatorControl control(gocator, verbose);
        GocatorAddress configuredAddress = GocatorConfigurator::configuredNetworkConnection(configFilename);
        gocator.init(GocatorConfigurator::deviceID(configFilename), 
                     configuredAddress.addr, 
                     configuredAddress.reconfigure);

        // Configure encoder 
        Encoder lme = GocatorConfigurator::configuredEncoder(configFilename);
        control.configureEncoder(lme);
        if (verbose) {
            std::cout << "\n<< Using " << lme.modelName << " encoder, ";
            std::cout << "resolution " << lme.resolution << " mm/tick" << std::endl;
        }
        
        // Configure trigger
        boost::shared_ptr<Trigger> trigger(GocatorConfigurator::configuredTrigger(configFilename));
        trigger->set(control);
        if (verbose) {
            std::cout << "<< Triggering: " << trigger->getTriggerType() << ", gate ";
            if (trigger->isTriggerGateEnabled()) {
                std::cout << "enabled";
            } else {
                std::cout << "disabled";
            }
            std::cout << " >>\n" << std::endl;
        }

        // Configure filtration
        GocatorFilter filtration = GocatorConfigurator::configuredFilter(configFilename);
        if (verbose) {
            std::cout << "<< Data Processing Settings >>" << std::endl;
            std::cout << "    Resolution:  ";
            switch (filtration.sampling) {
                case 0:
                std::cout << "high" << std::endl;
                break;
                case 1:
                std::cout << "balanced" << std::endl;
                break;
                case 2:
                std::cout << "low" << std::endl;
            }

            std::cout << "    Horizontal gap filling:  ";
            if (filtration.xGap) {
                std::cout << "enabled, " << filtration.xGapWindow << " mm" << std::endl;
            } else {
                std::cout << "disabled" << std::endl;
            }

            std::cout << "    Vertical gap filling:  ";
            if (filtration.yGap) {
                std::cout << "enabled, " << filtration.yGapWindow << " mm" << std::endl;
            } else {
                std::cout << "disabled" << std::endl;
            }

            std::cout << "    Horizontal signal averaging:  ";
            if (filtration.xSmooth) {
                std::cout << "enabled, " << filtration.xSmoothWindow << " mm" << std::endl;
            } else {
                std::cout << "disabled" << std::endl;
            }

            std::cout << "    Vertical signal averaging:  ";
            if (filtration.ySmooth) {
                std::cout << "enabled, " << filtration.ySmoothWindow << " mm" << std::endl;
            } else {
                std::cout << "disabled" << std::endl;
            }
            std::cout << "\n\n" << std::endl;
        }

        // Optionally turn the laser on without recording data - let the
        // user line up the scanner
        if (cmdline.count("target")) {
            target(control);
            return 0;
        }

        // Output profile  
        std::cout << "Connected to Gocator, monitoring encoder..." << std::endl;  
        // Optionally provide a comment to include in the data output's header
        // (Default - current date and time)
        if (cmdline.count("message")) {
            std::string messageString = cmdline["message"].as<std::string>();            
            recordProfile(control, outputFilename, messageString);
        } else {
            recordProfile(control, outputFilename);
        }
    } catch (const boost::program_options::invalid_option_value& ex) {
        std::cerr << "Encountered a bad config option in '" << configFilename << ".'" << std::endl;
        throw(ex);
    }
    return 0;
}