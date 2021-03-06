/**
 * @file as_communication_tracker_test.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "as_communication_tracker.h"

#include "mockalarm.h"
#include "test_interposer.hpp"

using ::testing::_;
using ::testing::StrEq;
using ::testing::AtLeast;
using ::testing::Mock;

class MockLog1 : public PDLog1<const char*>
{
public:
  MockLog1() : PDLog1(1, 2, "", "", "", "") {};
  MOCK_CONST_METHOD1(log, void(const char*));
};

class MockLog2 : public PDLog2<const char*, const char*>
{
public:
  MockLog2() : PDLog2(1, 2, "", "", "", "") {};
  MOCK_CONST_METHOD2(log, void(const char*, const char*));
};

class AsCommunicationTrackerTest : public ::testing::Test
{
public:
  AsCommunicationTracker* _comm_tracker;
  MockAlarm* _mock_alarm;
  const MockLog2* _mock_error_log;
  const MockLog1* _mock_ok_log;

  void SetUp()
  {
    AlarmManager am;
    _mock_alarm = new MockAlarm(&am);
    _mock_error_log = new MockLog2();
    _mock_ok_log = new MockLog1();
    _comm_tracker = new AsCommunicationTracker(_mock_alarm,
                                               _mock_error_log,
                                               _mock_ok_log);
  }

  void TearDown()
  {
    delete _comm_tracker;
    delete _mock_alarm;
    delete _mock_error_log;
    delete _mock_ok_log;
  }

  const std::string AS1 = "as1";
  const std::string AS2 = "as2";
};

// Advance time to the next AsCommunicationTracker checking period. This is
// slightly encapsulation breaking (since it means the UT knows how long this
// period is) but is the most practical thing to do in UT.
static void advance_time()
{
  cwtest_advance_time_ms((5 * 60 * 1000) + 1);
}

//
// Testcases.
//
// Note that in these tests for the communication tracker to treat a failed AS
// as having recovered, it needs to see two success calls in subsequent 5
// minute periods. This is because the state of the ASs is only checked when
// you actually calls the communication tracker.
//
// As such each on_success call checks the AS health in
// the previous period. So the first call checks the period in which the AS
// was failing (so does not treat the AS as being healthy) wheras only on the
// second call do we see the AS being healthy.
//
// This looks worse in UT than in real life!
//

// Test that is an AS is healthy, nothing happens.
TEST_F(AsCommunicationTrackerTest, SuccessIsIdempotent)
{
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);
}


// Test that a single AS failure raises the alarm and produces a log that
// includes the failure reason. When the AS is healthy again, the alarm is
// cleared and an "OK" log is produced.
TEST_F(AsCommunicationTrackerTest, SingleAsFailure)
{
  // The AS fails.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(StrEq(AS1), StrEq("Some failure reason")));
  advance_time();
  _comm_tracker->on_failure(AS1, "Some failure reason");

  // The AS starts succeeding again.
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(StrEq(AS1)));

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);
}


// Test that with an ongoing AS failure, the logs/alarms are only raised when
// the failure starts and only cleared when the failure resolves.
TEST_F(AsCommunicationTrackerTest, OngoingAsFailure)
{
  // The AS fails.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(_, _));
  _comm_tracker->on_failure(AS1, "Timeout");

  // The AS is still failed.
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");

  // The AS succeeds.
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(_));

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);
}


// Test that if an AS fails, then succeeds, then fails again, that the alarm is
// raised, cleared and re-raised (same for the logs).
TEST_F(AsCommunicationTrackerTest, FailSucceedFailSucceed)
{
  // The AS fails.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(_, _));
  _comm_tracker->on_failure(AS1, "Timeout");

  // The AS succeeds.
  Mock::VerifyAndClearExpectations(_mock_alarm);
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(_));

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);

  // Clear out our mock object so we correctly check when the AS fails again.
  Mock::VerifyAndClearExpectations(_mock_alarm);

  // The AS fails again.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(_, _));
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");

  // The AS succeeds again.
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(_));

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);

}


// Test that if multiple ASs fail:
// - The alarm is raised when any ASs start failing and only cleared when all
//   succeed.
// - Each AS has its own log and that the "error" and "OK" logs are generated
//   when *that AS* fails or recovers (independent of the other AS). The error
//   logs contain the error cause that is specific to that AS.
TEST_F(AsCommunicationTrackerTest, MultipleAsFailures)
{
  // AS1 fails. AS2 is OK.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(StrEq(AS1), StrEq("Timeout")));
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_success(AS2);

  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_success(AS2);

  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_success(AS2);

  // AS2 starts to fail. AS1 still failed.
  EXPECT_CALL(*_mock_error_log, log(StrEq(AS2), StrEq("Transport Error")));
  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_failure(AS2, "Transport Error");

  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_failure(AS2, "Transport Error");

  advance_time();
  _comm_tracker->on_failure(AS1, "Timeout");
  _comm_tracker->on_failure(AS2, "Transport Error");

  // AS1 recovers. AS2 still failed.
  EXPECT_CALL(*_mock_ok_log, log(StrEq(AS1)));

  advance_time();
  _comm_tracker->on_success(AS1);
  _comm_tracker->on_failure(AS2, "Transport Error");

  advance_time();
  _comm_tracker->on_success(AS1);
  _comm_tracker->on_failure(AS2, "Transport Error");

  // Both ASs now Ok.
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(StrEq(AS2)));

  advance_time();
  _comm_tracker->on_success(AS1);
  _comm_tracker->on_success(AS2);

  advance_time();
  _comm_tracker->on_success(AS1);
  _comm_tracker->on_success(AS2);
}


// Check that the first failure reason is the one that is logged. If another
// reason occurs while the AS is still uncontactable the new reason is not
// logged. However if the AS become contactable then fails again for a
// different reason, the new reason *is* logged.
TEST_F(AsCommunicationTrackerTest, MultipleFailureReasons)
{
  // The AS fails.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(StrEq(AS1), StrEq("Some failure reason")));
  advance_time();
  _comm_tracker->on_failure(AS1, "Some failure reason");

  advance_time();
  _comm_tracker->on_failure(AS1, "Another failure reason");

  // The AS starts succeeding again.
  EXPECT_CALL(*_mock_alarm, clear()).Times(AtLeast(1));
  EXPECT_CALL(*_mock_ok_log, log(StrEq(AS1)));

  advance_time();
  _comm_tracker->on_success(AS1);

  advance_time();
  _comm_tracker->on_success(AS1);

  // The AS starts failing again but for a different reason.
  EXPECT_CALL(*_mock_alarm, set());
  EXPECT_CALL(*_mock_error_log, log(StrEq(AS1), StrEq("Another failure reason")));

  advance_time();
  _comm_tracker->on_failure(AS1, "Another failure reason");
}
