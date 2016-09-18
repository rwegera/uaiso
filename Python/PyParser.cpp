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

#include "Python/PyParser.h"
#include "Python/PyLexer.h"
#include "Ast/Ast.h"
#include "Common/Assert.h"
#include "Common/Trace__.h"
#include "Common/Util__.h"
#include "Parsing/ParsingContext.h"
#include <tuple>

#define TRACE_NAME "PyParser"

using namespace uaiso;

PyParser::PyParser()
{}

bool PyParser::isTestAhead() const
{
    switch (ahead_) {
    case TK_LAMBDA:
        return true;

    default:
        return isNonLambdaTestAhead();
    }
}

bool PyParser::isNonLambdaTestAhead() const
{
    switch (ahead_) {
    case TK_NOT:
        return true;

    default:
        return isExprAhead();
    }
}

bool PyParser::isExprAhead() const
{
    return isFactorAhead();
}

bool PyParser::isFactorAhead() const
{
    switch (ahead_) {
    case TK_PLUS:
    case TK_MINUS:
    case TK_TILDE:
        return true;

    default:
        return isAtomAhead();
    }
}

bool PyParser::isAtomAhead() const
{
    switch (ahead_) {
    case TK_LPAREN:
    case TK_LBRACKET:
    case TK_LBRACE:
    case TK_BACKTICK:
    case TK_IDENT:
    case TK_COMPLETION:
    case TK_INT_LIT:
    case TK_FLOAT_LIT:
    case TK_STR_LIT:
    case TK_NULL_VALUE:
    case TK_TRUE_VALUE:
    case TK_FALSE_VALUE:
        return true;

    default:
        return false;
    }
}

bool PyParser::isArgAhead() const
{
    switch (ahead_) {
    case TK_STAR:
    case TK_STAR_STAR:
        return true;

    default:
        return isTestAhead();
    }
}

bool PyParser::isSubscriptAhead() const
{
    switch (ahead_) {
    case TK_DOT_DOT_DOT:
    case TK_COLON:
        return true;

    default:
        return isTestAhead();
    }
}

bool PyParser::isNameAhead() const
{
    return ahead_ == TK_IDENT || ahead_ == TK_COMPLETION;
}

std::pair<PyParser::Precedence, std::unique_ptr<BinExprAst>>
PyParser::fetchPrecAhead() const
{
    switch (ahead_) {
    case TK_PIPE:
        return std::make_pair(Precedence::Or, BitOrExprAst::create());

    case TK_CARET:
        return std::make_pair(Precedence::Xor, BitXorExprAst::create());

    case TK_AMPER:
        return std::make_pair(Precedence::And, BitAndExprAst::create());

    case TK_LS_LS:
    case TK_GR_GR:
        return std::make_pair(Precedence::Shift, ShiftExprAst::create());

    case TK_PLUS:
        return std::make_pair(Precedence::Term, AddExprAst::create());

    case TK_MINUS:
        return std::make_pair(Precedence::Term, SubExprAst::create());

    case TK_STAR:
        return std::make_pair(Precedence::Factor, MulExprAst::create());

    case TK_SLASH:
        return std::make_pair(Precedence::Factor, DivExprAst::create());

    case TK_SLASH_SLASH:
        return std::make_pair(Precedence::Factor, DivExprAst::create());

    case TK_PERCENT:
        return std::make_pair(Precedence::Factor, ModExprAst::create());

    default:
        return std::make_pair(Precedence::Zero, std::unique_ptr<BinExprAst>());
    }
}

/*
 * file_input: (NEWLINE | stmt)* ENDMARKER
 */
bool PyParser::parse(Lexer* lexer, ParsingContext* context)
{
    UAISO_ASSERT(lexer, return false);
    UAISO_ASSERT(context && context->fileName(), return false);

    setLexer(lexer);
    setContext(context);
    consumeToken();
    std::unique_ptr<StmtAstList> stmts;
    while (ahead_ != TK_EOP) {
        if (maybeConsume(TK_NEWLINE))
            continue;
        addToList(stmts, parseStmt());
    }

    if (stmts) {
        auto prog = std::unique_ptr<ProgramAst>(newAst<ProgramAst>());
        prog->setStmts(std::move(stmts));
        context->takeAst(std::unique_ptr<Ast>(prog.release()));
        return true;
    }

    return false;
}

/*
 * stmt: simple_stmt | compound_stmt
 * simple_stmt: small_stmt (';' small_stmt)* [';'] NEWLINE
 * small_stmt: expr_stmt | print_stmt  | del_stmt | pass_stmt | flow_stmt |
 *             import_stmt | global_stmt | exec_stmt | assert_stmt
 * compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt |
 *                funcdef | classdef | decorated
 */
std::unique_ptr<StmtAst> PyParser::parseStmt()
{
    switch (ahead_) {
    case TK_PRINT:
    case TK_DELETE:
    case TK_PASS:
    case TK_IMPORT:
    case TK_GLOBAL:
    case TK_EXEC:
    case TK_ASSERT:
    case TK_BREAK:
    case TK_CONTINUE:
    case TK_RETURN:
    case TK_RAISE:
    case TK_YIELD:
        return parseSimpleStmt();

    case TK_IF:
        return parseIfStmt();

    case TK_WHILE:
        return parseWhileStmt();

    case TK_FOR:
        return parseForStmt();

    case TK_TRY:
        return parseTryStmt();

    case TK_WITH:
        return parseWithStmt();

    case TK_DEF:
        return parseFuncDef();

    case TK_CLASS:
        return parseClassDef();

    case TK_AT:
        return parseDecorated();

    default:
        return parseSimpleStmt();
    }
}

/*
 * simple_stmt: small_stmt (';' small_stmt)* [';'] NEWLINE
 */
std::unique_ptr<StmtAst> PyParser::parseSimpleStmt()
{
    auto stmt = parseSmallStmt();
    if (maybeConsume(TK_NEWLINE))
        return stmt;

    auto block = BlockStmtAst::create();
    block->addStmt(std::move(stmt));
    while (maybeConsume(TK_SEMICOLON)) {
        if (ahead_ == TK_NEWLINE || ahead_ == TK_EOP)
            break;
        block->addStmt(parseSmallStmt());
    }

    if (!match(TK_NEWLINE)) {
        DEBUG_TRACE("parseSimpleStmt, skip to TK_NEWLINE\n");
        skipTo(TK_NEWLINE);
    }

    return Stmt(std::move(block));
}

/*
 * small_stmt: expr_stmt | print_stmt  | del_stmt | pass_stmt | flow_stmt |
 *             import_stmt | global_stmt | exec_stmt | assert_stmt
 */
std::unique_ptr<StmtAst> PyParser::parseSmallStmt()
{
    switch (ahead_) {
    case TK_PRINT:
        return parsePrintStmt();

    case TK_DELETE:
        return parseDelStmt();

    case TK_PASS:
        return parsePassStmt();

    case TK_IMPORT:
    case TK_FROM:
        return parseImportStmt();

    case TK_GLOBAL:
        return parseGlobalStmt();

    case TK_EXEC:
        return parseExecStmt();

    case TK_ASSERT:
        return parseAssertStmt();

    case TK_BREAK:
        return parseBreakStmt();

    case TK_CONTINUE:
        return parseContinueStmt();

    case TK_RETURN:
        return parseReturnStmt();

    case TK_RAISE:
        return parseRaiseStmt();

    case TK_YIELD:
        return parseYieldStmt();

    default:
        return parseExprStmt();
    }
}

/*
 * expr_stmt: testlist (augassign (yield_expr|testlist) |
 *                      ('=' (yield_expr|testlist))*)
 * augassign: ('+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' |
 *             '<<=' | '>>=' | '**=' | '//=')
 */
std::unique_ptr<StmtAst> PyParser::parseExprStmt()
{
    auto exprs = parseTestList();
    bool augmented = false;
    while (true) {
        switch (ahead_) {
        case TK_PLUS_EQ:
        case TK_MINUS_EQ:
        case TK_STAR_EQ:
        case TK_SLASH_EQ:
        case TK_PERCENT_EQ:
        case TK_AMPER_EQ:
        case TK_PIPE_EQ:
        case TK_CARET_EQ:
        case TK_LS_LS_EQ:
        case TK_GR_GR_EQ:
        case TK_STAR_STAR_EQ:
        case TK_SLASH_SLASH_EQ:
            augmented = true;
            // Fallthrough

        case TK_EQ: {
            consumeToken();
            auto assign = AssignExprAst::create();
            assign->setOprLoc(prevLoc_);
            assign->setExpr1s(std::move(exprs));
            if (ahead_ == TK_YIELD)
                assign->setExpr2s(ExprAstList::create(parseYieldExpr()));
            else
                assign->setExpr2s(parseTestList());
            exprs = ExprAstList::create(std::move(assign));

            if (augmented || ahead_ != TK_EQ)
                return Stmt(newAst<ExprStmtAst>()->setExprs(std::move(exprs)));

            break;
        }

        default:
            return Stmt(newAst<ExprStmtAst>()->setExprs(std::move(exprs)));
        }
    }
}

/*
 * print_stmt: 'print' ( [ test (',' test)* [','] ] |
 *                       '>>' test [ (',' test)+ [','] ] )
 */
std::unique_ptr<StmtAst> PyParser::parsePrintStmt()
{
    UAISO_ASSERT(ahead_ == TK_PRINT, return Stmt());
    consumeToken();
    auto print = PrintExprAst::create();
    print->setKeyLoc(prevLoc_);

    bool wantTest = false;
    if (maybeConsume(TK_GR_GR)) {
        print->setOprLoc(prevLoc_);
        print->addExpr(parseTest());
        if (!maybeConsume(TK_COMMA))
            return Stmt(newAst<ExprStmtAst>()->addExpr(std::move(print)));

        if (print->exprs())
            print->exprs()->lastSubList()->delim_ = prevLoc_;
        wantTest = true;
    }

    if (wantTest || isTestAhead()) {
        print->addExpr(parseTest());
        if (maybeConsume(TK_COMMA)) {
            if (print->exprs())
                print->exprs()->lastSubList()->delim_ = prevLoc_;
            if (isTestAhead())
                print->mergeExprs(parseTestList());
        }
    }

    return Stmt(newAst<ExprStmtAst>()->addExpr(std::move(print)));
}

/*
 * del_stmt: 'del' exprlist
 */
std::unique_ptr<StmtAst> PyParser::parseDelStmt()
{
    UAISO_ASSERT(ahead_ == TK_DELETE, return Stmt());
    consumeToken();
    auto del = DelExprAst::create();
    del->setKeyLoc(prevLoc_);
    del->setExprs(parseExprList());
    return Stmt(newAst<ExprStmtAst>()->addExpr(std::move(del)));
}

/*
 * pass_stmt: 'pass'
 */
std::unique_ptr<StmtAst> PyParser::parsePassStmt()
{
    UAISO_ASSERT(ahead_ == TK_PASS, return Stmt());
    consumeToken();
    return Stmt(newAst<EmptyStmtAst>()->setKeyLoc(prevLoc_));
}

/*
 * import_stmt: import_name | import_from
 * import_name: 'import' dotted_as_names
 * import_from: 'from' ('.'* dotted_name | '.'+) 'import' sub_import
 */
std::unique_ptr<StmtAst> PyParser::parseImportStmt()
{
    UAISO_ASSERT(ahead_ == TK_IMPORT || ahead_ == TK_FROM, return Stmt());

    switch (ahead_) {
    case TK_IMPORT: {
        consumeToken();
        auto import = ImportGroupDeclAst::create();
        import->setKeyLoc(prevLoc_);
        do {
            if (import->modules_)
                import->modules_->delim_ = prevLoc_;
            auto module = ImportDeclAst::create();
            module->setTarget(newAst<IdentExprAst>()->setName(parseDottedName()));
            if (maybeConsume(TK_AS)) {
                module->setAsLoc(prevLoc_);
                module->setLocalName(parseName());
            }
            import->addModule(std::move(module));
        } while (maybeConsume(TK_COMMA));
        if (!import->modules_)
            fail();

        return Stmt(newAst<DeclStmtAst>()->setDecl(std::move(import)));
    }

    case TK_FROM: {
        consumeToken();
        auto import = ImportGroupDeclAst::create();
        import->setKeyLoc(prevLoc_);

        // DESIGN: Store the dots (relative location info).
        bool wantName = true;
        while (maybeConsume(TK_DOT) || maybeConsume(TK_DOT_DOT_DOT))
            wantName = false;

        // Confusing rules... The thing comming after 'from' might be a module,
        // preceeded or not by dots (relative indication), or dots alone. In
        // this latter case, specifying the folder of the upcoming 'import'.
        // Details in PEP 0328:
        //
        // package/
        //     __init__.py
        //     subpackage1/
        //         __init__.py
        //         moduleX.py
        //         moduleY.py
        //     subpackage2/
        //         __init__.py
        //         moduleZ.py
        //     moduleA.py
        //
        // from .moduleY import spam
        // from .moduleY import spam as ham
        // from . import moduleY
        // from ..subpackage1 import moduleY
        // from ..subpackage2.moduleZ import eggs
        // from ..moduleA import foo
        // from ...package import bar
        // from ...sys import path
        // from .. import moduleY

        if (wantName || isNameAhead()) {
            // A selective import, items specified after 'import'.
            auto module = ImportDeclAst::create();
            module->setTarget(newAst<IdentExprAst>()->setName(parseDottedName()));
            match(TK_IMPORT);
            module->setLDelimLoc(prevLoc_);
            if (maybeConsume(TK_STAR))
                module->setMode(SimpleNameAst::create(prevLoc_));
            else
                module->setSelections(parseSubImports(true));
            import->addModule(std::move(module));
        } else {
            // An "ordinary" (non-selective) import, 'from' is just to indicate
            // the module after 'import' is relative.
            match(TK_IMPORT);
            import->setHintLoc(prevLoc_);
            import->setModules(parseSubImports(false));
        }

        return Stmt(newAst<DeclStmtAst>()->setDecl(std::move(import)));
    }

    default:
        fail();
        return Stmt();
    }
}

/*
 * sub_import: ('*' | '(' import_as_names ')' | import_as_names)
 * import_as_name: NAME ['as' NAME]
 * import_as_names: import_as_name (',' import_as_name)* [',']
 */
std::unique_ptr<DeclAstList> PyParser::parseSubImports(bool selective)
{
    bool wantParen = false;
    switch (ahead_) {
    case TK_LPAREN:
        consumeToken();
        wantParen = true;
        // Fallthrough

    default:
        DeclList decls;
        if (selective) {
            do {
                if (decls)
                    decls->delim_ = prevLoc_;
                auto item = ImportSelectionDeclAst::create();
                item->setActualName(parseName());
                if (maybeConsume(TK_AS)) {
                    item->setAsLoc(prevLoc_);
                    item->setAlternateName(parseName());
                }
                addToList(decls, std::move(item));
            } while (maybeConsume(TK_COMMA));
        } else {
            do {
                if (decls)
                    decls->delim_ = prevLoc_;
                auto module = ImportDeclAst::create();
                module->setTarget(newAst<IdentExprAst>()->setName(parseName()));
                if (maybeConsume(TK_AS)) {
                    module->setAsLoc(prevLoc_);
                    module->setLocalName(parseName());
                }
                addToList(decls, std::move(module));
            } while (maybeConsume(TK_COMMA));
        }
        if (wantParen && !match(TK_RPAREN)) {
            DEBUG_TRACE("parseSubImports, skip to TK_RPAREN\n");
            skipTo(TK_RPAREN);
        }
        if (!decls)
            fail();

        return decls;
    }
}

/*
 * global_stmt: 'global' NAME (',' NAME)*
 */
std::unique_ptr<StmtAst> PyParser::parseGlobalStmt()
{
    UAISO_ASSERT(ahead_ == TK_GLOBAL, return Stmt());
    consumeToken();
    auto group = VarGroupDeclAst::create();
    group->setSpec(newAst<InferredSpecAst>());
    group->setKeyLoc(prevLoc_);
    do {
        if (group->decls_)
            group->decls_->delim_ = prevLoc_;
        group->addDecl(newAst<VarDeclAst>()->setName(parseName()));
    } while (maybeConsume(TK_COMMA));
    if (!group->decls())
        fail();

    return Stmt(newAst<DeclStmtAst>()->setDecl(std::move(group)));
}

/*
 * exec_stmt: 'exec' expr ['in' test [',' test]]
 */
std::unique_ptr<StmtAst> PyParser::parseExecStmt()
{
    UAISO_ASSERT(ahead_ == TK_EXEC, return Stmt());
    consumeToken();
    auto eval = EvalStmtAst::create();
    eval->setKeyLoc(prevLoc_);
    eval->setExpr(parseExpr());

    // TODO: Model remaining items.
    if (maybeConsume(TK_IN)) {
        parseTest();
        if (maybeConsume(TK_COMMA))
            parseTest();
    }

    return Stmt(std::move(eval));
}

/*
 * assert_stmt: 'assert' test [',' test]
 */
std::unique_ptr<StmtAst> PyParser::parseAssertStmt()
{
    UAISO_ASSERT(ahead_ == TK_ASSERT, return Stmt());
    consumeToken();
    auto expr = AssertExprAst::create();
    expr->setKeyLoc(prevLoc_);
    expr->setExpr(parseTest());

    // TODO: Generalize "message" from AssertExprAst to "action".
    if (maybeConsume(TK_COMMA))
        parseTest();

    return Stmt(newAst<ExprStmtAst>()->addExpr(std::move(expr)));
}

/*
 * if_stmt: 'if' if_else
 */
std::unique_ptr<StmtAst> PyParser::parseIfStmt()
{
    UAISO_ASSERT(ahead_ == TK_IF, return Stmt());
    consumeToken();
    return parseIfElseIfStmt();
}

/*
 * if_else: test ':' suite ('elif' test ':' suite)* ['else' ':' suite]
 *
 * Note: 'elif' parsed as 'else' 'if'.
 */
std::unique_ptr<StmtAst> PyParser::parseIfElseIfStmt()
{
    auto ef = IfStmtAst::create();
    ef->setIfLoc(prevLoc_);
    ef->setExpr(parseTest());
    match(TK_COLON);
    ef->setThen(parseSuite());
    if (maybeConsume(TK_ELIF)) {
        ef->setElseLoc(prevLoc_);
        ef->setNotThen(parseIfElseIfStmt());
    } else if (maybeConsume(TK_ELSE)) {
        ef->setElseLoc(prevLoc_);
        match(TK_COLON);
        ef->setNotThen(parseSuite());
    }

    return Stmt(std::move(ef));
}

/*
 * while_stmt: 'while' test ':' suite ['else' ':' suite]
 */
std::unique_ptr<StmtAst> PyParser::parseWhileStmt()
{
    UAISO_ASSERT(ahead_ == TK_WHILE, return Stmt());
    consumeToken();
    auto whyle = WhileStmtAst::create();
    whyle->setWhileLoc(prevLoc_);
    whyle->setExpr(parseTest());
    match(TK_COLON);
    whyle->setStmt(parseSuite());

    if (maybeConsume(TK_ELSE)) {
        match(TK_COLON);
        // DESIGN: WhileStmtAst needs an else clause.
        parseSuite();
    }

    return Stmt(std::move(whyle));
}

/*
 * for_stmt: 'for' exprlist 'in' testlist ':' suite ['else' ':' suite]
 */
std::unique_ptr<StmtAst> PyParser::parseForStmt()
{
    UAISO_ASSERT(ahead_ == TK_FOR, return Stmt());
    consumeToken();
    auto four = ForeachStmtAst::create();

    // Convert the exprs (when plain identifiers) into var decls.
    auto exprs = parseExprList();
    auto group = VarGroupDeclAst::create();
    group->setSpec(newAst<InferredSpecAst>());
    for (auto expr : *exprs) {
        if (expr->kind() != Ast::Kind::IdentExpr)
            continue;
        auto name = IdentExpr_Cast(expr)->name_.release();
        group->addDecl(newAst<VarDeclAst>()->setName(name));
    }
    four->setDecl(std::move(group));

    match(TK_IN);
    auto test = parseTestList();
    if (test) {
        // DESIGN: Extend ForeachStmtAst to allow exprs? For now, take the first one.
        four->setExpr(std::move(test->detachHead().first));
    }
    match(TK_COLON);
    four->setStmt(parseSuite());

    if (maybeConsume(TK_ELSE)) {
        match(TK_COLON);
        // DESIGN: ForeachStmtAst needs an else clause.
        parseSuite();
    }

    return Stmt(std::move(four));
}

/*
 * try_stmt: ('try' ':' suite
 *            ((except_clause ':' suite)+
 *             ['else' ':' suite]
 *             ['finally' ':' suite] |
 *             'finally' ':' suite))
 * except_clause: 'except' [test [('as' | ',') test]]
 */
std::unique_ptr<StmtAst> PyParser::parseTryStmt()
{
    UAISO_ASSERT(ahead_ == TK_TRY, return Stmt());
    consumeToken();
    auto trie = TryStmtAst::create();
    trie->setKeyLoc(prevLoc_);
    match(TK_COLON);
    trie->setStmt(parseSuite());

    bool seenElse = false;
    while (true) {
        switch (ahead_) {
        case TK_EXCEPT: {
            consumeToken();
            auto catche = CatchClauseStmtAst::create();
            catche->setKeyLoc(prevLoc_);
            if (isTestAhead()) {
                auto test = parseTest();
                if (test) {
                    auto group = ParamGroupDeclAst::create();
                    if (test->kind() != Ast::Kind::IdentExpr) {
                        // TODO: This is an error, right?
                    } else {
                        group->setSpec(newAst<NamedSpecAst>()->setName(
                                IdentExpr_Cast(test.get())->name_.release()));
                    }
                    if (maybeConsume(TK_AS) || maybeConsume(TK_COMMA)) {
                        auto ident = parseTest();
                        if (ident) {
                            if (ident->kind() != Ast::Kind::IdentExpr) {
                                context_->trackReport(Diagnostic::IdentifierExpected,
                                                      prevLoc_);
                            } else {
                                group->addDecl(newAst<ParamDeclAst>()
                                    ->setName(IdentExpr_Cast(ident.get())->name_.release()));
                            }
                        }
                    }
                    catche->setDecl(std::move(group));
                }
            }
            match(TK_COLON);
            catche->setStmt(parseSuite());
            trie->addCatch(std::move(catche));
            break;
        }

        case TK_FINALLY: {
            consumeToken();
            auto finaly = FinallyClauseStmtAst::create();
            finaly->setKeyLoc(prevLoc_);
            match(TK_COLON);
            finaly->setStmt(parseSuite());
            trie->setFinal(std::move(finaly));
            return Stmt(std::move(trie));
        }

        case TK_ELSE: {
            if (!trie->catchs() || seenElse) {
                // TODO: Once else is added to stmt, check against it (no seenElse).
                fail();
            } else {
                consumeToken();
                match(TK_COLON);
                // DESIGN: ForeachStmtAst needs an else clause.
                parseSuite();
                seenElse = true;
            }
            break;
        }

        default:
            // Check for 'except' only ('finally' always returns).
            if (!trie->catchs())
                fail();
            return Stmt(std::move(trie));
        }
    }
}

/*
 * with_stmt: 'with' with_item (',' with_item)* ':' suite
 */
std::unique_ptr<StmtAst> PyParser::parseWithStmt()
{
    UAISO_ASSERT(ahead_ == TK_WITH, return Stmt());
    consumeToken();
    auto with = WithStmtAst::create();
    with->setKeyLoc(prevLoc_);
    with->setExprs(parseList<ExprAstList>(TK_COMMA,
                                          &PyParser::isTestAhead,
                                          &PyParser::parseWithItem,
                                          false).first);
    if (!with->exprs())
        fail();

    match(TK_COLON);
    with->setStmt(parseSuite());

    return Stmt(std::move(with));
}

/*
 * with_item: test ['as' expr]
 */
std::unique_ptr<ExprAst> PyParser::parseWithItem()
{
    auto test = parseTest();
    if (maybeConsume(TK_AS)) {
        auto assign = AssignExprAst::create();
        assign->setOprLoc(prevLoc_);
        assign->addExpr1(std::move(test));
        assign->addExpr2(parseExpr());
        return Expr(std::move(assign));
    }

    return test;
}

/*
 * varargslist: (fpdef ['=' test] ',')*
 *              ('*' NAME [',' '**' NAME] | '**' NAME) |
 *              fpdef ['=' test] (',' fpdef ['=' test])* [',']
 * fpdef: NAME | '(' fplist ')'
 * fplist: fpdef (',' fpdef)* [',']
 */
std::unique_ptr<DeclAst> PyParser::parseVarArgsList(bool wantParen)
{
    auto clause = ParamClauseDeclAst::create();
    if (wantParen) {
        match(TK_LPAREN);
        clause->setLDelimLoc(prevLoc_);
    }

    bool seenStar = false;
    bool takeComma = true;
    do {
        if (clause->decls_)
            clause->decls_->lastSubList()->delim_ = prevLoc_;

        auto group = ParamGroupDeclAst::create();
        group->setSpec(newAst<InferredSpecAst>());
        switch (ahead_) {
        case TK_COMPLETION:
        case TK_IDENT: {
            if (seenStar)
                break;
            auto name = parseName();
            if (maybeConsume(TK_EQ)) {
                auto param = std::unique_ptr<ParamDeclAst__<
                        ParamVariadic__Empty__, ParamDefaultArg__>>(newAst<
                        ParamDeclAst__<ParamVariadic__Empty__, ParamDefaultArg__>>());
                param->setAssignLoc(prevLoc_);
                param->setName(std::move(name));
                param->setDefaultArg(parseTest().release());
                group->addDecl(std::move(param));
            } else {
                group->addDecl(newAst<ParamDeclAst>()->setName(std::move(name)));
            }
            clause->addDecl(std::move(group));
            break;
        }

        case TK_STAR: {
            if (seenStar)
                break;
            seenStar = true;
            consumeToken();
            clause->addDecl(completeParam(std::move(group)));
            break;
        }

        case TK_STAR_STAR: {
            consumeToken();
            clause->addDecl(completeParam(std::move(group)));
            // Fallthrough
        }

        default:
            takeComma = false;
            break;
        }
    } while (takeComma && maybeConsume(TK_COMMA));

    if (wantParen && !match(TK_RPAREN)) {
        DEBUG_TRACE("parseFuncDef, skip to TK_RPAREN\n");
        skipTo(TK_RPAREN);
    }

    return Decl(std::move(clause));
}

/*
 * funcdef: 'def' NAME parameters ':' suite
 * parameters: '(' [varargslist] ')'
 */
std::unique_ptr<StmtAst> PyParser::parseFuncDef()
{
    UAISO_ASSERT(ahead_ == TK_DEF, return Stmt());
    consumeToken();
    auto spec = std::unique_ptr<FuncSpecAst__<>>(newAst<FuncSpecAst__<>>());
    spec->setKeyLoc(prevLoc_);
    auto decl = FuncDeclAst::create();
    decl->setName(parseName());
    spec->setParam(parseVarArgsList(true));

    match(TK_COLON);
    spec->setLDelimLoc(prevLoc_);
    decl->setStmt(parseSuite());
    decl->setSpec(std::move(spec));

    return Stmt(newAst<DeclStmtAst>()->setDecl(std::move(decl)));
}

/*
 * classdef: 'class' NAME ['(' [testlist] ')'] ':' suite
 */
std::unique_ptr<StmtAst> PyParser::parseClassDef()
{
    UAISO_ASSERT(ahead_ == TK_CLASS, return Stmt());
    consumeToken();
    auto spec = RecordSpecAst::create();
    spec->setKeyLoc(prevLoc_);
    auto decl = RecordDeclAst::create();
    decl->setName(parseName());

    if (maybeConsume(TK_LPAREN)) {
        auto bases = parseTestList();
        for (auto base : *bases) {
            if (base->kind() != Ast::Kind::IdentExpr)
                continue;
            auto name = IdentExpr_Cast(base)->name_.release();
            spec->addBase(newAst<BaseDeclAst>()->setName(name));
        }
        if (!match(TK_RPAREN)) {
            DEBUG_TRACE("parseClassDef, skip to TK_RPAREN\n");
            skipTo(TK_RPAREN);
        }
    }

    match(TK_COLON);
    spec->setLDelimLoc(prevLoc_);
    spec->setProto(parseSuite());
    decl->setSpec(std::move(spec));

    return Stmt(newAst<DeclStmtAst>()->setDecl(std::move(decl)));
}

/*
 * decorated: decorators (classdef | funcdef)
 * decorators: decorator+
 * decorator: '@' dotted_name [ '(' [arglist] ')' ] NEWLINE
 */
std::unique_ptr<StmtAst> PyParser::parseDecorated()
{
    UAISO_ASSERT(ahead_ == TK_AT, return Stmt());
    consumeToken();
    do {
        // TODO: Work on decorators.
        parseDottedName();
        if (maybeConsume(TK_LPAREN)) {
            parseArgList();
            if (!match(TK_RPAREN)) {
                DEBUG_TRACE("parseDecorated, skip to TK_RPAREN\n");
                skipTo(TK_RPAREN);
            }
        }
        if (!match(TK_NEWLINE)) {
            DEBUG_TRACE("parseDecorated, skip to TK_NEWLINE\n");
            skipTo(TK_NEWLINE);
        }
    } while (maybeConsume(TK_AT));

    switch (ahead_) {
    case TK_CLASS:
        return parseClassDef();

    case TK_DEF:
        return parseFuncDef();

    default:
        fail();
        return Stmt();
    }
}

/*
 * continue_stmt: 'continue'
 */
std::unique_ptr<StmtAst> PyParser::parseContinueStmt()
{
    UAISO_ASSERT(ahead_ == TK_CONTINUE, return Stmt());
    consumeToken();
    return Stmt(newAst<ContinueStmtAst>()->setKeyLoc(prevLoc_));
}

/*
 * break_stmt: 'break'
 */
std::unique_ptr<StmtAst> PyParser::parseBreakStmt()
{
    UAISO_ASSERT(ahead_ == TK_BREAK, return Stmt());
    consumeToken();
    return Stmt(newAst<BreakStmtAst>()->setKeyLoc(prevLoc_));
}

/*
 * yield_stmt: yield_expr
 */
std::unique_ptr<StmtAst> PyParser::parseYieldStmt()
{
    return Stmt(newAst<YieldStmtAst>()->setExpr(parseYieldExpr()));
}

/*
 * raise_stmt: 'raise' [test [',' test [',' test]]]
 */
std::unique_ptr<StmtAst> PyParser::parseRaiseStmt()
{
    UAISO_ASSERT(ahead_ == TK_RAISE, return Stmt());
    consumeToken();
    auto stmt = ThrowStmtAst::create();
    stmt->setKeyLoc(prevLoc_);
    if (isTestAhead()) {
        stmt->setExpr(parseTest());

        // TODO: Model remaining exprs.
        if (maybeConsume(TK_COMMA)) {
            parseTest();
            if (maybeConsume(TK_COMMA))
                parseTest();
        }
    }

    return Stmt(std::move(stmt));
}

/*
 * return_stmt: 'return' [testlist]
 */
std::unique_ptr<StmtAst> PyParser::parseReturnStmt()
{
    UAISO_ASSERT(ahead_ == TK_RETURN, return Stmt());
    consumeToken();
    auto stmt = ReturnStmtAst::create();
    stmt->setKeyLoc(prevLoc_);
    if (isTestAhead())
        stmt->setExprs(parseTestList());
    return Stmt(std::move(stmt));
}

/*
 * suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT
 */
std::unique_ptr<StmtAst> PyParser::parseSuite()
{
    if (!maybeConsume(TK_NEWLINE))
        return parseSimpleStmt();

    // Special handling that allows triggering of completion with a more
    // relaxed location requirement, where an indent would be expected.
    if (ahead_ == TK_COMPLETION) {
        auto ident = IdentExprAst::create();
        ident->setName(parseName());
        return Stmt(newAst<ExprStmtAst>()->addExpr(std::move(ident)));
    }

    match(TK_INDENT);
    auto block = BlockStmtAst::create();
    block->addStmt(parseStmt());
    while (!maybeConsume(TK_DEDENT) && ahead_ != TK_EOP)
        block->addStmt(parseStmt());

    return Stmt(std::move(block));
}

/*
 * exprlist: expr (',' expr)* [',']
 */
std::unique_ptr<ExprAstList> PyParser::parseExprList()
{
    return parseList<ExprAstList>(TK_COMMA,
                                  &PyParser::isExprAhead,
                                  &PyParser::parseExpr).first;
}

/*
 * test: or_test ['if' or_test 'else' test] | lambdef
 */
std::unique_ptr<ExprAst> PyParser::parseTest()
{
    if (ahead_ == TK_LAMBDA)
        return parseLambdaDef();

    auto orTest = parseOrTest();
    if (maybeConsume(TK_IF)) {
        // Must be a conditional.
        auto condExpr = TerExprAst::create();
        condExpr->setYes(std::move(orTest));
        condExpr->setQuestionLoc(prevLoc_);
        condExpr->setCond(parseOrTest());
        match(TK_ELSE);
        condExpr->setDelimLoc(prevLoc_);
        condExpr->setNo(parseTest());
        orTest = std::move(condExpr);
    }

    return orTest;
}

/*
 * old_test: or_test | old_lambdef
 */
std::unique_ptr<ExprAst> PyParser::parseOldTest()
{
    if (ahead_ == TK_LAMBDA)
        return parseOldLambdaDef();
    return parseOrTest();
}

/*
 * testlist: test (',' test)* [',']
 */
std::unique_ptr<ExprAstList> PyParser::parseTestList()
{
    return parseList<ExprAstList>(TK_COMMA,
                                  &PyParser::isTestAhead,
                                  &PyParser::parseTest).first;
}

/*
 * testlist1: test (',' test)*
 */
std::unique_ptr<ExprAstList> PyParser::parseTestList1()
{
    return parseList<ExprAstList>(TK_COMMA,
                                  &PyParser::isTestAhead,
                                  &PyParser::parseTest,
                                  false).first;
}

/*
 * testlist_safe: old_test [(',' old_test)+ [',']]
 */
std::unique_ptr<ExprAstList> PyParser::parseTestListSafe()
{
    // BUG: The trailing comma is allowed only for non-singleton lists. This
    // not handled by the helper parseList function currently.

    return parseList<ExprAstList>(TK_COMMA,
                                  &PyParser::isTestAhead,
                                  &PyParser::parseOldTest).first;
}

/*
 * argument: test [comp_for] | test '=' test
 */
std::unique_ptr<ExprAst> PyParser::parseArg()
{
    auto test = parseTest();
    switch (ahead_) {
    case TK_FOR: {
        auto listCompre = parseCompFor(ListCompreExprAst::create());
        listCompre->setExpr(std::move(test));
        return Expr(std::move(listCompre));
    }

    case TK_EQ:
        consumeToken();
        return completeAssignExpr(std::move(test), &PyParser::parseTest);

    default:
        return test;
    }
}

/*
 * arglist: (argument ',')* (argument [','] |
 *                           '*' test (',' argument)* [',' '**' test] |
 *                           '**' test)
 */
std::unique_ptr<ExprAstList> PyParser::parseArgList()
{
    ExprList args;
    if (isTestAhead()) {
        auto p = parseList<ExprAstList>(TK_COMMA, &PyParser::isTestAhead,
                                        &PyParser::parseArg);
        args = std::move(p.first);
        // If there's no trailing comma, this arg must be the last one.
        if (!p.second)
            return args;
    }

    bool wantStarStar = false;
    if (maybeConsume(TK_STAR)) {
        auto unpack = UnpackExprAst::create();
        unpack->setKeyLoc(prevLoc_);
        unpack->setExpr(parseTest());
        addToList(args, std::move(unpack));

        if (maybeConsume(TK_COMMA)) {
            if (args)
                args->lastSubList()->delim_ = prevLoc_;
            if (isTestAhead()) {
                auto p = parseList<ExprAstList>(TK_COMMA, &PyParser::isTestAhead,
                                                &PyParser::parseArg);
                mergeList(args, std::move(p.first));
                if (p.second)
                    wantStarStar = true;
            } else {
                wantStarStar = true;
            }
        }
    }

    if (maybeConsume(TK_STAR_STAR)) {
        auto unpack = UnpackExprAst::create();
        unpack->setKeyLoc(prevLoc_);
        unpack->setExpr(parseTest());
        addToList(args, std::move(unpack));
    } else if (wantStarStar) {
        fail();
    }

    return args;
}

/*
 * comp_iter: comp_for | comp_if
 * comp_for: 'for' exprlist 'in' or_test [comp_iter]
 */
std::unique_ptr<ListCompreExprAst>
PyParser::parseCompFor(std::unique_ptr<ListCompreExprAst> listCompre)
{
    UAISO_ASSERT(ahead_ == TK_FOR, return listCompre);
    UAISO_ASSERT(listCompre, return listCompre);

    match(TK_FOR);
    listCompre->addGen(newAst<GeneratorAst>());
    auto gen = listCompre->gens()->back();
    gen->setPatterns(parseExprList());

    match(TK_IN);
    gen->setOprLoc(prevLoc_);
    gen->setRange(parseOrTest());

    return completeListCompre(std::move(listCompre),
                              &PyParser::parseCompFor, &PyParser::parseCompIf);
}

/*
 * list_iter: list_for | list_if
 * list_for: 'for' exprlist 'in' testlist_safe [list_iter]
 */
std::unique_ptr<ListCompreExprAst>
PyParser::parseListFor(std::unique_ptr<ListCompreExprAst> listCompre)
{
    UAISO_ASSERT(ahead_ == TK_FOR, return listCompre);
    UAISO_ASSERT(listCompre, return listCompre);

    match(TK_FOR);
    listCompre->addGen(newAst<GeneratorAst>());
    auto gen = listCompre->gens()->back();
    gen->setPatterns(parseExprList());

    match(TK_IN);
    gen->setOprLoc(prevLoc_);

    auto tests = parseTestListSafe();
    if (tests) {
        // DESIGN: An expr list as the expr of a list comprehension? Wrap as a
        // comma expr?
        gen->setRange(std::move(tests->detachHead().first));
    }

    return completeListCompre(std::move(listCompre),
                              &PyParser::parseListFor, &PyParser::parseListIf);
}

/*
 * comp_iter: comp_for | comp_if
 * comp_if: 'if' old_test [comp_iter]
 */
std::unique_ptr<ListCompreExprAst>
PyParser::parseCompIf(std::unique_ptr<ListCompreExprAst> listCompre)
{
    return parseListOrCompIf(std::move(listCompre),
                             &PyParser::parseCompFor, &PyParser::parseCompIf);
}

/*
 * list_iter: list_for | list_if
 * list_if: 'if' old_test [list_iter]
 */
std::unique_ptr<ListCompreExprAst>
PyParser::parseListIf(std::unique_ptr<ListCompreExprAst> listCompre)
{
    return parseListOrCompIf(std::move(listCompre),
                             &PyParser::parseListFor, &PyParser::parseListIf);
}

std::unique_ptr<ListCompreExprAst>
PyParser::parseListOrCompIf(std::unique_ptr<ListCompreExprAst> listCompre,
                            ListCompre (PyParser::*genFunc) (ListCompre),
                            ListCompre (PyParser::*filterFunc) (ListCompre))
{
    UAISO_ASSERT(ahead_ == TK_IF, return listCompre);
    UAISO_ASSERT(listCompre, return listCompre);
    UAISO_ASSERT(listCompre->gens(), return listCompre);

    match(TK_IF);
    auto gen = listCompre->gens()->back();
    gen->addFilter(parseOldTest());

    return completeListCompre(std::move(listCompre), genFunc, filterFunc);
}

std::unique_ptr<ListCompreExprAst>
PyParser::completeListCompre(ListCompre listCompre,
                             ListCompre (PyParser::*genFunc) (ListCompre),
                             ListCompre (PyParser::*filterFunc) (ListCompre))
{
    if (ahead_ == TK_FOR)
        return ((this)->*(genFunc))(std::move(listCompre));

    if (ahead_ == TK_IF)
        return ((this)->*(filterFunc))(std::move(listCompre));

    return std::move(listCompre);
}

/*
 * or_test: and_test ('or' and_test)*
 */
std::unique_ptr<ExprAst> PyParser::parseOrTest()
{
    auto andTest = parseAndTest();
    while (maybeConsume(TK_OR)) {
        andTest = completeBinExpr<LogicOrExprAst>(std::move(andTest),
                                                     &PyParser::parseAndTest);
    }
    return andTest;
}

/*
 * and_test: not_test ('and' not_test)*
 */
std::unique_ptr<ExprAst> PyParser::parseAndTest()
{
    auto notTest = parseNotTest();
    while (maybeConsume(TK_AND)) {
        notTest = completeBinExpr<LogicAndExprAst>(std::move(notTest),
                                                      &PyParser::parseNotTest);
    }
    return notTest;
}

/*
 * not_test: 'not' not_test | comparison
 */
std::unique_ptr<ExprAst> PyParser::parseNotTest()
{
    if (maybeConsume(TK_NOT)) {
        auto notTest = LogicNotExprAst::create();
        notTest->setOprLoc(prevLoc_);
        notTest->setExpr(parseNotTest());
        return Expr(std::move(notTest));
    }

    return parseComparison();
}

/*
 * comparison: expr (comp_op expr)*
 * comp_op: '<'|'>'|'=='|'>='|'<='|'<>'|'!='|'in'|'not' 'in'|'is'|'is' 'not'
 */
std::unique_ptr<ExprAst> PyParser::parseComparison()
{
    auto expr = parseExpr();
    while (true) {
        switch (ahead_) {
        case TK_LS:
        case TK_GR:
        case TK_EQ_EQ:
        case TK_GR_EQ:
        case TK_LS_EQ:
        case TK_LS_GR:
        case TK_EXCLAM_EQ:
            consumeToken();
            expr = completeBinExpr<RelExprAst>(std::move(expr), &PyParser::parseExpr);
            break;

        case TK_IN:
            consumeToken();
            expr = completeBinExpr<InExprAst>(std::move(expr), &PyParser::parseExpr);
            break;

        case TK_IS: // May be followed by 'not'.
            consumeToken();
            maybeConsume(TK_NOT);
            expr = completeBinExpr<IsExprAst>(std::move(expr), &PyParser::parseExpr);
            break;

        case TK_NOT: // Must be followed by 'in'.
            consumeToken();
            match(TK_IN);
            expr = completeBinExpr<InExprAst>(std::move(expr), &PyParser::parseExpr);
            break;

        default:
            return expr;
        }
    }
}

/*
 * expr: xor_expr ('|' xor_expr)*
 * xor_expr: and_expr ('^' and_expr)*
 * and_expr: shift_expr ('&' shift_expr)*
 * shift_expr: arith_expr (('<<'|'>>') arith_expr)*
 * arith_expr: term (('+'|'-') term)*
 * term: factor (('*'|'/'|'%'|'//') factor)*
 */
std::unique_ptr<ExprAst> PyParser::parseExpr()
{
    return parseBinExpr(Precedence::Or);
}

std::unique_ptr<ExprAst> PyParser::parseBinExpr(Precedence curPrec)
{
    auto factor = parseFactor();
    while (true) {
        Precedence prec;
        std::unique_ptr<BinExprAst> bin;
        std::tie(prec, bin) = fetchPrecAhead();
        if (prec < curPrec)
            break;

        consumeToken();
        bin->setOprLoc(prevLoc_);
        bin->setExpr1(std::move(factor));
        bin->setExpr2(parseBinExpr(Precedence(curPrec + 1)));
        factor = std::move(bin);
    }

    return factor;
}

/*
 * factor: ('+'|'-'|'~') factor | power
 */
std::unique_ptr<ExprAst> PyParser::parseFactor()
{
    std::unique_ptr<UnaryExprAst> expr;
    switch (ahead_) {
    case TK_PLUS:
        consumeToken();
        return completeUnaryExpr<PlusExprAst>(&PyParser::parseFactor);

    case TK_MINUS:
        consumeToken();
        return completeUnaryExpr<MinusExprAst>(&PyParser::parseFactor);

    case TK_TILDE:
        consumeToken();
        return completeUnaryExpr<BitCompExprAst>(&PyParser::parseFactor);

    default:
        return parsePower();
    }
}

/*
 * power: atom trailer* ['**' factor]
 * trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME
 */
std::unique_ptr<ExprAst> PyParser::parsePower()
{
    auto atom = parseAtom();
    bool trailer = true;
    do {
        switch (ahead_) {
        case TK_LPAREN: {
            consumeToken();
            auto call = CallExprAst::create();
            call->setLDelimLoc(prevLoc_);
            call->setBase(std::move(atom));
            if (isArgAhead())
                call->setArgs(parseArgList());
            if (!match(TK_RPAREN)) {
                DEBUG_TRACE("parsePower, skip to TK_RPAREN\n");
                skipTo(TK_RPAREN);
            }
            call->setRDelimLoc(prevLoc_);
            atom = std::move(call);
            break;
        }

        case TK_LBRACKET: {
            consumeToken();
            auto arrayAccess = ArraySliceExprAst::create(); // See comment in this AST.
            arrayAccess->setLDelimLoc(prevLoc_);
            arrayAccess->setBase(std::move(atom));
            arrayAccess->setRange(parseSubscript());
            if (!match(TK_RBRACKET)) {
                DEBUG_TRACE("parsePower, skip to TK_RBRACKET\n");
                skipTo(TK_RBRACKET);
            }
            atom = std::move(arrayAccess);
            break;
        }

        case TK_DOT: {
            consumeToken();
            auto member = MemberAccessExprAst::create();
            member->setOprLoc(prevLoc_);
            member->setExpr(std::move(atom));
            member->setName(parseName());
            atom = std::move(member);
            break;
        }

        default:
            trailer = false;
            break;
        }
    } while (trailer);

    if (maybeConsume(TK_STAR_STAR))
        return completeBinExpr<PowerExprAst>(std::move(atom), &PyParser::parseFactor);

    return atom;
}

/*
 * atom: ('(' [yield_expr|testlist_comp] ')' |
 *        '[' [listmaker] ']' |
 *        '{' [dictorsetmaker] '}' |
 *        '`' testlist1 '`' |
 *        NAME | NUMBER | STRING+)
 *
 * Note: Accept 'None', 'True', and 'False, as well. The lexer process them
 * as keywords.
 */
std::unique_ptr<ExprAst> PyParser::parseAtom()
{
    switch (ahead_) {
    case TK_LPAREN:
        return parseWrappedOrTuple();

    case TK_LBRACE:
        return parseDictOrSetMaker();

    case TK_LBRACKET:
        return parseListMaker();

    case TK_BACKTICK: {
        consumeToken();
        auto str = TypeidExprAst::create();
        str->setLDelimLoc(prevLoc_);
        parseTestList1(); // Let it die.
        if (!match(TK_BACKTICK)) {
            DEBUG_TRACE("parseAtom, skip to TK_BACKTICK\n");
            skipTo(TK_BACKTICK);
        }
        str->setRDelimLoc(prevLoc_);
        return Expr(std::move(str));
    }

    case TK_COMPLETION:
    case TK_IDENT: {
        auto ident = IdentExprAst::create();
        ident->setName(parseName());
        return Expr(std::move(ident));
    }

    case TK_INT_LIT:
        consumeToken();
        return NumLitExprAst::create(prevLoc_, NumLitVariety::IntFormat);

    case TK_FLOAT_LIT:
        consumeToken();
        return NumLitExprAst::create(prevLoc_, NumLitVariety::FloatFormat);

    case TK_NULL_VALUE:
        consumeToken();
        return NullLitExprAst::create(prevLoc_);

    case TK_TRUE_VALUE:
    case TK_FALSE_VALUE:
        consumeToken();
        return BoolLitExprAst::create(prevLoc_);

    case TK_STR_LIT:
        return parseStrLit();

    default:
        fail();
        return Expr();
    }
}

/*
 * subscript: '.' '.' '.' | test | [test] ':' [test] [sliceop]
 * sliceop: ':' [test]
 */
std::unique_ptr<ExprAst> PyParser::parseSubscript()
{
    switch (ahead_) {
    case TK_DOT_DOT_DOT:
        // TODO: Model this kind of slice.
        consumeToken();
        return SubrangeExprAst::create();

    case TK_COLON:
        consumeToken();
        return completeSubrangeExpr(Expr());

    default:
        auto test = parseTest();
        if (maybeConsume(TK_COLON))
            return completeSubrangeExpr(std::move(test));
        return test;
    }
}

/*
 * subscriptlist: subscript (',' subscript)* [',']
 */
std::unique_ptr<ExprAstList> PyParser::parseSubscriptList()
{
    return parseList<ExprAstList>(TK_COMMA,
                                  &PyParser::isSubscriptAhead,
                                  &PyParser::parseSubscript).first;
}

/*
 * dictorsetmaker: ( (test ':' test (comp_for | (',' test ':' test)* [','])) |
 *                   (test (comp_for | (',' test)* [','])) )
 *
 * Note: This will actually parse '{' [dictorsetmaker] '}'
 */
std::unique_ptr<ExprAst> PyParser::parseDictOrSetMaker()
{
    UAISO_ASSERT(ahead_ == TK_LBRACE, return Expr());
    consumeToken();
    auto dictOrSet = ArrayInitExprAst::create();
    // DESIGN: Differentiate a set literal, '{1, 2}', from a list literal,
    // '(1, 2)'. Both are parsed as ArrayInitExprAst. Perhaps add a variety
    // to the AST or handle through Lang.
    dictOrSet->setLDelimLoc(prevLoc_);
    if (maybeConsume(TK_RBRACE)) {
        dictOrSet->setRDelimLoc(prevLoc_);
        return Expr(std::move(dictOrSet));
    }

    auto test = parseTest();
    switch (ahead_) {
    case TK_COLON: {
        consumeToken();
        auto desig = DesignateExprAst::create();
        desig->setDelimLoc(prevLoc_);
        desig->setId(std::move(test));
        desig->setValue(parseTest());

        if (ahead_ == TK_FOR) {
            auto listCompre = parseListFor(ListCompreExprAst::create());
            listCompre->setLDelimLoc(dictOrSet->lDelimLoc());
            listCompre->setExpr(std::move(desig));
            if (!match(TK_RBRACE)) {
                DEBUG_TRACE("parseDictOrSetMaker, skip to TK_RBRACE\n");
                skipTo(TK_RBRACE);
            }
            listCompre->setRDelimLoc(prevLoc_);
            return Expr(std::move(listCompre));
        }

        dictOrSet->addInit(std::move(desig));
        while (maybeConsume(TK_COMMA)) {
            if (!isTestAhead())
                break;
            if (dictOrSet->inits())
                dictOrSet->inits()->delim_ = prevLoc_;
            desig = DesignateExprAst::create();
            desig->setId(parseTest());
            match(TK_COLON);
            desig->setDelimLoc(prevLoc_);
            desig->setValue(parseTest());
            dictOrSet->addInit(std::move(desig));
        }
        if (!match(TK_RBRACE)) {
            DEBUG_TRACE("parseDictOrSetMaker, skip to TK_RBRACE\n");
            skipTo(TK_RBRACE);
        }
        dictOrSet->setRDelimLoc(prevLoc_);
        return Expr(std::move(dictOrSet));
    }

    case TK_FOR:  {
        auto listCompre = parseListFor(ListCompreExprAst::create());
        listCompre->setLDelimLoc(dictOrSet->lDelimLoc());
        listCompre->setExpr(std::move(test));
        if (!match(TK_RBRACE)) {
            DEBUG_TRACE("parseDictOrSetMaker, skip to TK_RBRACE\n");
            skipTo(TK_RBRACE);
        }
        listCompre->setRDelimLoc(prevLoc_);
        return Expr(std::move(listCompre));
    }

    case TK_COMMA:
        consumeToken();
        if (dictOrSet->inits())
            dictOrSet->inits()->delim_ = prevLoc_;
        dictOrSet->addInit(std::move(test));
        dictOrSet->mergeInits(parseTestList());
        // Fallthrough

    default:
        if (test)
            dictOrSet->addInit(std::move(test));
        if (!match(TK_RBRACE)) {
            DEBUG_TRACE("parseDictOrSetMaker, skip to TK_RBRACE\n");
            skipTo(TK_RBRACE);
        }
        dictOrSet->setRDelimLoc(prevLoc_);
        return Expr(std::move(dictOrSet));
    }
}

/*
 * listmaker: test ( list_for | (',' test)* [','] )
 *
 * Note: This will actually parse '[' [listmaker] ']'
 */
std::unique_ptr<ExprAst> PyParser::parseListMaker()
{
    UAISO_ASSERT(ahead_ == TK_LBRACKET, return Expr());
    consumeToken();
    auto list = ArrayInitExprAst::create();
    list->setLDelimLoc(prevLoc_);
    if (maybeConsume(TK_RBRACKET)) {
        list->setRDelimLoc(prevLoc_);
        return Expr(std::move(list));
    }

    auto test = parseTest();
    switch (ahead_) {
    case TK_FOR: {
        auto listCompre = parseListFor(ListCompreExprAst::create());
        listCompre->setLDelimLoc(list->lDelimLoc());
        listCompre->setExpr(std::move(test));
        if (!match(TK_RBRACKET)) {
            DEBUG_TRACE("parseListMaker, skip to TK_RBRACKET\n");
            skipTo(TK_RBRACKET);
        }
        listCompre->setRDelimLoc(prevLoc_);
        return Expr(std::move(listCompre));
    }

    case TK_COMMA:
        consumeToken();
        if (list->inits())
            list->inits()->delim_ = prevLoc_;
        list->addInit(std::move(test));
        if (isTestAhead())
            list->mergeInits(parseTestList());
        // Fallthrough

    default:
        if (test)
            list->addInit(std::move(test));
        if (!match(TK_RBRACKET)) {
            DEBUG_TRACE("parseListMaker, skip to TK_RBRACKET\n");
            skipTo(TK_RBRACKET);
        }
        list->setRDelimLoc(prevLoc_);
        return Expr(std::move(list));
    }
}

/*
 * wrappedortuple: '(' [yield_expr|testlist_comp] ')'
 * testlist_comp: test ( comp_for | (',' test)* [','] )
 *
 * '()'     - Tuple
 * '(1)'    - Wrapped expr
 * '(1,)'   - Tuple
 * '(1, 2)' - Tuple
 */
std::unique_ptr<ExprAst> PyParser::parseWrappedOrTuple()
{
    UAISO_ASSERT(ahead_ == TK_LPAREN, return Expr());
    consumeToken();
    auto tuple = TupleLitExprAst::create();
    tuple->setLDelimLoc(prevLoc_);
    if (maybeConsume(TK_RPAREN)) {
        tuple->setRDelimLoc(prevLoc_);
        return Expr(std::move(tuple));
    }

    if (ahead_ == TK_YIELD)
        return completeWrapped([this] () { return parseYieldExpr(); });

    auto test = parseTest();
    switch (ahead_) {
    case TK_FOR:
        return completeWrapped([&test, this] () {
            auto listCompre = parseListFor(ListCompreExprAst::create());
            listCompre->setExpr(std::move(test));
            return listCompre;
        });

    case TK_COMMA:
        consumeToken();
        if (tuple->inits())
            tuple->inits()->delim_ = prevLoc_;
        tuple->addInit(std::move(test));
        if (isTestAhead())
            mergeList(tuple->inits_, parseTestList());
        if (!match(TK_RPAREN)) {
            DEBUG_TRACE("parseWrappedOrTuple, skip to TK_RPAREN\n");
            skipTo(TK_RPAREN);
        }
        tuple->setRDelimLoc(prevLoc_);
        return Expr(std::move(tuple));

    default:
        return completeWrapped([&test, this]() { return std::move(test); });
    }
}

/*
 * yield_expr: 'yield' [testlist]
 */
std::unique_ptr<ExprAst> PyParser::parseYieldExpr()
{
    UAISO_ASSERT(ahead_ == TK_YIELD, return Expr());
    consumeToken();
    auto yield = YieldExprAst::create();
    yield->setKeyLoc(prevLoc_);
    if (isTestAhead())
        yield->setExprs(parseTestList());
    return Expr(std::move(yield));
}

/*
 * lambdef: 'lambda' [varargslist] ':' test
 */
std::unique_ptr<ExprAst> PyParser::parseLambdaDef()
{
    return parseLambdaCore(&PyParser::parseTest);
}

/*
 * old_lambdef: 'lambda' [varargslist] ':' old_test
 */
std::unique_ptr<ExprAst> PyParser::parseOldLambdaDef()
{
    return parseLambdaCore(&PyParser::parseOldTest);
}

std::unique_ptr<ExprAst> PyParser::parseLambdaCore(Expr (PyParser::*parseFunc) ())
{
    UAISO_ASSERT(ahead_ == TK_LAMBDA, return Expr());
    consumeToken();
    auto spec = std::unique_ptr<FuncSpecAst__<>>(newAst<FuncSpecAst__<>>());
    spec->setKeyLoc(prevLoc_);
    spec->setParam(parseVarArgsList(false));

    match(TK_COLON);
    spec->setLDelimLoc(prevLoc_);
    auto lambda = FuncLitExprAst::create();
    lambda->setSpec(std::move(spec));
    lambda->setStmt(newAst<ExprStmtAst>()->addExpr(((this)->*(parseFunc))()));

    return Expr(std::move(lambda));
}

/*
 * dotted_name: NAME ('.' NAME)*
 * dotted_as_name: dotted_name ['as' NAME]
 * dotted_as_names: dotted_as_name (',' dotted_as_name)*
 */
std::unique_ptr<NameAst> PyParser::parseDottedName()
{
    auto name = NestedNameAst::create();
    name->setNames(parseList<NameAstList>(TK_DOT,
                                          &PyParser::isNameAhead,
                                          &PyParser::parseName,
                                          false).first);
    return Name(std::move(name));
}

std::unique_ptr<NameAst> PyParser::parseName()
{
    if (maybeConsume(TK_COMPLETION)) {
        auto name = CompletionNameAst::create();
        name->setNameLoc(prevLoc_);
        return Name(std::move(name));
    }

    // If not a completion, we must match an identifier. However, if the
    // match doesn't succeed in this case we can't ignore and create a
    // simple name AST. Otherwise we end up with name without a
    // corresponding identifier in the lexeme map. An error name is then
    // created for consistency throughout the pipeline.
    if (match(TK_IDENT)) {
        auto name = SimpleNameAst::create();
        name->setNameLoc(prevLoc_);
        return Name(std::move(name));
    }

    return Name(newAst<ErrorNameAst>()->setNameLoc(prevLoc_));
}

std::unique_ptr<ExprAst> PyParser::parseStrLit()
{
    UAISO_ASSERT(ahead_ == TK_STR_LIT, return Expr());
    consumeToken();
    auto str = StrLitExprAst::create();
    str->setLitLoc(prevLoc_);
    if (ahead_ == TK_STR_LIT) {
        auto concat = ConcatExprAst::create();
        concat->setExpr1(std::move(str));
        concat->setExpr2(parseStrLit());
        return Expr(std::move(concat));
    }

    return Expr(std::move(str));
}

std::unique_ptr<ExprAst> PyParser::completeSubrangeExpr(Expr expr)
{
    auto range = SubrangeExprAst::create();
    range->setDelim1Loc(prevLoc_);
    range->setLow(std::move(expr));
    if (isTestAhead())
        range->setHi(parseTest());
    if (maybeConsume(TK_COLON))
        range->setDelim2Loc(prevLoc_);
    if (isTestAhead())
        range->setMax(parseTest()); // TODO: In Go, this is max.
    return Expr(std::move(range));
}

std::unique_ptr<ExprAst>
PyParser::completeWrapped(const std::function<Expr ()> exprFunc)
{
    auto wrap = WrappedExprAst::create();
    wrap->setLDelimLoc(prevLoc_);
    wrap->setExpr(exprFunc());
    if (!match(TK_RPAREN)) {
        DEBUG_TRACE("completeWrapped, skip to TK_PAREN\n");
        skipTo(TK_RPAREN);
    }
    wrap->setRDelimLoc(prevLoc_);
    return Expr(std::move(wrap));
}

template <class UnaryAstT> std::unique_ptr<ExprAst>
PyParser::completeUnaryExpr(Expr (PyParser::*parseFunc) ())
{
    auto unary = UnaryAstT::create();
    unary->setOprLoc(prevLoc_);
    unary->setExpr((((this)->*(parseFunc))()));
    return Expr(std::move(unary));
}

template <class BinaryAstT> std::unique_ptr<ExprAst>
PyParser::completeBinExpr(Expr expr, Expr (PyParser::*parseFunc) ())
{
    auto bin = BinaryAstT::create();
    bin->setOprLoc(prevLoc_);
    bin->setExpr1(std::move(expr));
    bin->setExpr2((((this)->*(parseFunc))()));
    return Expr(std::move(bin));
}

std::unique_ptr<ExprAst>
PyParser::completeAssignExpr(Expr expr, Expr (PyParser::*parseFunc) ())
{
    auto assign = AssignExprAst::create();
    assign->setOprLoc(prevLoc_);
    assign->setExpr1s(ExprAstList::create(std::move(expr)));
    assign->setExpr2s(ExprAstList::create((((this)->*(parseFunc))())));
    return Expr(std::move(assign));
}

std::unique_ptr<DeclAst> PyParser::completeParam(ParamGroup group)
{
    auto param = ParamDeclAst__<ParamVariadic__>::create();
    param->setVariadicLoc(prevLoc_);
    param->setName(parseName());
    group->addDecl(std::move(param));
    return std::move(group);
}

template <class AstListT> std::pair<std::unique_ptr<AstListT>, bool>
PyParser::parseList(Token tk,
                    bool (PyParser::*checkAhead) () const,
                    std::unique_ptr<typename AstListT::AstType> (PyParser::*parseFunc) (),
                    bool trailingOK)
{
    using List = std::unique_ptr<AstListT>;

    List list;
    auto item = ((this)->*(parseFunc))();
    addToList(list, std::move(item));
    while (maybeConsume(tk)) {
        if (list)
            list->delim_ = prevLoc_;
        if (trailingOK && !(((this)->*(checkAhead))()))
            return std::make_pair(std::move(list), true);

        addToList(list, (((this)->*(parseFunc))()));
    }

    return std::make_pair(std::move(list), false);
}
