#include <doctest/doctest.h>
#include <timenav/timenav.hpp>

TEST_CASE("timenav exposes a version string") { CHECK(timenav::version() == "0.0.1"); }
