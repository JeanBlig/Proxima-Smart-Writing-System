#include <iostream>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "../AppLayer/AppLayer.h"

int main()
{
    std::cout << "=====================================\n";
    std::cout << "Running Proxima Smart Writing Tests\n";
    std::cout << "=====================================\n\n";

    // Create the test runner
    CppUnit::TextUi::TestRunner runner;

    // Get the global registry (contains all registered tests)
    CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();

    // Add all tests to the runner
    runner.addTest(registry.makeTest());

    // Run tests
    bool success = runner.run("", false);

    std::cout << "\n=====================================\n";

    if (success)
    {
        std::cout << "ALL TESTS PASSED\n";
    }
    else
    {
        std::cout << "SOME TESTS FAILED\n";
    }

    std::cout << "=====================================\n";

    return success ? 0 : 1;
}