#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Demonstrate spdlog
TEST(SpdlogTest, BasicLogging) {
    spdlog::info("spdlog works in tests");
    SUCCEED();
}

// Demonstrate nlohmann/json
TEST(JsonTest, ParseAndAccess) {
    using json = nlohmann::json;
    auto j = json::parse(R"({"name":"test","value":42})");
    EXPECT_EQ(j["name"], "test");
    EXPECT_EQ(j["value"], 42);
}

TEST(JsonTest, Serialize) {
    using json = nlohmann::json;
    json j;
    j["items"] = {1, 2, 3};
    j["active"] = true;
    EXPECT_EQ(j.dump(), R"({"active":true,"items":[1,2,3]})");
}
