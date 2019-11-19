/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <rmf_traffic_ros2/Time.hpp>

#include <cassert>

namespace rmf_traffic_ros2 {

//==============================================================================
builtin_interfaces::msg::Time convert(rmf_traffic::Time time)
{
  const auto duration = time.time_since_epoch();

  // We're assuming that we never have to go back in time to before the UNIX
  // epoch.
  // TODO(MXG): Be more robust than this.
  assert(duration.count() > 0);

  builtin_interfaces::msg::Time result;
  result.sec = std::chrono::duration_cast<
      std::chrono::seconds>(duration).count();

  const auto nanoseconds = duration - std::chrono::seconds(result.sec);
  result.nanosec = nanoseconds.count();

  return result;
}

} // namespace rmf_traffic_ros2