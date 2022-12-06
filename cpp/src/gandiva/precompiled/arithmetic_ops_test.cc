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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "../execution_context.h"
#include "gandiva/precompiled/types.h"

namespace gandiva {

TEST(TestArithmeticOps, TestIsDistinctFrom) {
  EXPECT_EQ(is_distinct_from_timestamp_timestamp(1000, true, 1000, false), true);
  EXPECT_EQ(is_distinct_from_timestamp_timestamp(1000, false, 1000, true), true);
  EXPECT_EQ(is_distinct_from_timestamp_timestamp(1000, false, 1000, false), false);
  EXPECT_EQ(is_distinct_from_timestamp_timestamp(1000, true, 1000, true), false);

  EXPECT_EQ(is_not_distinct_from_int32_int32(1000, true, 1000, false), false);
  EXPECT_EQ(is_not_distinct_from_int32_int32(1000, false, 1000, true), false);
  EXPECT_EQ(is_not_distinct_from_int32_int32(1000, false, 1000, false), true);
  EXPECT_EQ(is_not_distinct_from_int32_int32(1000, true, 1000, true), true);
}

TEST(TestArithmeticOps, TestMod) {
  gandiva::ExecutionContext context;
  EXPECT_EQ(mod_int32_int32(10, 0), 10);

  const double acceptable_abs_error = 0.00000000001;  // 1e-10

  EXPECT_DOUBLE_EQ(mod_float64_float64(reinterpret_cast<gdv_int64>(&context), 2.5, 0.0),
                   0.0);
  // EXPECT_TRUE(context.has_error());
  // EXPECT_EQ(context.get_error(), "divide by zero error");

  context.Reset();
  EXPECT_NEAR(mod_float64_float64(reinterpret_cast<gdv_int64>(&context), 2.5, 1.2), 0.1,
              acceptable_abs_error);
  EXPECT_FALSE(context.has_error());

  context.Reset();
  EXPECT_DOUBLE_EQ(mod_float64_float64(reinterpret_cast<gdv_int64>(&context), 2.5, 2.5),
                   0.0);
  EXPECT_FALSE(context.has_error());

  context.Reset();
  EXPECT_NEAR(mod_float64_float64(reinterpret_cast<gdv_int64>(&context), 9.2, 3.7), 1.8,
              acceptable_abs_error);
  EXPECT_FALSE(context.has_error());
}

TEST(TestArithmeticOps, TestPMod) {
  bool out_valid = false;
  EXPECT_EQ(pmod_int8_int8(10, true, 3, true, &out_valid), 1);
  EXPECT_EQ(out_valid, true);
  EXPECT_EQ(pmod_int16_int16(10, true, 3, true, &out_valid), 1);
  EXPECT_EQ(pmod_int32_int32(10, true, 3, true, &out_valid), 1);
  // Negative input.
  EXPECT_EQ(pmod_int32_int32(-10, true, 3, true, &out_valid), 2);
  EXPECT_EQ(pmod_int32_int32(-5, true, -3, true, &out_valid), -2);

  EXPECT_FLOAT_EQ(pmod_float32_float32(0.5, true, 0.3, true, &out_valid), 0.2);
  EXPECT_FLOAT_EQ(pmod_float32_float32(-1.1, true, 2, true, &out_valid), 0.9);

  EXPECT_DOUBLE_EQ(pmod_float64_float64(0.5, true, 0.3, true, &out_valid), 0.2);
  EXPECT_DOUBLE_EQ(pmod_float64_float64(-1.1, true, 2, true, &out_valid), 0.9);

  // Not valid cases.
  pmod_int32_int32(0, false, 3, false, &out_valid);
  EXPECT_EQ(out_valid, false);
  pmod_int32_int32(10, true, 0, true, &out_valid);
  EXPECT_EQ(out_valid, false);
  pmod_float64_float64(-1.1, true, 0.0, true, &out_valid);
  EXPECT_EQ(out_valid, false);
}

TEST(TestArithmeticOps, TestCompare) {
  EXPECT_EQ(equal_with_nan_float32_float32(NAN, 1.0/0.0), false);
  EXPECT_EQ(equal_with_nan_float32_float32(1.0/0.0, NAN), false);
  EXPECT_EQ(not_equal_with_nan_float32_float32(NAN, 1.0/0.0), true);
  EXPECT_EQ(not_equal_with_nan_float32_float32(1.0/0.0, NAN), true);
}

TEST(TestArithmeticOps, TestDivide) {
  bool out_valid = false;
  EXPECT_EQ(divide_int64_int64(10, true, 0, true, &out_valid), 0);
  EXPECT_EQ(out_valid, false);

  out_valid = false;
  EXPECT_EQ(divide_int64_int64(10, true, 0, false, &out_valid), 0);
  EXPECT_EQ(out_valid, false);

  out_valid = false;
  EXPECT_EQ(divide_int64_int64(10, false, 0, true, &out_valid), 0);
  EXPECT_EQ(out_valid, false);

  out_valid = false;
  EXPECT_EQ(divide_int64_int64(10, true, 2, true, &out_valid), 5);
  EXPECT_EQ(out_valid, true);
}

TEST(TestArithmeticOps, TestDiv) {
  gandiva::ExecutionContext context;
  EXPECT_EQ(div_int64_int64(reinterpret_cast<gdv_int64>(&context), 101, 0), 0);
  // EXPECT_EQ(context.has_error(), true);
  // EXPECT_EQ(context.get_error(), "divide by zero error");
  context.Reset();

  EXPECT_EQ(div_int64_int64(reinterpret_cast<gdv_int64>(&context), 101, 111), 0);
  EXPECT_EQ(context.has_error(), false);
  context.Reset();

  EXPECT_EQ(div_float64_float64(reinterpret_cast<gdv_int64>(&context), 1010.1010, 2.1),
            481.0);
  EXPECT_EQ(context.has_error(), false);
  context.Reset();

  EXPECT_EQ(
      div_float64_float64(reinterpret_cast<gdv_int64>(&context), 1010.1010, 0.00000),
      0.0);
  // EXPECT_EQ(context.has_error(), true);
  // EXPECT_EQ(context.get_error(), "divide by zero error");
  context.Reset();

  EXPECT_EQ(div_float32_float32(reinterpret_cast<gdv_int64>(&context), 1010.1010f, 2.1f),
            481.0f);
  EXPECT_EQ(context.has_error(), false);
  context.Reset();
}

TEST(TestArithmeticOps, TestBitwiseOps) {
  // bitwise AND
  EXPECT_EQ(bitwise_and_int32_int32(0x0147D, 0x17159), 0x01059);
  EXPECT_EQ(bitwise_and_int32_int32(0xFFFFFFCC, 0x00000297), 0x00000284);
  EXPECT_EQ(bitwise_and_int32_int32(0x000, 0x285), 0x000);
  EXPECT_EQ(bitwise_and_int64_int64(0x563672F83, 0x0D9FCF85B), 0x041642803);
  EXPECT_EQ(bitwise_and_int64_int64(0xFFFFFFFFFFDA8F6A, 0xFFFFFFFFFFFF791C),
            0xFFFFFFFFFFDA0908);
  EXPECT_EQ(bitwise_and_int64_int64(0x6A5B1, 0x00000), 0x00000);

  // bitwise OR
  EXPECT_EQ(bitwise_or_int32_int32(0x0147D, 0x17159), 0x1757D);
  EXPECT_EQ(bitwise_or_int32_int32(0xFFFFFFCC, 0x00000297), 0xFFFFFFDF);
  EXPECT_EQ(bitwise_or_int32_int32(0x000, 0x285), 0x285);
  EXPECT_EQ(bitwise_or_int64_int64(0x563672F83, 0x0D9FCF85B), 0x5FBFFFFDB);
  EXPECT_EQ(bitwise_or_int64_int64(0xFFFFFFFFFFDA8F6A, 0xFFFFFFFFFFFF791C),
            0xFFFFFFFFFFFFFF7E);
  EXPECT_EQ(bitwise_or_int64_int64(0x6A5B1, 0x00000), 0x6A5B1);

  // bitwise XOR
  EXPECT_EQ(bitwise_xor_int32_int32(0x0147D, 0x17159), 0x16524);
  EXPECT_EQ(bitwise_xor_int32_int32(0xFFFFFFCC, 0x00000297), 0XFFFFFD5B);
  EXPECT_EQ(bitwise_xor_int32_int32(0x000, 0x285), 0x285);
  EXPECT_EQ(bitwise_xor_int64_int64(0x563672F83, 0x0D9FCF85B), 0x5BA9BD7D8);
  EXPECT_EQ(bitwise_xor_int64_int64(0xFFFFFFFFFFDA8F6A, 0xFFFFFFFFFFFF791C), 0X25F676);
  EXPECT_EQ(bitwise_xor_int64_int64(0x6A5B1, 0x00000), 0x6A5B1);
  EXPECT_EQ(bitwise_xor_int64_int64(0x6A5B1, 0x6A5B1), 0x00000);

  // bitwise NOT
  EXPECT_EQ(bitwise_not_int32(0x00017159), 0xFFFE8EA6);
  EXPECT_EQ(bitwise_not_int32(0xFFFFF226), 0x00000DD9);
  EXPECT_EQ(bitwise_not_int64(0x000000008BCAE9B4), 0xFFFFFFFF7435164B);
  EXPECT_EQ(bitwise_not_int64(0xFFFFFF966C8D7997), 0x0000006993728668);
  EXPECT_EQ(bitwise_not_int64(0x0000000000000000), 0xFFFFFFFFFFFFFFFF);
}

TEST(TestArithmeticOps, TestCastVarhcar) {
  gandiva::ExecutionContext ctx;
  uint64_t ctx_ptr = reinterpret_cast<gdv_int64>(&ctx);
  gdv_int32 out_len = 0;

  const char* out_str = castVARCHAR_int32_int64(ctx_ptr, 88, 11L, &out_len);
  EXPECT_EQ(std::string(out_str, out_len), "88");
  EXPECT_FALSE(ctx.has_error());

  out_str = castVARCHAR_float64_int64(ctx_ptr, 8.712128f, 21L, &out_len);
  EXPECT_EQ(std::string(out_str, out_len), "8.712128");
  EXPECT_FALSE(ctx.has_error());
}

}  // namespace gandiva
