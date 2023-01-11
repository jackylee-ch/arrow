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

#include <regex>

#include "gandiva/node.h"
#include "gandiva/regex_util.h"
#include <sstream>

using namespace simdjson;

namespace gandiva {

Status JsonHolder::Make(const FunctionNode& node, std::shared_ptr<JsonHolder>* holder) {
  return Make(holder);
}

Status JsonHolder::Make(std::shared_ptr<JsonHolder>* holder) {
  *holder = std::shared_ptr<JsonHolder>(new JsonHolder());
  return Status::OK();
}

error_code handle_types(simdjson_result<ondemand::value> raw_res, std::string* res) {
 switch (raw_res.type()) {
   case ondemand::json_type::number: {
      std::stringstream ss;
      switch (raw_res.get_number_type()) {
        case ondemand::number_type::unsigned_integer: {
          uint64_t num_res;
          auto error = raw_res.get_uint64().get(num_res);
          if (!error) {
            ss << num_res;
            *res = ss.str();
          }
          return error;
        }
        case ondemand::number_type::signed_integer: {
          int64_t num_res;
          auto error = raw_res.get_int64().get(num_res);
          if (!error) {
            ss << num_res;
            *res = ss.str();
          }
          return error;
        }
        case ondemand::number_type::floating_point_number: {
          double num_res;
          auto error = raw_res.get_double().get(num_res);
          if (!error) {
            ss << num_res;
            *res = ss.str();
          }
          return error;
        }
      }
    }
   case ondemand::json_type::string: {
     std::string_view res_view;
     auto error = raw_res.get_string().get(res_view);
     *res = std::string(res_view);
     return error;
    }
   case ondemand::json_type::boolean: {
     bool bool_res = false;
     raw_res.get_bool().get(bool_res);
     if (bool_res) {
       *res = "true";
     } else {
       *res = "false";
     }
     return error_code::SUCCESS;
    }
   case ondemand::json_type::object: {
     // For nested case, e.g., for "{"my": {"hello": 10}}", "$.my" will return an object type.
     auto obj = raw_res.get_object();
     // For the case that result is a json object.
     std::stringstream ss;
     ss << obj;
     *res = ss.str();
     return error_code::SUCCESS;
    }
   case ondemand::json_type::array: {
     auto array_obj = raw_res.get_array();
     // For the case that result is a json object.
     std::stringstream ss;
     ss << array_obj;
     *res = ss.str();
     return error_code::SUCCESS;
    }
   case ondemand::json_type::null: {
     return error_code::UNSUPPORTED_ARCHITECTURE;
    }
  }
}

// This is simple validation by checking whether the obtained result is followed by expected char.
// It is useful in ondemand kind of parsing which can ignore the validation of character following
// closing '"'. This functon is a simple checking. For many cases, even though it returns true, the
// raw json string can still be illegal possibly.
bool check_char(const char* current_position, int check_index) {
  char ending_char = current_position[check_index];
  if (ending_char == ',') {
    return true;
  } else if (ending_char == '}') {
    return true;
  } else if (ending_char == ']') {
    return true;
  } else if (ending_char == ' ' || ending_char == '\r' || ending_char == '\n' || ending_char == '\t') {
    // space, '\r', '\n' or '\t' can precede valid ending char.
    return check_char(current_position, check_index + 1);
  } else {
    return false;
  }
}

const uint8_t* JsonHolder::operator()(gandiva::ExecutionContext* ctx, const std::string& json_str,
 const std::string& json_path, int32_t* out_len) {
  if (json_str.size() == 2 && json_str == "{}") return nullptr;
  if (json_path.length() < 3) {
    return nullptr;
  }

  padded_string padded_input(json_str);

  // Just for json string validation. With ondemand api, when a target field is found, the remaining
  // json string will not be parsed and validated. So we use the below dom api for fully parsing and
  // return null result finally for illegal json string, which is consistent with Spark.
  // This validation can bring much perf. overhead.
  // dom::parser parser_validate;
  // dom::element doc_validate;
  // auto error_validate = parser_validate.parse(padded_input).get(doc_validate);
  // if (error_validate) {
  //   return nullptr;
  // }

  ondemand::parser parser;
  ondemand::document doc;
  try {
    doc = parser.iterate(padded_input);
  } catch (simdjson_error& e) {
    return nullptr;
  }

  // Follow spark's format for specifying a field, e.g., "$.a.b".
  char formatted_json_path[json_path.length() + 1];
  int j = 0;
  for (int i = 0; i < json_path.length(); i++) {
    if (json_path[i] == '$' || json_path[i] == ']' || json_path[i] == '\'') {
      continue;
    } else if (json_path[i] == '[' || json_path[i] == '.') {
      formatted_json_path[j] = '/';
      j++;
    } else {
      formatted_json_path[j] = json_path[i];
      j++;
    }
  }
  formatted_json_path[j] = '\0';

  std::string res;
  error_code error;
  try {
    auto raw_res = doc.at_pointer(formatted_json_path);
    if (raw_res.error() == error_code::NO_SUCH_FIELD) {
      return nullptr;
    }
    error = handle_types(raw_res, &res);
    if (error) {
      return nullptr;
    }
  } catch (...) {
    return nullptr;
  }

  *out_len = res.length();
  if (*out_len == 0) {
    return reinterpret_cast<const uint8_t*>("");
  }

  const char* current_location;
  doc.current_location().get(current_location);
  if (!check_char(current_location, 0)) {
    return nullptr;
  }

  uint8_t* result_buffer = reinterpret_cast<uint8_t*>(ctx->arena()->Allocate(*out_len));
  memcpy(result_buffer, res.data(), *out_len);

  return result_buffer;
}

}  // namespace gandiva
