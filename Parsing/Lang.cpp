/******************************************************************************
 * Copyright (c) 2014-2016 Leandro T. C. Melo (ltcmelo@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 *****************************************************************************/

/*--------------------------*/
/*--- The UaiSo! Project ---*/
/*--------------------------*/

#include "Parsing/Lang.h"
#include <cctype>

using namespace uaiso;

Lang::~Lang()
{}

bool Lang::hasBlockLevelScope() const { return true; }

bool Lang::hasFuncLevelScope() const { return true; }

bool Lang::hasExecutableRecord() const { return false; }

bool Lang::hasNewlineAsTerminator() const { return false; }

bool Lang::isPurelyOO() const { return false; }

bool Lang::requiresReturnTypeInference() const { return false; }

std::string Lang::packageSeparator() const
{
    return ".";
}

std::string Lang::memberAccessOprtr() const
{
    return ".";
}

std::string Lang::funcCallDelim() const
{
    return "(";
}

bool Lang::isIdentFirstChar(char ch) const
{
    return std::isalpha(ch) || ch == '_';
}

bool Lang::isIdentChar(char ch) const
{
    return std::isalnum(ch) || ch == '_';
}

bool Lang::isStrLitEscape(char ch) const
{
    return std::isprint(ch);
}

bool Lang::isStrLitQuote(char ch) const
{
    return ch == '"';
}

bool Lang::hasStrLitJoinEscape() const
{
    return false;
}

std::pair<bool, char> Lang::strLitJoinEscapeMatcher() const
{
    return std::make_pair(false, 0);
}

bool Lang::isOctalPrefix(char ch) const
{
    return ch == 'o' || ch == 'O';
}

bool Lang::isHexPrefix(char ch) const
{
    return ch == 'x' || ch == 'X';
}

bool Lang::isBinPrefix(char ch) const
{
    return ch == 'b' || ch == 'B';
}

bool Lang::isLongSuffix(char ch) const
{
    return ch == 'L' || ch == 'l';
}

bool Lang::isExponent(char ch) const
{
    return ch == 'e' || ch == 'E';
}

bool Lang::hasStrictDecimalPoint() const
{
    return false;
}
