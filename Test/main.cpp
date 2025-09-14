//
//  main.cpp
//  Test
//
//  Created by Rasmus Anthin on 2025-09-14.
//

#include <iostream>
#include <applaudio/AudioEngine.h>

int main(int argc, const char* argv[])
{
  // insert code here...
  applaudio::AudioEngine engine;
  engine.print_device_name();
  std::cout << "Hello, World!\n";
  return 0;
}
