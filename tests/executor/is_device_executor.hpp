#include <cudex/executor/inline_executor.hpp>
#include <cudex/executor/is_device_executor.hpp>
#include <cudex/executor/stream_executor.hpp>


namespace ns = cudex;


void test_is_device_executor()
{
  static_assert(ns::is_device_executor<ns::stream_executor>::value, "Error.");
  static_assert(!ns::is_device_executor<ns::inline_executor>::value, "Error.");
}

