#pragma once
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <map>

namespace utils
{

   inline std::string BinaryToHex(const std::string &binary)
   {
      const std::map<std::string, char> binToHexMap = {
          {"0000", '0'}, {"0001", '1'}, {"0010", '2'}, {"0011", '3'}, {"0100", '4'}, {"0101", '5'}, {"0110", '6'}, {"0111", '7'}, {"1000", '8'}, {"1001", '9'}, {"1010", 'A'}, {"1011", 'B'}, {"1100", 'C'}, {"1101", 'D'}, {"1110", 'E'}, {"1111", 'F'}};

      std::string hexString = "";
      std::string tempBinary = binary;

      int paddingNeeded = (4 - tempBinary.length() % 4) % 4;
      if (paddingNeeded != 0)
      {
         tempBinary.insert(0, paddingNeeded, '0');
      }

      for (size_t i = 0; i < tempBinary.length(); i += 4)
      {
         std::string nibble = tempBinary.substr(i, 4);
         hexString += binToHexMap.at(nibble);
      }

      size_t firstNonZero = hexString.find_first_not_of('0');
      if (std::string::npos != firstNonZero)
      {
         return hexString.substr(firstNonZero);
      }
      return "0";
   }

}