//
//  StringUtils.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-01.
//

#pragma once
#include <string>


namespace apl_str
{

  // char or wchar_t
  template<typename char_t>
  char_t to_lower(char_t ch)
  {
    return std::tolower(ch);
  }
  
  template<typename char_t>
  std::basic_string<char_t> to_lower(const std::basic_string<char_t>& str)
  {
    std::basic_string<char_t> ret = str;
    size_t len = str.length();
    for (size_t c_idx = 0; c_idx < len; ++c_idx)
      ret[c_idx] = to_lower(str[c_idx]);
    return ret;
  }
  
}
