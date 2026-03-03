#include <gtest/gtest.h>

#include <hegel/hegel.h>

using namespace hegel::generators;

TEST(OutsideContext, DrawThrowsOutsideTest) {
    try {
        hegel::draw(booleans());
        FAIL() << "Expected exception when calling draw() outside a Hegel "
                  "test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "draw() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

TEST(OutsideContext, NoteThrowsOutsideTest) {
    try {
        hegel::note("hello");
        FAIL() << "Expected exception when calling note() outside a Hegel test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "note() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}

TEST(OutsideContext, AssumeThrowsOutsideTest) {
    try {
        hegel::assume(true);
        FAIL()
            << "Expected exception when calling assume() outside a Hegel test";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "assume() cannot be called outside of a Hegel test"),
                  std::string::npos)
            << "Unexpected error message: " << e.what();
    }
}
