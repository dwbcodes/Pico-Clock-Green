#include <unity.h>

#include "network_identity.hpp"

using pico_clock::format_fqdn;
using pico_clock::valid_domain_name;
using pico_clock::valid_hostname;
using pico_clock::valid_network_identity;

void setUp() {}
void tearDown() {}

void test_validates_dns_host_labels() {
    TEST_ASSERT_TRUE(valid_hostname("greenpico"));
    TEST_ASSERT_TRUE(valid_hostname("clock-2"));
    TEST_ASSERT_FALSE(valid_hostname(""));
    TEST_ASSERT_FALSE(valid_hostname("-clock"));
    TEST_ASSERT_FALSE(valid_hostname("clock.local"));
}

void test_validates_optional_multi_label_domains() {
    TEST_ASSERT_TRUE(valid_domain_name(""));
    TEST_ASSERT_TRUE(valid_domain_name("internal"));
    TEST_ASSERT_TRUE(valid_domain_name("lab.example.com"));
    TEST_ASSERT_FALSE(valid_domain_name(".internal"));
    TEST_ASSERT_FALSE(valid_domain_name("lab..internal"));
    TEST_ASSERT_FALSE(valid_domain_name("lab_internal"));
}

void test_formats_fqdn_with_or_without_domain() {
    char output[320]{};
    TEST_ASSERT_TRUE(format_fqdn("greenpico", "internal", output,
                                 sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("greenpico.internal", output);
    TEST_ASSERT_TRUE(format_fqdn("greenpico", "", output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("greenpico", output);
    char small[8]{};
    TEST_ASSERT_FALSE(format_fqdn("greenpico", "internal", small,
                                  sizeof(small)));
}

void test_rejects_an_fqdn_over_253_characters() {
    char domain[254]{};
    for (int index = 0; index < 63; ++index) domain[index] = 'a';
    domain[63] = '.';
    for (int index = 64; index < 127; ++index) domain[index] = 'b';
    domain[127] = '.';
    for (int index = 128; index < 191; ++index) domain[index] = 'c';
    domain[191] = '.';
    for (int index = 192; index < 253; ++index) domain[index] = 'd';
    TEST_ASSERT_TRUE(valid_domain_name(domain));
    TEST_ASSERT_FALSE(valid_network_identity("greenpico", domain));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_validates_dns_host_labels);
    RUN_TEST(test_validates_optional_multi_label_domains);
    RUN_TEST(test_formats_fqdn_with_or_without_domain);
    RUN_TEST(test_rejects_an_fqdn_over_253_characters);
    return UNITY_END();
}
