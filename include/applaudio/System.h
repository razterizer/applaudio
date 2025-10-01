//
//  System.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-01.
//

#pragma once
#include "StringUtils.h"
#include <fstream>

namespace apl_sys
{

  bool is_wsl()
  {
#ifdef __linux__
    static bool checked = false;
    static bool wsl_flag = false;
    
    if (checked)
      return wsl_flag;
    std::ifstream version_file("/proc/version");
    std::string version;
    if (version_file && std::getline(version_file, version))
    {
      auto version_lower = apl_str::to_lower(version);
      //std::cout << version << std::endl;
      wsl_flag = version_lower.find("microsoft") != std::string::npos
              || version_lower.find("wsl") != std::string::npos;
    }
    checked = true;
    return wsl_flag;
#endif
    return false;
  }

}
