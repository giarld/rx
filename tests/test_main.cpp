#include <gtest/gtest.h>

#define USE_GANY_CORE
#include <gx/gany.h>

#include <rx/leak_observer.h>

int main(int argc, char **argv)
{
    initGAnyCore();

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    rx::LeakObserver::checkLeak();
    return result;
}
