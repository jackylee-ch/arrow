// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "gandiva/parse_url_holder.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace gandiva {

  class TestParseUrlHolder : public ::testing::Test {
    protected:
      ExecutionContext execution_context_;
    };

    TEST_F(TestParseUrlHolder, TestParseUrlWithPart) {
    std::shared_ptr<ParseUrlHolder> part_url_holder;
    auto status = ParseUrlHolder::Make(&part_url_holder);
    auto &parse_url = * part_url_holder;
    std::string input_string = "https://userinfo@arrow.apache.org:8080/path?query=1#fragment";
    int32_t out_length = 0;

    // HOST
    std::string part_string = "HOST";
    const char *ret1 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret1_as_str(ret1, out_length);
    EXPECT_EQ(out_length, 16);
    EXPECT_EQ(ret1_as_str, "arrow.apache.org");

    // PATH
    part_string = "PATH";
    const char *ret2 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret2_as_str(ret2, out_length);
    EXPECT_EQ(out_length, 5);
    EXPECT_EQ(ret2_as_str, "/path");

    // QUERY
    part_string = "QUERY";
    const char *ret3 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret3_as_str(ret3, out_length);
    EXPECT_EQ(out_length, 7);
    EXPECT_EQ(ret3_as_str, "query=1");

    // PROTOCOL
    part_string = "PROTOCOL";
    const char *ret4 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret4_as_str(ret4, out_length);
    EXPECT_EQ(out_length, 5);
    EXPECT_EQ(ret4_as_str, "https");

    // FILE
    part_string = "FILE";
    const char *ret5 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret5_as_str(ret5, out_length);
    EXPECT_EQ(out_length, 13);
    EXPECT_EQ(ret5_as_str, "/path?query=1");

    // AUTHORITY
    part_string = "AUTHORITY";
    const char *ret6 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret6_as_str(ret6, out_length);
    EXPECT_EQ(out_length, 30);
    EXPECT_EQ(ret6_as_str, "userinfo@arrow.apache.org:8080");

    // USERINFO
    part_string = "USERINFO";
    const char *ret7 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret7_as_str(ret7, out_length);
    EXPECT_EQ(out_length, 8);
    EXPECT_EQ(ret7_as_str, "userinfo");

    // REF
    part_string = "REF";
    const char *ret8 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret8_as_str(ret8, out_length);
    EXPECT_EQ(out_length, 8);
    EXPECT_EQ(ret8_as_str, "fragment");

    // REF empty
    input_string = "http://user:pass@host/?#";
    const char *ret18 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    std::string ret18_as_str(ret18, out_length);
    EXPECT_EQ(out_length, 0);
    EXPECT_EQ(ret18_as_str, "");

    // REF not exist
    input_string = "http://user:pass@host";
    const char *ret19 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    EXPECT_EQ(ret19, nullptr);

    // Invalid part
    part_string = "HOST_AND_PORT";
    const char *ret9 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    EXPECT_EQ(ret9, nullptr);

    // Invalid url
    input_string = "abc-abc";
    const char *ret10 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()), &out_length);
    EXPECT_EQ(ret10, nullptr);
  }

  TEST_F(TestParseUrlHolder, TestParseUrlWithQueryPattern) {
    std::shared_ptr<ParseUrlHolder> part_url_holder;
    auto status = ParseUrlHolder::Make(&part_url_holder);
    auto &parse_url = *part_url_holder;
    std::string input_string = "https://userinfo@arrow.apache.org:8080/path?query=1&inx=1_1";
    std::string part_string = "QUERY";
    int32_t out_length = 0;

    // Test query
    std::string query_string = "query";
    const char *ret1 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()),
        query_string.c_str(), static_cast<int32_t>(query_string.length()), &out_length);
    std::string ret1_as_str(ret1, out_length);
    EXPECT_EQ(out_length, 1);
    EXPECT_EQ(ret1_as_str, "1");

    query_string = "inx";
    const char *ret2 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()),
        query_string.c_str(), static_cast<int32_t>(query_string.length()), &out_length);
    std::string ret2_as_str(ret2, out_length);
    EXPECT_EQ(out_length, 3);
    EXPECT_EQ(ret2_as_str, "1_1");

    // Test encoded query
    input_string = "https://use%20r:pas%20s@example.com/dir%20/pa%20th.HTML?query=x%20y&q2=2#Ref%20two";
    query_string = "query";
    const char *ret13 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()),
        query_string.c_str(), static_cast<int32_t>(query_string.length()), &out_length);
    std::string ret13_as_str(ret13, out_length);
    EXPECT_EQ(out_length, 5);
    EXPECT_EQ(ret13_as_str, "x%20y");

    // Invalid pattern
    query_string = "query_pattern";
    const char *ret3 = parse_url(
        &execution_context_, input_string.c_str(), static_cast<int32_t>(input_string.length()),
        part_string.c_str(), static_cast<int32_t>(part_string.length()),
        query_string.c_str(), static_cast<int32_t>(query_string.length()), &out_length);
    EXPECT_EQ(ret3, nullptr);
  }
}  // namespace gandiva
