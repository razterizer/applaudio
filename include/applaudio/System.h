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
    static const bool result = []()
    {
      std::ifstream version_file("/proc/version");
      std::string version;
      
      if (!version_file || !std::getline(version_file, version))
        return false;
      
      auto version_lower = apl_str::to_lower(version);
      return version_lower.find("microsoft") != std::string::npos
          || version_lower.find("wsl") != std::string::npos;
    }();
    
    return result;
#else
    return false;
#endif
  }

}
