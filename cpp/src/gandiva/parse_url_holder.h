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

#pragma once

#include <memory>
#include <re2/re2.h>
#include <string>
#include <unordered_map>

#include "arrow/status.h"
#include "arrow/util/uri.h"
#include "gandiva/execution_context.h"
#include "gandiva/function_holder.h"
#include "gandiva/node.h"
#include "gandiva/visibility.h"

namespace gandiva {

  /// Function Holder for 'parse_url'
  class GANDIVA_EXPORT ParseUrlHolder: public FunctionHolder {
    public:
    ~ParseUrlHolder() override = default;

    // Invoked by function_holder_registry.h
    static Status Make(const FunctionNode& node, std::shared_ptr<ParseUrlHolder>* holder);

    static Status Make(std::shared_ptr<ParseUrlHolder>* holder);

    const char * operator()(
        ExecutionContext *ctx, const char * url, int32_t url_len,
        const char * part, int32_t part_len, int32_t *out_length) {
      std::string url_string(url, url_len);
      std::string part_string(part, part_len);
      arrow::internal::Uri uri;
      std::string out;

      // Here we skip the query parsing, as urlparser does not support invalid characters in url,
      // which is actually supported in vanilla Spark.
      std::string::size_type start_query_position = url_string.find_first_of("?");
      std::string query_string;
      if (start_query_position != url_string.npos) {
        std::string::size_type end_query_position = url_string.find_last_of("#");
        if (end_query_position != url_string.npos) {
          query_string =
              url_string.substr(start_query_position + 1, end_query_position - start_query_position -1);
          url_string = url_string.replace(
              start_query_position + 1, end_query_position - start_query_position - 1, "query=query");
        } else {
          query_string =
              url_string.substr(start_query_position + 1, url_len - start_query_position -1);
          url_string = url_string.replace(
              start_query_position + 1, url_len - start_query_position - 1, "query=query");
        }
      }
      auto status = uri.Parse(url_string);
      if (!status.ok()) {
        return nullptr;
      }

      if (part_string == "HOST") {
        out = uri.host();
      } else if (part_string == "PATH") {
        out = uri.path();
      } else if (part_string == "QUERY") {
        // consistent with vanilla spark
        if (uri.has_query()) {
          out = query_string;
        }  else {
          return nullptr;
        }
      } else if (part_string == "PROTOCOL") {
        out = uri.scheme();
      } else if (part_string == "FILE") {
        if (uri.has_query()) {
          out = uri.path() + "?" + query_string;
        } else {
          out = uri.path();
        }
      } else if (part_string == "AUTHORITY") {
        if (uri.has_user_info()) {
          out = uri.user_info() + "@" + uri.host();
        } else {
          out = uri.host();
        }
        if (uri.has_port()) {
          out = out + ":" + uri.port_text();
        }
      } else if (part_string == "USERINFO") {
        out = uri.user_info();
      } else if (part_string == "REF") {
        // consistent with vanilla spark
        if (uri.has_fragment()) {
          out = uri.fragment();
        } else {
          return nullptr;
        }
      } else {
        return nullptr;
      }

      *out_length = static_cast<int32_t>(out.length());
      char *result_buffer = reinterpret_cast<char *>(ctx->arena()->Allocate(*out_length));
      if (result_buffer == NULLPTR) {
        ctx->set_error_msg("Could not allocate memory for result! Wrong result may be returned!");
        *out_length = 0;
        return nullptr;
      }
      memcpy(result_buffer, out.data(), *out_length);

      return result_buffer;
    }

    const char * operator()(
        ExecutionContext *ctx, const char * url, int32_t url_len,
        const char * part, int32_t part_len,
        const char * pattern, int32_t pattern_len, int32_t *out_length) {
      std::string url_string(url, url_len);
      std::string part_string(part, part_len);
      std::string pattern_string(pattern, pattern_len);
      arrow::internal::Uri uri;
      std::string out;

      // Here we skip the query parsing, as urlparser does not support invalid character in url,
      // which is actually supported in vanilla Spark.
      std::string::size_type start_query_position = url_string.find_first_of("?");
      std::string query_string;
      if (start_query_position != url_string.npos) {
        std::string::size_type end_query_position = url_string.find_last_of("#");
        if (end_query_position != url_string.npos) {
          query_string =
              url_string.substr(start_query_position + 1, end_query_position - start_query_position -1);
          url_string = url_string.replace(
              start_query_position + 1, end_query_position - start_query_position - 1, "query=query");
        } else {
          query_string =
              url_string.substr(start_query_position + 1, url_len - start_query_position -1);
          url_string = url_string.replace(
              start_query_position + 1, url_len - start_query_position - 1, "query=query");
        }
      }
      auto status = uri.Parse(url_string);
      if (!status.ok()) {
        return nullptr;
      }

      if (part_string != "QUERY" || !uri.has_query()) {
        return nullptr;
      } else {
        RE2 re2("(&|^)" + pattern_string + "=([^&]*)");
        int groups_num = re2.NumberOfCapturingGroups();
        RE2::Arg *args[groups_num];
        for (int i = 0; i < groups_num; i++) {
          args[i] = new RE2::Arg;
        }
        *(args[1]) = &out;
        // Use re2 instead of pattern_ for better performance.
        bool matched = RE2::PartialMatchN(query_string, re2, args, groups_num);
        if (!matched) {
          *out_length = 0;
          return nullptr;
        }

        *out_length = static_cast<int32_t>(out.length());
        char *result_buffer = reinterpret_cast<char *>(ctx->arena()->Allocate(*out_length));
        if (result_buffer == NULLPTR) {
          ctx->set_error_msg("Could not allocate memory for result! Wrong result may be returned!");
          *out_length = 0;
          return nullptr;
        }
        memcpy(result_buffer, out.data(), *out_length);

        return result_buffer;
      }
    }
  };  // namespace gandiva
}
