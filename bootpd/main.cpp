#include <iostream>
#include <string>
#include <vector>
#include <span>
#include <fstream>
#include <filesystem>
#include <cstdlib>

#include <common/address_v4.hpp>
#include <common/lexical_cast.hpp>
#include <common/control_c.hpp>
#include <common/config_ini.hpp>
#include <common/logger.hpp>
#include <common/arguments.hpp>

#include "dhcp_server_v4.hpp"
#include "tftp_server_v4.hpp"

int main(int argc, char** argv)
{
  using namespace std::string_literals;
  using namespace std::string_view_literals;
  try
  { 
    arguments args(argc, argv);
    
#ifndef NDEBUG
    std::filesystem::current_path(R"(C:\Users\alex\Desktop\projects\leisure\Cornel\workspace)");    
#endif

    std::filesystem::path config_path(args.value_or("-C"sv, "config.ini"sv));
    
    {
      using namespace std::filesystem;
      if (!exists(config_path))
        throw std::runtime_error("Unable to find file : "s + config_path.string());       
      if (!is_regular_file(config_path))
        throw std::runtime_error("Not a regular file : "s + config_path.string());
      if (file_size(config_path) < 1u)
        throw std::runtime_error("File is empty : "s + config_path.string());
    }
    
    config_ini config_ini_v(std::ifstream{ config_path });

    if (args.has("-O"))
    {
      Glog.debug("Overriding config ..."sv);
      for(auto line : args.values("-O")) {
        Glog.debug("* Adding line: '{}'"sv, line);
        config_ini_v.insert_line(line);
      }
    }
    
    dhcp_server_v4 dhcp_server_v (config_ini_v);    
    tftp_server_v4 tftp_server_v (config_ini_v);
    
    dhcp_server_v.start();
    tftp_server_v.start();
    
    while(!control_c::stop_requested())
    {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1s);
    }
    return 0;
  }
  catch (std::exception const& ex)
  {
    Glog.debug("current_path : {}"sv, std::filesystem::current_path().string());
    Glog.fatal("{}"sv, ex.what());
  }
  catch (...)
  {
    Glog.debug("current_path : {}"sv, std::filesystem::current_path().string());
    Glog.fatal("Unknown unhandled exception");
  }
  system("pause");
  return -1;
}
