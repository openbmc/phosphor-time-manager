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
    try{
        phosphor::time::EpochTime EpochTime(
                sdbusplus::bus::new_system(),
                busname.c_str(),
                objpath.c_str());
            EpochTime.run();
        exit(EXIT_SUCCESS);
    }
    catch(const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    exit(EXIT_FAILURE);
}
