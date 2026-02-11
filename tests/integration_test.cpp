#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include "muxi/server_client.hpp"
#include "muxi/formation_client.hpp"

namespace {

std::string getEnv(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
}

bool isConfigured() {
    return !getEnv("MUXI_SDK_E2E_SERVER_URL").empty() &&
           !getEnv("MUXI_SDK_E2E_KEY_ID").empty() &&
           !getEnv("MUXI_SDK_E2E_SECRET_KEY").empty() &&
           !getEnv("MUXI_SDK_E2E_FORMATION_ID").empty() &&
           !getEnv("MUXI_SDK_E2E_CLIENT_KEY").empty() &&
           !getEnv("MUXI_SDK_E2E_ADMIN_KEY").empty();
}

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!isConfigured()) {
            GTEST_SKIP() << "E2E environment variables not set";
        }
    }
};

class ServerIntegrationTest : public IntegrationTest {
protected:
    muxi::ServerClient getClient() {
        muxi::ServerConfig config;
        config.url = getEnv("MUXI_SDK_E2E_SERVER_URL");
        config.key_id = getEnv("MUXI_SDK_E2E_KEY_ID");
        config.secret_key = getEnv("MUXI_SDK_E2E_SECRET_KEY");
        return muxi::ServerClient(config);
    }
};

class FormationIntegrationTest : public IntegrationTest {
protected:
    muxi::FormationClient getClient() {
        muxi::FormationConfig config;
        config.server_url = getEnv("MUXI_SDK_E2E_SERVER_URL");
        config.formation_id = getEnv("MUXI_SDK_E2E_FORMATION_ID");
        config.client_key = getEnv("MUXI_SDK_E2E_CLIENT_KEY");
        config.admin_key = getEnv("MUXI_SDK_E2E_ADMIN_KEY");
        return muxi::FormationClient(config);
    }
};

// C++ SDK doesn't have ping() method

TEST_F(ServerIntegrationTest, Health) {
    auto client = getClient();
    auto result = client.health();
    EXPECT_FALSE(result.empty());
}

TEST_F(ServerIntegrationTest, Status) {
    auto client = getClient();
    auto result = client.status();
    EXPECT_FALSE(result.empty());
}

TEST_F(ServerIntegrationTest, ListFormations) {
    auto client = getClient();
    auto result = client.list_formations();
    EXPECT_FALSE(result.empty());
}

TEST_F(FormationIntegrationTest, Health) {
    auto client = getClient();
    auto result = client.health();
    EXPECT_FALSE(result.empty());
}

TEST_F(FormationIntegrationTest, GetStatus) {
    auto client = getClient();
    auto result = client.get_status();
    EXPECT_FALSE(result.empty());
}

TEST_F(FormationIntegrationTest, GetConfig) {
    auto client = getClient();
    auto result = client.get_config();
    EXPECT_FALSE(result.empty());
}

TEST_F(FormationIntegrationTest, GetAgents) {
    auto client = getClient();
    auto result = client.get_agents();
    EXPECT_FALSE(result.empty());
}

}  // namespace
