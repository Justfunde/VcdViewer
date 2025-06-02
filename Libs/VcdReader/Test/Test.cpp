#include "Include/VcdStructs.hpp"
#include <filesystem>
#include <gtest/gtest.h>

// TEST(VcdReader, C17_flag)
//{
//    const std::filesystem::path fPath = "/home/justfunde/Work/vcd/test1.vcd";
//
//    newVcdReader reader;
//    const auto vcdHandle = reader.ParseFile(fPath);
// }

//"/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd"

// TEST(VcdReader, DISABLED_LargeFile)
//{
//    const std::filesystem::path fPath = "/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd";
//    // const std::filesystem::path fPath = "/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd";
//
//    newVcdReader reader;
//    auto vcdHandle = reader.ParseFile(fPath);
//
//    // vcdHandle->ensureSignalLoaded("'");
//    for (auto [_, pin] : vcdHandle->pinAlias2DescriptionMap)
//    {
//       // vcdHandle->ensureSignalLoaded(pin->alias);
//    }
// }
//
// TEST(VcdReader, DISABLED_LargeParallel)
//{
//    const std::filesystem::path fPath = "/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd";
//    // const std::filesystem::path fPath = "/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd";
//
//    newVcdReader reader;
//    auto vcdHandle = reader.ParseFile(fPath);
//
//    vcdHandle->loadParallel();
//    for (auto [_, pin] : vcdHandle->pinAlias2DescriptionMap)
//    {
//    }
// }
//
TEST(VcdReaderNew, majorityOf5)
{
   const std::filesystem::path fPath = "/home/justfunde/Projects/MIET/VcdViewer/VcdTests/test1.vcd";

   newVcd::Handle h;
   h.Init(fPath);
   h.LoadHdr();
   h.LoadSignals();
   // auto map = h.alias2pinMap();
   // auto pPin = map["%"];
   // auto pPin2 = std::static_pointer_cast<newVcd::SimplePinDescription>(pPin);
   int a = 5;
   a += 5;
   // const auto vcdHandle = reader.ParseFile(fPath);
}

// TEST(VcdReaderNew, majorityOf5_large)
//{
//    const std::filesystem::path fPath = "/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd";
//
//    newVcd::Handle h;
//    h.Init(fPath);
//    h.LoadHdr();
//    h.LoadSignalsParallel();
//    // auto map = h.alias2pinMap();
//    // auto pPin = map["%"];
//    // auto pPin2 = std::static_pointer_cast<newVcd::SimplePinDescription>(pPin);
//    int a = 5;
//    a += 5;
//    // const auto vcdHandle = reader.ParseFile(fPath);
// }
//
int main(int argc, char **argv)
{
   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
