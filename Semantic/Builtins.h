/******************************************************************************
 * Copyright (c) 2014-2015 Leandro T. C. Melo (ltcmelo@gmail.com)
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

#ifndef UAISO_BUILTINS_H__
#define UAISO_BUILTINS_H__

#include "Common/Config.h"
#include "Parsing/Token.h"
#include <memory>
#include <vector>

namespace uaiso {

class Func;
class LexemeMap;

/*!
 * \brief The Builtins class
 */
class UAISO_API Builtins
{
public:
    virtual ~Builtins();

    /*!
     * \brief tokenSpell
     * \param tk
     * \return
     *
     * Return the actual spelling of a token in a given language.
     *
     * \remarks Tokens are unified, based on their typical meaning (see
     * Tokens.def). This is a hook to circumvent this issue.
     */
    virtual const char* tokenSpell(Token tk) const;

    using FuncPtr = std::unique_ptr<Func>;

    /*!
     * \brief valueConstructors
     * \return
     *
     * Return a list of all builtin value constructors.
     */
    virtual std::vector<FuncPtr> valueConstructors(LexemeMap* lexemes) const;

    /*!
     * \brief moduleNames
     * \return
     *
     * Return a list of all builtin module names.
     */
    virtual std::vector<std::string> moduleNames() const;
};

} // namespace uaiso

#endif
