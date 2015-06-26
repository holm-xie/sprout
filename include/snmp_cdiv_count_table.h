/**
 * @file snmp_counter_table.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <vector>
#include <map>
#include <string>
#include <atomic>

#include "snmp_includes.h"
#include "snmp_time_period_table.h"
#include "logger.h"

#ifndef SNMP_CDIV_COUNT_TABLE_H
#define SNMP_CDIV_COUNT_TABLE_H

// This file contains infrastructure for the CDiv AS diversions table.

namespace SNMP
{

// Storage for the underlying data
struct CDivCount
{
  std::atomic_uint_fast64_t total;
  std::atomic_uint_fast64_t unconditional;
  std::atomic_uint_fast64_t busy;
  std::atomic_uint_fast64_t not_registered;
  std::atomic_uint_fast64_t no_answer;
  std::atomic_uint_fast64_t not_reachable;

  void reset();
};


class CDivCountRow: public TimeBasedRow<CDivCount>
{
public:
  CDivCountRow(int index, View* view):
    TimeBasedRow<CDivCount>(index, view) {};
  ColumnData get_columns();
};

class CDivCountTable: public ManagedTable<CDivCountRow, int>
{
public:
  CDivCountTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
    ManagedTable<CDivCountRow, int>(name,
                                  tbl_oid,
                                  oidlen,
                                  2,
                                  7, // Columns 2-7 should be visible
                                  { ASN_INTEGER }), // Type of the index column
    five_second(5),
    five_minute(300)
  {
    // We have a fixed number of rows, so create them in the constructor.
    add(TimePeriodIndexes::scopePrevious5SecondPeriod);
    add(TimePeriodIndexes::scopeCurrent5MinutePeriod);
    add(TimePeriodIndexes::scopePrevious5MinutePeriod);
  }
 
  void increment_total()
  {
    five_second.get_current()->total++;
    five_minute.get_current()->total++;
  }

  void increment_unconditional()
  {
    five_second.get_current()->unconditional++;
    five_minute.get_current()->unconditional++;
  }

  void increment_busy()
  {
    five_second.get_current()->busy++;
    five_minute.get_current()->busy++;
  }

  void increment_not_registered()
  {
    five_second.get_current()->not_registered++;
    five_minute.get_current()->not_registered++;
  }

  void increment_no_answer()
  {
    five_second.get_current()->no_answer++;
    five_minute.get_current()->no_answer++;
  }

  void increment_not_reachable()
  {
    five_second.get_current()->not_reachable++;
    five_minute.get_current()->not_reachable++;
  }

private: 
  // Map row indexes to the view of the underlying data they should expose
  CDivCountRow* new_row(int index)
  {
    CDivCountRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new CDivCountRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new CDivCountRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new CDivCountRow::PreviousView(&five_minute);
        break;
    }
    return new CDivCountRow(index, view);
  }

  CDivCountRow::CurrentAndPrevious five_second;
  CDivCountRow::CurrentAndPrevious five_minute;
};

}

#endif
