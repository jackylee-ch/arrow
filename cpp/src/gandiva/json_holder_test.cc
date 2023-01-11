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

#include "gandiva/json_holder.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "gandiva/regex_util.h"

namespace gandiva {

class TestJsonHolder : public ::testing::Test {
 protected:
  ExecutionContext execution_context_;
};

TEST_F(TestJsonHolder, TestJson) {
  std::shared_ptr<JsonHolder> json_holder;

  auto status = JsonHolder::Make(&json_holder);
  EXPECT_EQ(status.ok(), true) << status.message();

  auto get_json_object = *json_holder;

  int32_t out_len;

  const uint8_t* data = get_json_object(&execution_context_, R"({"hello": "3.5"})", "$.hello", &out_len);
  EXPECT_EQ(std::string((char*)data, out_len), "3.5");

  data = get_json_object(&execution_context_, R"({"hello": 292222730})", "$.hello", &out_len);
  EXPECT_EQ(std::string((char*)data, out_len), "292222730");

  data = get_json_object(&execution_context_, R"({"hello": -292222730})", "$.hello", &out_len);
  EXPECT_EQ(std::string((char*)data, out_len), "-292222730");


  // test the case that value is not surrended by double quotes.
  data = get_json_object(&execution_context_, R"({"hello": 3.5})", "$.hello", &out_len);
  EXPECT_EQ(out_len, 3);
  EXPECT_EQ(std::string((char*)data, out_len), "3.5");

  data = get_json_object(&execution_context_, R"({"my": {"hello": 3.5}})", "$.my.hello", &out_len);
  EXPECT_EQ(std::string((char*)data, out_len), "3.5");

  // bool case.
  data = get_json_object(&execution_context_, R"({"my": {"hello": true}})", "$.my.hello", &out_len);
  EXPECT_EQ(std::string((char*)data, out_len), "true");

  // no data contained for given field.
  data = get_json_object(&execution_context_, R"({"hello": 3.5})", "$.hi", &out_len);
  EXPECT_EQ(data, nullptr);

  // empty string.
  data = get_json_object(&execution_context_, R"({"hello": ""})", "$.hello", &out_len);
  EXPECT_EQ(out_len, 0);
  EXPECT_EQ(std::string((char*)data, out_len), "");

  // illegal json string.
  data = get_json_object(&execution_context_, R"({"hello"-3.5})", "$.hello", &out_len);
  EXPECT_EQ(data, nullptr);
  
  // field name is incorrectly given.
  data = get_json_object(&execution_context_, R"({"hello": 3.5})", "$hello", &out_len);
  EXPECT_EQ(data, nullptr);

  // field name is not given.
  data = get_json_object(&execution_context_, R"({"hello": 3.5})", "$.", &out_len);
  EXPECT_EQ(data, nullptr);
  
  data = get_json_object(&execution_context_, R"({"name": "fang", "age": 5, "id": "001"})", "$.age", &out_len);
  EXPECT_EQ(out_len, 1);
  EXPECT_EQ(std::string((char*)data, out_len), "5");

  data = get_json_object(&execution_context_, R"({"name": "fang", "age": "5", "id": "001"})", "$.id", &out_len);
  EXPECT_EQ(out_len, 3);
  EXPECT_EQ(std::string((char*)data, out_len), "001");

  out_len = 0;
  data = get_json_object(&execution_context_, R"({"my": {"param": {"name": "fang", "age": "5", "id": "001"}}})", "$.my.param", &out_len);
  std::string expected_res = R"({"name": "fang", "age": "5", "id": "001"})";
  EXPECT_EQ(out_len, expected_res.length());
  EXPECT_EQ(std::string((char*)data, out_len), expected_res);

  // Using bracket to specify json path.
  out_len = 0;
  data = get_json_object(&execution_context_, R"({"my": {"param": {"name": "fang", "age": "5", "id": "001"}}})", "$['my']['param']", &out_len);
  expected_res = R"({"name": "fang", "age": "5", "id": "001"})";
  EXPECT_EQ(out_len, expected_res.length());
  EXPECT_EQ(std::string((char*)data, out_len), expected_res);

  // Start from array index.
  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang", "age": "5", "id": "001"}}}, {"other": "placehoder"}])",
   "$[0]['my']['param']['age']", &out_len);
  EXPECT_EQ(out_len, 1);
  EXPECT_EQ(std::string((char*)data, out_len), "5");

  // Mix bracket and period.
  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang", "age": "5", "id": "001"}}}, {"other": "placehoder"}])",
   "$[0].my.param.age", &out_len);
  EXPECT_EQ(out_len, 1);
  EXPECT_EQ(std::string((char*)data, out_len), "5");

  // Get array result.
  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang"}}}, {"other": ["placeholder1", "placeholder2"]}])",
   "$[1].other", &out_len);
  expected_res = R"(["placeholder1", "placeholder2"])";
  EXPECT_EQ(out_len, expected_res.length());
  EXPECT_EQ(std::string((char*)data, out_len), expected_res);

  // Return array element as result.
  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang"}}}, {"other": ["placeholder1", "placeholder2"]}])",
   "$[1].other[0]", &out_len);
  expected_res = "placeholder1";
  EXPECT_EQ(out_len, expected_res.length());
  EXPECT_EQ(std::string((char*)data, out_len), expected_res);

  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang"}}}, {"other": ["placeholder1", "placeholder2"]}])",
   "$[1].other[1]", &out_len);
  expected_res = "placeholder2";
  EXPECT_EQ(out_len, expected_res.length());
  EXPECT_EQ(std::string((char*)data, out_len), expected_res);

  // The nested quote is viewed as illegal json string.
  out_len = 0;
  data = get_json_object(&execution_context_, R"([{"my": {"param": {"name": "fang"quoted""}}}, {"other": ["placeholder1", "placeholder2"]}])",
   "$[0].my.param.name", &out_len);
  EXPECT_EQ(data, nullptr);
}

}  // namespace gandiva
