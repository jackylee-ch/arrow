# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Split up into several register functions by category to reduce cyclomatic
# complexity (linter)
register_bindings_datetime <- function() {
  register_bindings_datetime_utility()
  register_bindings_datetime_components()
  register_bindings_datetime_conversion()
  register_bindings_duration()
  register_bindings_duration_constructor()
  register_bindings_duration_helpers()
  register_bindings_datetime_parsers()
  register_bindings_datetime_rounding()
}

register_bindings_datetime_utility <- function() {
  register_binding("base::strptime", function(x,
                                              format = "%Y-%m-%d %H:%M:%S",
                                              tz = "",
                                              unit = "ms") {
    # Arrow uses unit for time parsing, strptime() does not.
    # Arrow has no default option for strptime (format, unit),
    # we suggest following format = "%Y-%m-%d %H:%M:%S", unit = MILLI/1L/"ms",
    # (ARROW-12809)

    unit <- make_valid_time_unit(
      unit,
      c(valid_time64_units, valid_time32_units)
    )

    output <- build_expr(
      "strptime",
      x,
      options =
        list(
          format = format,
          unit = unit,
          error_is_null = TRUE
        )
    )

    if (tz == "") {
      tz <- Sys.timezone()
    }

    # if a timestamp does not contain timezone information (i.e. it is
    # "timezone-naive") we can attach timezone information (i.e. convert it into
    # a "timezone-aware" timestamp) with `assume_timezone`
    # if we want to cast to a different timezone, we can only do it for
    # timezone-aware timestamps, not for timezone-naive ones
    if (!is.null(tz)) {
      output <- build_expr(
        "assume_timezone",
        output,
        options =
          list(
            timezone = tz
          )
      )
    }
    output
  })

  register_binding("base::strftime", function(x,
                                              format = "",
                                              tz = "",
                                              usetz = FALSE) {
    if (usetz) {
      format <- paste(format, "%Z")
    }
    if (tz == "") {
      tz <- Sys.timezone()
    }
    # Arrow's strftime prints in timezone of the timestamp. To match R's strftime behavior we first
    # cast the timestamp to desired timezone. This is a metadata only change.
    if (call_binding("is.POSIXct", x)) {
      ts <- Expression$create("cast", x, options = list(to_type = timestamp(x$type()$unit(), tz)))
    } else {
      ts <- x
    }
    Expression$create("strftime", ts, options = list(format = format, locale = check_time_locale()))
  })

  register_binding("lubridate::format_ISO8601", function(x, usetz = FALSE, precision = NULL, ...) {
    ISO8601_precision_map <-
      list(
        y = "%Y",
        ym = "%Y-%m",
        ymd = "%Y-%m-%d",
        ymdh = "%Y-%m-%dT%H",
        ymdhm = "%Y-%m-%dT%H:%M",
        ymdhms = "%Y-%m-%dT%H:%M:%S"
      )

    if (is.null(precision)) {
      precision <- "ymdhms"
    }
    if (!precision %in% names(ISO8601_precision_map)) {
      abort(
        paste(
          "`precision` must be one of the following values:",
          paste(names(ISO8601_precision_map), collapse = ", "),
          "\nValue supplied was: ",
          precision
        )
      )
    }
    format <- ISO8601_precision_map[[precision]]
    if (usetz) {
      format <- paste0(format, "%z")
    }
    Expression$create("strftime", x, options = list(format = format, locale = "C"))
  })

  register_binding("lubridate::is.Date", function(x) {
    inherits(x, "Date") ||
      (inherits(x, "Expression") && x$type_id() %in% Type[c("DATE32", "DATE64")])
  })

  is_instant_binding <- function(x) {
    inherits(x, c("POSIXt", "POSIXct", "POSIXlt", "Date")) ||
      (inherits(x, "Expression") && x$type_id() %in% Type[c("TIMESTAMP", "DATE32", "DATE64")])
  }
  register_binding("lubridate::is.instant", is_instant_binding)
  register_binding("lubridate::is.timepoint", is_instant_binding)

  register_binding("lubridate::is.POSIXct", function(x) {
    inherits(x, "POSIXct") ||
      (inherits(x, "Expression") && x$type_id() %in% Type[c("TIMESTAMP")])
  })

  register_binding("lubridate::date", function(x) {
    build_expr("cast", x, options = list(to_type = date32()))
  })
}

register_bindings_datetime_components <- function() {
  register_binding("lubridate::second", function(x) {
    Expression$create("add", Expression$create("second", x), Expression$create("subsecond", x))
  })

  register_binding("lubridate::wday", function(x,
                                               label = FALSE,
                                               abbr = TRUE,
                                               week_start = getOption("lubridate.week.start", 7),
                                               locale = Sys.getlocale("LC_TIME")) {
    if (label) {
      if (abbr) {
        format <- "%a"
      } else {
        format <- "%A"
      }
      return(Expression$create("strftime", x, options = list(format = format, locale = check_time_locale(locale))))
    }

    Expression$create("day_of_week", x, options = list(count_from_zero = FALSE, week_start = week_start))
  })

  register_binding("lubridate::week", function(x) {
    (call_binding("yday", x) - 1) %/% 7 + 1
  })

  register_binding("lubridate::month", function(x,
                                                label = FALSE,
                                                abbr = TRUE,
                                                locale = Sys.getlocale("LC_TIME")) {
    if (call_binding("is.integer", x)) {
      x <- call_binding(
        "if_else",
        call_binding("between", x, 1, 12),
        x,
        NA_integer_
      )
      if (!label) {
        # if we don't need a label we can return the integer itself (already
        # constrained to 1:12)
        return(x)
      }
      # make the integer into a date32() - which interprets integers as
      # days from epoch (we multiply by 28 to be able to later extract the
      # month with label) - NB this builds a false date (to be used by strftime)
      # since we only know and care about the month
      x <- build_expr("cast", x * 28L, options = cast_options(to_type = date32()))
    }

    if (label) {
      if (abbr) {
        format <- "%b"
      } else {
        format <- "%B"
      }
      return(build_expr("strftime", x, options = list(format = format, locale = check_time_locale(locale))))
    }

    build_expr("month", x)
  })

  register_binding("lubridate::qday", function(x) {
    # We calculate day of quarter by flooring timestamp to beginning of quarter and
    # calculating days between beginning of quarter and timestamp/date in question.
    # Since we use one one-based numbering we add one.
    floored_x <- build_expr("floor_temporal", x, options = list(unit = 9L))
    build_expr("days_between", floored_x, x) + Expression$scalar(1L)
  })

  register_binding("lubridate::am", function(x) {
    hour <- Expression$create("hour", x)
    hour < 12
  })
  register_binding("lubridate::pm", function(x) {
    !call_binding("am", x)
  })
  register_binding("lubridate::tz", function(x) {
    if (!call_binding("is.POSIXct", x)) {
      abort(
        paste0(
          "timezone extraction for objects of class `",
          infer_type(x)$ToString(),
          "` not supported in Arrow"
        )
      )
    }

    x$type()$timezone()
  })
  register_binding("lubridate::semester", function(x, with_year = FALSE) {
    month <- call_binding("month", x)
    semester <- call_binding("if_else", month <= 6, 1L, 2L)
    if (with_year) {
      year <- call_binding("year", x)
      return(year + semester / 10)
    } else {
      return(semester)
    }
  })
}

register_bindings_datetime_conversion <- function() {
  register_binding("lubridate::make_datetime", function(year = 1970L,
                                                        month = 1L,
                                                        day = 1L,
                                                        hour = 0L,
                                                        min = 0L,
                                                        sec = 0,
                                                        tz = "UTC") {

    # ParseTimestampStrptime currently ignores the timezone information (ARROW-12820).
    # Stop if tz other than 'UTC' is provided.
    if (tz != "UTC") {
      arrow_not_supported("Time zone other than 'UTC'")
    }

    x <- call_binding("str_c", year, month, day, hour, min, sec, sep = "-")
    build_expr("strptime", x, options = list(format = "%Y-%m-%d-%H-%M-%S", unit = 0L))
  })

  register_binding("lubridate::make_date", function(year = 1970L,
                                                    month = 1L,
                                                    day = 1L) {
    x <- call_binding("make_datetime", year, month, day)
    build_expr("cast", x, options = cast_options(to_type = date32()))
  })

  register_binding("base::ISOdatetime", function(year,
                                                 month,
                                                 day,
                                                 hour,
                                                 min,
                                                 sec,
                                                 tz = "UTC") {

    # NAs for seconds aren't propagated (but treated as 0) in the base version
    sec <- call_binding(
      "if_else",
      call_binding("is.na", sec),
      0,
      sec
    )

    call_binding("make_datetime", year, month, day, hour, min, sec, tz)
  })

  register_binding("base::ISOdate", function(year,
                                             month,
                                             day,
                                             hour = 12,
                                             min = 0,
                                             sec = 0,
                                             tz = "UTC") {
    call_binding("make_datetime", year, month, day, hour, min, sec, tz)
  })

  register_binding("base::as.Date", function(x,
                                             format = NULL,
                                             tryFormats = "%Y-%m-%d",
                                             origin = "1970-01-01",
                                             tz = "UTC") {
    if (is.null(format) && length(tryFormats) > 1) {
      abort(
        paste(
          "`as.Date()` with multiple `tryFormats` is not supported in Arrow,",
          "consider using the lubridate specialised parsing functions such as, `ymd()`, `ymd()`, etc."
        )
      )
    }

    # base::as.Date() and lubridate::as_date() differ in the way they use the
    # `tz` argument. Both cast to the desired timezone, if present. The
    # difference appears when the `tz` argument is not set: `as.Date()` uses the
    # default value ("UTC"), while `as_date()` keeps the original attribute
    # => we only cast when we want the behaviour of the base version or when
    # `tz` is set (i.e. not NULL)
    if (call_binding("is.POSIXct", x)) {
      x <- build_expr("cast", x, options = cast_options(to_type = timestamp(timezone = tz)))
    }

    binding_as_date(
      x = x,
      format = format,
      tryFormats = tryFormats,
      origin = origin
    )
  })

  register_binding("lubridate::as_date", function(x,
                                                  format = NULL,
                                                  origin = "1970-01-01",
                                                  tz = NULL) {
    # base::as.Date() and lubridate::as_date() differ in the way they use the
    # `tz` argument. Both cast to the desired timezone, if present. The
    # difference appears when the `tz` argument is not set: `as.Date()` uses the
    # default value ("UTC"), while `as_date()` keeps the original attribute
    # => we only cast when we want the behaviour of the base version or when
    # `tz` is set (i.e. not NULL)
    if (call_binding("is.POSIXct", x) && !is.null(tz)) {
      x <- build_expr("cast", x, options = cast_options(to_type = timestamp(timezone = tz)))
    }
    binding_as_date(
      x = x,
      format = format,
      origin = origin
    )
  })

  register_binding("lubridate::as_datetime", function(x,
                                                      origin = "1970-01-01",
                                                      tz = "UTC",
                                                      format = NULL) {
    if (call_binding("is.numeric", x)) {
      delta <- call_binding("difftime", origin, "1970-01-01")
      delta <- build_expr("cast", delta, options = cast_options(to_type = int64()))
      x <- build_expr("cast", x, options = cast_options(to_type = int64()))
      x <- build_expr("+", x, delta)
    }

    if (call_binding("is.character", x) && !is.null(format)) {
      # unit = 0L is the identifier for seconds in valid_time32_units
      x <- build_expr(
        "strptime",
        x,
        options = list(format = format, unit = 0L, error_is_null = TRUE)
      )
    }
    output <- build_expr("cast", x, options = cast_options(to_type = timestamp()))
    build_expr("assume_timezone", output, options = list(timezone = tz))
  })

  register_binding("lubridate::decimal_date", function(date) {
    y <- build_expr("year", date)
    start <- call_binding("make_datetime", year = y, tz = "UTC")
    sofar <- call_binding("difftime", date, start, units = "secs")
    total <- call_binding(
      "if_else",
      build_expr("is_leap_year", date),
      Expression$scalar(31622400L), # number of seconds in a leap year (366 days)
      Expression$scalar(31536000L) # number of seconds in a regular year (365 days)
    )
    y + sofar$cast(int64()) / total
  })

  register_binding("lubridate::date_decimal", function(decimal, tz = "UTC") {
    y <- build_expr("floor", decimal)

    start <- call_binding("make_datetime", year = y, tz = tz)
    seconds <- call_binding(
      "if_else",
      build_expr("is_leap_year", start),
      Expression$scalar(31622400L), # number of seconds in a leap year (366 days)
      Expression$scalar(31536000L) # number of seconds in a regular year (365 days)
    )

    fraction <- decimal - y
    delta <- build_expr("floor", seconds * fraction)
    delta <- make_duration(delta, "s")
    start + delta
  })
}

register_bindings_duration <- function() {
  register_binding("base::difftime", function(time1,
                                              time2,
                                              tz,
                                              units = "secs") {
    if (units != "secs") {
      abort("`difftime()` with units other than `secs` not supported in Arrow")
    }

    if (!missing(tz)) {
      warn("`tz` argument is not supported in Arrow, so it will be ignored")
    }

    # cast to timestamp if time1 and time2 are not dates or timestamp expressions
    # (the subtraction of which would output a `duration`)
    if (!call_binding("is.instant", time1)) {
      time1 <- build_expr("cast", time1, options = cast_options(to_type = timestamp()))
    }

    if (!call_binding("is.instant", time2)) {
      time2 <- build_expr("cast", time2, options = cast_options(to_type = timestamp()))
    }

    # if time1 or time2 are timestamps they cannot be expressed in "s" /seconds
    # otherwise they cannot be added subtracted with durations
    # TODO delete the casting to "us" once
    # https://issues.apache.org/jira/browse/ARROW-16060 is solved
    if (inherits(time1, "Expression") &&
      time1$type_id() %in% Type[c("TIMESTAMP")] && time1$type()$unit() != 2L) {
      time1 <- build_expr("cast", time1, options = cast_options(to_type = timestamp("us")))
    }

    if (inherits(time2, "Expression") &&
      time2$type_id() %in% Type[c("TIMESTAMP")] && time2$type()$unit() != 2L) {
      time2 <- build_expr("cast", time2, options = cast_options(to_type = timestamp("us")))
    }

    # we need to go build the subtract expression instead of `time1 - time2` to
    # prevent complaints when we try to subtract an R object from an Expression
    subtract_output <- build_expr("-", time1, time2)
    build_expr("cast", subtract_output, options = cast_options(to_type = duration("s")))
  })
  register_binding("base::as.difftime", function(x,
                                                 format = "%X",
                                                 units = "secs") {
    # windows doesn't seem to like "%X"
    if (format == "%X" & tolower(Sys.info()[["sysname"]]) == "windows") {
      format <- "%H:%M:%S"
    }

    if (units != "secs") {
      abort("`as.difftime()` with units other than 'secs' not supported in Arrow")
    }

    if (call_binding("is.character", x)) {
      x <- build_expr("strptime", x, options = list(format = format, unit = 0L))
      # we do a final cast to duration ("s") at the end
      x <- make_duration(x$cast(time64("us")), unit = "us")
    }

    # numeric -> duration not supported in Arrow yet so we use int64() as an
    # intermediate step
    # TODO: revisit after ARROW-15862

    if (call_binding("is.numeric", x)) {
      # coerce x to be int64(). it should work for integer-like doubles and fail
      # for pure doubles
      # if we abort for all doubles, we risk erroring in cases in which
      # coercion to int64() would work
      x <- build_expr("cast", x, options = cast_options(to_type = int64()))
    }

    build_expr("cast", x, options = cast_options(to_type = duration(unit = "s")))
  })
}

register_bindings_duration_constructor <- function() {
  register_binding("lubridate::make_difftime", function(num = NULL,
                                                        units = "secs",
                                                        ...) {
    if (units != "secs") {
      abort("`make_difftime()` with units other than 'secs' not supported in Arrow")
    }

    chunks <- list(...)

    # lubridate concatenates durations passed via the `num` argument with those
    # passed via `...` resulting in a vector of length 2 - which is virtually
    # unusable in a dplyr pipeline. Arrow errors in this situation
    if (!is.null(num) && length(chunks) > 0) {
      abort("`make_difftime()` with both `num` and `...` not supported in Arrow")
    }

    if (!is.null(num)) {
      # build duration from num if present
      duration <- num
    } else {
      # build duration from chunks when nothing is passed via ...
      duration <- duration_from_chunks(chunks)
    }

    make_duration(duration, "s")
  })
}

register_bindings_duration_helpers <- function() {
  duration_helpers_map_factory <- function(value, unit) {
    force(value)
    force(unit)
    function(x = 1) make_duration(x * value, unit)
  }

  for (name in names(.helpers_function_map)) {
    register_binding(
      name,
      duration_helpers_map_factory(
        .helpers_function_map[[name]][[1]],
        .helpers_function_map[[name]][[2]]
      )
    )
  }

  register_binding("lubridate::dpicoseconds", function(x = 1) {
    abort("Duration in picoseconds not supported in Arrow.")
  })
}

register_bindings_datetime_parsers <- function() {
  register_binding("lubridate::parse_date_time", function(x,
                                                          orders,
                                                          tz = "UTC",
                                                          truncated = 0,
                                                          quiet = TRUE,
                                                          exact = FALSE) {
    if (!quiet) {
      arrow_not_supported("`quiet = FALSE`")
    }

    if (truncated > 0) {
      if (truncated > (nchar(orders) - 3)) {
        arrow_not_supported(paste0("a value for `truncated` > ", nchar(orders) - 3))
      }
      # build several orders for truncated formats
      orders <- map_chr(0:truncated, ~ substr(orders, start = 1, stop = nchar(orders) - .x))
    }

    if (!inherits(x, "Expression")) {
      x <- Expression$scalar(x)
    }

    if (exact == TRUE) {
      # no data processing takes place & we don't derive formats
      parse_attempts <- build_strptime_exprs(x, orders)
    } else {
      parse_attempts <- attempt_parsing(x, orders = orders)
    }

    coalesce_output <- build_expr("coalesce", args = parse_attempts)

    # we need this binding to be able to handle a NULL `tz`, which, in turn,
    # will be used by bindings such as `ymd()` to return a date or timestamp,
    # based on whether tz is NULL or not
    if (!is.null(tz)) {
      build_expr("assume_timezone", coalesce_output, options = list(timezone = tz))
    } else {
      coalesce_output
    }
  })

  parser_vec <- c(
    "ymd", "ydm", "mdy", "myd", "dmy", "dym", "ym", "my", "yq",
    "ymd_HMS", "ymd_HM", "ymd_H", "dmy_HMS", "dmy_HM", "dmy_H",
    "mdy_HMS", "mdy_HM", "mdy_H", "ydm_HMS", "ydm_HM", "ydm_H"
  )

  parser_map_factory <- function(order) {
    force(order)
    function(x, quiet = TRUE, tz = NULL, locale = NULL, truncated = 0) {
      if (!is.null(locale)) {
        arrow_not_supported("`locale`")
      }
      # Parsers returning datetimes return UTC by default and never return dates.
      if (is.null(tz) && nchar(order) > 3) {
        tz <- "UTC"
      }
      parse_x <- call_binding("parse_date_time", x, order, tz, truncated, quiet)
      if (is.null(tz)) {
        # we cast so we can mimic the behaviour of the `tz` argument in lubridate
        # "If NULL (default), a Date object is returned. Otherwise a POSIXct with
        # time zone attribute set to tz."
        parse_x <- parse_x$cast(date32())
      }
      parse_x
    }
  }

  for (order in parser_vec) {
    register_binding(
      paste0("lubridate::", tolower(order)),
      parser_map_factory(order)
    )
  }

  register_binding("lubridate::fast_strptime", function(x,
                                                        format,
                                                        tz = "UTC",
                                                        lt = FALSE,
                                                        cutoff_2000 = 68L) {
    # `lt` controls the output `lt = TRUE` returns a POSIXlt (which doesn't play
    # well with mutate, for example)
    if (lt) {
      arrow_not_supported("`lt = TRUE` argument")
    }

    # TODO revisit once https://issues.apache.org/jira/browse/ARROW-16596 is done
    if (cutoff_2000 != 68L) {
      arrow_not_supported("`cutoff_2000` != 68L argument")
    }

    parse_attempt_expressions <- list()

    parse_attempt_expressions <- map(
      format,
      ~ build_expr(
        "strptime",
        x,
        options = list(
          format = .x,
          unit = 0L,
          error_is_null = TRUE
        )
      )
    )

    coalesce_output <- build_expr("coalesce", args = parse_attempt_expressions)

    build_expr("assume_timezone", coalesce_output, options = list(timezone = tz))
  })
}

register_bindings_datetime_rounding <- function() {
  register_binding(
    "lubridate::round_date",
    function(x,
             unit = "second",
             week_start = getOption("lubridate.week.start", 7)) {
      opts <- parse_period_unit(unit)
      if (opts$unit == 7L) { # weeks (unit = 7L) need to accommodate week_start
        return(shift_temporal_to_week("round_temporal", x, week_start, options = opts))
      }

      Expression$create("round_temporal", x, options = opts)
    }
  )

  register_binding(
    "lubridate::floor_date",
    function(x,
             unit = "second",
             week_start = getOption("lubridate.week.start", 7)) {
      opts <- parse_period_unit(unit)
      if (opts$unit == 7L) { # weeks (unit = 7L) need to accommodate week_start
        return(shift_temporal_to_week("floor_temporal", x, week_start, options = opts))
      }

      Expression$create("floor_temporal", x, options = opts)
    }
  )

  register_binding(
    "lubridate::ceiling_date",
    function(x,
             unit = "second",
             change_on_boundary = NULL,
             week_start = getOption("lubridate.week.start", 7)) {
      opts <- parse_period_unit(unit)
      if (is.null(change_on_boundary)) {
        change_on_boundary <- ifelse(call_binding("is.Date", x), TRUE, FALSE)
      }
      opts$ceil_is_strictly_greater <- change_on_boundary

      if (opts$unit == 7L) { # weeks (unit = 7L) need to accommodate week_start
        return(shift_temporal_to_week("ceil_temporal", x, week_start, options = opts))
      }

      Expression$create("ceil_temporal", x, options = opts)
    }
  )
}
