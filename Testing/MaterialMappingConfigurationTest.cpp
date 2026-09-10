/**
 * Regression coverage for material-mapping configuration snapshots.
 */

#include "PowerLawFunctor.h"
#include "PowerLawParameters.h"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
  void Require(bool condition, const std::string& message)
  {
    if (!condition)
    {
      throw std::runtime_error(message);
    }
  }

  void RequireNear(double actual, double expected, const std::string& message)
  {
    Require(std::abs(actual - expected) < 1e-10, message);
  }
}

int main()
{
  try
  {
    PowerLawFunctor original;
    original.AddPowerLaw(PowerLawParameters(1.0, 1.0, 7.0), 0.0);
    original.AddPowerLaw(PowerLawParameters(2.0, 1.0, 3.0), 200.0);
    original.AddPowerLaw(PowerLawParameters(3.0, 1.0, 9.0), 300.0);

    // This exercises the first call above every configured range, where an
    // uninitialized cache used to dereference a null or stale pointer.
    RequireNear(original(350.0), 1059.0,
                "A newly configured power-law functor must evaluate its final range.");

    auto copied = original;
    RequireNear(copied(-2.0), 5.0,
                "A copied power-law configuration must not retain iterators into its source map.");
    RequireNear(copied(250.0), 759.0,
                "A copied power-law configuration must preserve its parameters.");

    PowerLawFunctor assigned;
    assigned = original;
    RequireNear(assigned(100.0), 203.0,
                "An assigned power-law configuration must reset and rebuild its cache.");

    auto moved = std::move(assigned);
    RequireNear(moved(250.0), 759.0,
                "A moved power-law configuration must remain valid for background processing.");

    PowerLawFunctor empty;
    bool emptyWasRejected = false;
    try
    {
      static_cast<void>(empty(1.0));
    }
    catch (const std::logic_error&)
    {
      emptyWasRejected = true;
    }
    Require(emptyWasRejected, "An empty power-law configuration must be rejected explicitly.");

    std::cout << "Material-mapping configuration regression test passed." << std::endl;
    return EXIT_SUCCESS;
  }
  catch (const std::exception& exception)
  {
    std::cerr << "Material-mapping configuration regression test failed: " << exception.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "Material-mapping configuration regression test failed with an unknown exception." << std::endl;
  }

  return EXIT_FAILURE;
}
