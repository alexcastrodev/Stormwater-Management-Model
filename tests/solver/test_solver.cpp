/*
 *   test_solver.cpp
 *
 *   Regression tests for the SWMM solver library using Boost Test.
 */

#define BOOST_TEST_MODULE "solver"
#include <boost/test/included/unit_test.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "swmm5.h"
#include "swmm_output.h"

#ifdef _WIN32
#include <direct.h>
#define GETCWD _getcwd
#else
#include <unistd.h>
#define GETCWD getcwd
#endif

extern "C" {
    extern int fill_malloc_enabled;
    int fill_malloc_available(void);
}

using namespace std;

namespace {

// 2022-11-27 11:00 to 13:00, reporting every 10 minutes: 12 periods.
const char* TOTAL_DURATION_INP = R"INP([OPTIONS]
FLOW_UNITS           LPS
FLOW_ROUTING         DYNWAVE
START_DATE           11/27/2022
START_TIME           11:00
END_DATE             11/27/2022
END_TIME             13:00
REPORT_STEP          00:10:00
ROUTING_STEP         10

[JUNCTIONS]
J1 10 3 0 0 0
J2 9.5 3 0 0 0

[OUTFALLS]
O1 9 FREE NO

[CONDUITS]
C1 J1 J2 100 0.013 0 0 0 0
C2 J2 O1 100 0.013 0 0 0 0

[XSECTIONS]
C1 CIRCULAR 0.4 0 0 0 1
C2 CIRCULAR 0.4 0 0 0 1

[INFLOWS]
J1 FLOW Q FLOW 1.0 1.0 0

[TIMESERIES]
Q 0 0
Q 0.5 50
Q 1 80
Q 2 0

[REPORT]
SUBCATCHMENTS NONE
NODES ALL
LINKS ALL

)INP";

// Absolute path of a file in the working directory. The tests do not pass relative paths to
// swmm_open: it resolves them with realpath() into a MAXFNAME (260) byte buffer, which glibc
// built with _FORTIFY_SOURCE (the default on Ubuntu) aborts as a buffer overflow.
string in_cwd(const string& name) {
    char cwd[4096];

    BOOST_REQUIRE(GETCWD(cwd, sizeof(cwd)) != nullptr);
    return string(cwd) + "/" + name;
}

void write_file(const string& path, const char* text) {
    ofstream out(path.c_str());
    out << text;
}

void remove_files(const string& base) {
    remove((base + ".inp").c_str());
    remove((base + ".rpt").c_str());
    remove((base + ".out").c_str());
}

// Number of reporting periods saved in an output file
int count_periods(const string& path) {
    SMO_Handle handle = nullptr;
    int periods = -1;

    BOOST_REQUIRE_EQUAL(SMO_init(&handle), 0);
    BOOST_REQUIRE_EQUAL(SMO_open(handle, path.c_str()), 0);
    BOOST_REQUIRE_EQUAL(SMO_getTimes(handle, SMO_numPeriods, &periods), 0);
    SMO_close(&handle);
    return periods;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(test_solver_auto)

// (EndDateTime - StartDateTime) * 86400 evaluates to 7199.9999995809 for this
// window instead of 7200. TotalDuration used to be floor() of that, so the last
// reporting time (7200 s) fell outside the simulation and its period was lost.
BOOST_AUTO_TEST_CASE(LastReportingPeriodIsSaved) {
    const string base = in_cwd("total_duration");

    write_file(base + ".inp", TOTAL_DURATION_INP);
    BOOST_REQUIRE_EQUAL(swmm_run((base + ".inp").c_str(), (base + ".rpt").c_str(),
        (base + ".out").c_str()), 0);

    BOOST_CHECK_EQUAL(count_periods(base + ".out"), 12);
    remove_files(base);
}

BOOST_AUTO_TEST_SUITE_END()
