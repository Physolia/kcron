/***************************************************************************
 *   ctmonth.h                                                             *
 *   --------------------------------------------------------------------  *
 *   CT Month Header                                                       *
 *   --------------------------------------------------------------------  *
 *   Copyright (C) 1999, Gary Meyer <gary@meyer.net>                       *
 *   --------------------------------------------------------------------  *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 ***************************************************************************/

#ifndef CTMONTH_H
#define CTMONTH_H

// Do not introduce any Qt or KDE dependencies into the "CT"-prefixed classes.
// I want to be able to reuse these classes with another GUI toolkit. -GM 11/99

#include "ctunit.h"
#include <string>

/**
  * Cron table task month.
  */
class CTMonth : public CTUnit<1,12>
{
public:
/**
  * Constructs from a tokenized string.
  */
  CTMonth(const string& tokStr = "");

/**
  * Get natural language description.
  */
  string describe() const;

/**
  * Get month name.
  */
  string getName(const int ndx,
    const bool format = CTMonth::longFormat) const;

private:
  string shortName[13];
  string longName[13];
};

#endif // CTMONTH_H
