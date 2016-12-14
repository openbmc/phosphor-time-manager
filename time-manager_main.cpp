#include "config.h"
//#include "log_manager.hpp"
#include <sdbusplus/bus.hpp>
#include <cstdlib>
#include <iostream>
#include <exception>
#include "time-manager.hpp"

int main(int argc, char *argv[])
{
    std::string busname,objpath;

    busname = BUSNAME_TIME_MANAGER;
    objpath = OBJ_TIME_MANAGER;

    // Instantiate the Timemanager object.
    auto tmgr = std::make_unique<TimeManager>();

    // Wait for the work
    auto r = tmgr->waitForClientRequest();

/*
    try {
        auto EpochTime = phosphor::EpochTime::EpochTime(
                sdbusplus::bus::new_system(),
                BUSNAME,
                OBJ);
        EpochTime.run();
        exit(EXIT_SUCCESS);
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    exit(EXIT_FAILURE);
*/
if(1) {
    try{
        auto EpochTime = EpochTime(
                sdbusplus::bus::new_system(),
                busname.c_str(),
                objpath.c_str());
            EpochTime.run();
        exit(EXIT_SUCCESS);
    }

}
else {
        try{
        phosphor::time::EpochTime EpochTime(
                sdbusplus::bus::new_system(),
                busname.c_str(),
                objpath.c_str());
            EpochTime.run();
        exit(EXIT_SUCCESS);
    }
}



    catch(const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    exit(EXIT_FAILURE);
}
