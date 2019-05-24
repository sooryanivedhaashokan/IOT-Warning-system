#ifndef __SENSOR_AGGREGATOR_PROTOCOL_HPP__
#define __SENSOR_AGGREGATOR_PROTOCOL_HPP__

#include <array>
#include <vector>

#include "sample_data.hpp"

namespace sensor_aggregator {
  std::vector<unsigned char> makeHandshakeMsg();

  std::vector<unsigned char> makeSampleMsg(SampleData& sample);

  SampleData decodeSampleMsg(std::vector<unsigned char>& buff, unsigned int length);
}

#endif
