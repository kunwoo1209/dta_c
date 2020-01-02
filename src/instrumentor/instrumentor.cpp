#include <string>
#include <set>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <ostream>
#include <vector>
#include <algorithm>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceManager.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace std;

static llvm::cl::OptionCategory MyOptionCategory("Options");
static llvm::cl::opt<std::string> OutputFilename("o",
        llvm::cl::desc("Specify output filename"),
        llvm::cl::value_desc("filename"), llvm::cl::cat(MyOptionCategory));

bool global_variable = false;

LangOptions MyLangOpts;
SourceManager *ptrMySourceMgr;
Rewriter MyRewriter;

vector<std::string> registerVariable;
vector<SourceLocation> defStartLocList;
vector<SourceLocation> condStartLocVec;
vector<SourceLocation> condEndLocVec;
vector<SourceLocation> assignRhsStartLocVec;
vector<SourceLocation> assignRhsEndLocVec;
vector<std::string> assignLhsNameVec;
vector<SourceLocation> argumentStartLocVec;
vector<SourceLocation> argumentEndLocVec;
std::string functionName;
bool first_function_body = true;


int GetLineNumber(SourceManager &src_mgr, SourceLocation loc)
{
	return static_cast<int>(src_mgr.getPresumedLineNumber(loc));
}


vector<std::string> lhsOfAssignmentOperator(SourceLocation StartLoc, SourceLocation EndLoc) {
	vector<std::string> listOfLhs = vector<std::string>();

	BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr);
	for (unsigned i = 0; i < assignRhsStartLocVec.size(); ++i)
		if ((isBefore(assignRhsStartLocVec[i], StartLoc) || assignRhsStartLocVec[i] == StartLoc)
				&& (isBefore(EndLoc, assignRhsEndLocVec[i]) || EndLoc == assignRhsEndLocVec[i]))
			listOfLhs.push_back(assignLhsNameVec[i]);
	return listOfLhs;
}

bool isFunctionArgument(SourceLocation StartLoc, SourceLocation EndLoc) {
	BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr);
	for (unsigned i = 0; i < argumentStartLocVec.size(); ++i)
		if ((isBefore(argumentStartLocVec[i], StartLoc) || argumentStartLocVec[i] == StartLoc)
				&& (isBefore(EndLoc, argumentEndLocVec[i]) || EndLoc == argumentEndLocVec[i]))
			return true;
	return false;

}

std::string Expr2String(const Expr *e) {
	PrintingPolicy Policy(MyLangOpts);
	std::string str1;
	llvm::raw_string_ostream os(str1);
	e->printPretty(os, NULL, Policy);

	return str1;
}

std::string opOfCompoundAssignOperator(CompoundAssignOperator * cao) {
	std::string strCompoundAssignOperator = Expr2String(cao);
	std::string opStr = "";
	size_t idx = strCompoundAssignOperator.find("=");
	while (strchr("+-*/%&|^<>", strCompoundAssignOperator[idx-1])) {
		opStr = strCompoundAssignOperator[idx-1] + opStr;
		idx -= 1;
	}
	return opStr;
}

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
	MyASTVisitor()   {}
	
	bool addressable(Expr * expr, std::string name) {
		if (isa<MemberExpr>(expr)) {
			MemberExpr *me = cast<MemberExpr>(expr);
			if (!isBitField(me) && notRegisterVariable(name))
				return true;
		} else if (notRegisterVariable(name))
			return true;
		return false;
	}
	bool VisitStmt(Stmt *s) {

		BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
		SourceLocation condStartLoc, condEndLoc;

		/*if (isa<IfStmt>(s)) {

			const IfStmt * ifStmt = cast<IfStmt>(s);
			const Expr * condExpr = ifStmt->getCond();
			SourceRange exprRange = condExpr->getSourceRange();
			int rangeSize = MyRewriter.getRangeSize(exprRange);

			condStartLoc = condExpr->getLocStart();
			condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize);

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} else if (isa<ForStmt>(s)) {

			const ForStmt * forStmt = cast<ForStmt>(s);
			const Expr * condExpr = forStmt->getCond();

			if (condExpr == NULL) {

				const Stmt * forBody = forStmt->getBody();

				condStartLoc = forBody->getLocStart();
				condEndLoc = forBody->getLocStart();
				
			} else {

				SourceRange exprRange = condExpr->getSourceRange();
				int rangeSize = MyRewriter.getRangeSize(exprRange);
	
				condStartLoc = condExpr->getLocStart();
				condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize);

			}

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} else if (isa<WhileStmt>(s)) {

			const WhileStmt * whileStmt = cast<WhileStmt>(s);
			const Expr * condExpr = whileStmt->getCond();
			SourceRange exprRange = condExpr->getSourceRange();
			int rangeSize = MyRewriter.getRangeSize(exprRange);

			condStartLoc = condExpr->getLocStart();
			condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize); 

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} else if (isa<DoStmt>(s)) {

			const DoStmt * doStmt = cast<DoStmt>(s);
			const Expr * condExpr = doStmt->getCond();
			SourceRange exprRange = condExpr->getSourceRange();
			int rangeSize = MyRewriter.getRangeSize(exprRange);

			condStartLoc = condExpr->getLocStart();
			condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize); 

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} else if (isa<ConditionalOperator>(s)) {

			const ConditionalOperator * condOp = cast<ConditionalOperator>(s);
			const Expr * condExpr = condOp->getCond();
			SourceRange exprRange = condExpr->getSourceRange();
			int rangeSize = MyRewriter.getRangeSize(exprRange);

			condStartLoc = condExpr->getLocStart();
			condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize);

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} else if (isa<SwitchStmt>(s)) {

			SwitchStmt *switchStmt = cast<SwitchStmt>(s);
			const Expr *condExpr = switchStmt->getCond();
			SourceRange exprRange = condExpr->getSourceRange();
			int rangeSize = MyRewriter.getRangeSize(exprRange);

			condStartLoc = condExpr->getLocStart();
			condEndLoc = condExpr->getLocStart().getLocWithOffset(rangeSize);

			while (true) {
				if (!condEndLocVec.empty() && isBefore(condEndLocVec.front(), condStartLoc)) {
					condStartLocVec.erase(condStartLocVec.begin());
					condEndLocVec.erase(condEndLocVec.begin());
				} else 
					break;
			}
			condStartLocVec.push_back(condStartLoc);
			condEndLocVec.push_back(condEndLoc);

		} 
*/
		return true;
	}
	
	std::string ExprWOSideEffect(Expr * e) {

		e = e->IgnoreParenCasts();

		// SubExpr is Array
		if (isa<ArraySubscriptExpr>(e)) {
			ArraySubscriptExpr *asep = cast<ArraySubscriptExpr>(e);

			Expr *base = asep->getBase()->IgnoreParenCasts();
			Expr *idx = asep->getIdx()->IgnoreParenCasts();

			return "(" + ExprWOSideEffect(base) + ")[" + ExprWOSideEffect(idx) + "]";
		}

		// SubExpr is Structure Member
		else if (isa<MemberExpr>(e)) {
			MemberExpr *me = cast<MemberExpr>(e);
			Expr * base = me->getBase()->IgnoreParenCasts();
			std::string baseName = ExprWOSideEffect(base);
			if (!isBitField(me)) {
				std::string MemberExprName = Expr2String(me);
				std::string UseName = "";
				unsigned int i = 0, j = 0;
				int numParentheses = 0;
				while (i < baseName.length() && j < MemberExprName.length()) {
					if (baseName[i] == MemberExprName[j] && MemberExprName[j] != '(' && MemberExprName[j] != ')') {
						UseName += baseName[i];
						i++, j++;
					} else if (MemberExprName[j] == '(' || MemberExprName[j] == ')') {
						if (MemberExprName[j] == '(') numParentheses += 1;
						else numParentheses -= 1;
						UseName += MemberExprName[j];
						j++;
					} else if (baseName[i] == '(' || baseName[i] == ')') {
						UseName += baseName[i];
						i++;
					} else
						j++;
				}
				while (i < baseName.length()) {
					UseName += baseName[i];
					i++;
				}
				if (numParentheses > 0) {
					j = MemberExprName.length()-1;
					while (numParentheses != 0) {
						if (MemberExprName[j] == ')') {
							numParentheses -= 1;
							if (numParentheses == 0) break;
						}
						j--;
					}
				}
				while (j < MemberExprName.length()) {
					UseName += MemberExprName[j];
					j++;
				}
				i = 0;
				int continuous_open_bracket = 0;
				int continuous_close_bracket = 0;
				while (i < UseName.length()) {
					if (UseName[i] == '(' && UseName[i+1] == '(') {
						continuous_open_bracket++;
						i += 2;
					} else if (UseName[i] == ')' && UseName[i+1] == ')') {
						continuous_close_bracket++;
						i += 2;
					} else
						i++;
				}
				if (continuous_open_bracket == continuous_close_bracket && continuous_open_bracket > 0) {
					std::string tempUseName = "";
					i = 0;
					while (i < UseName.length()) {
						if (UseName[i] == '(' && UseName[i+1] == '(') {
							tempUseName += '(';
							i += 2;
						} else if (UseName[i] == ')' && UseName[i+1] == ')') {
							tempUseName += ')';
							i += 2;
						} else {
							tempUseName += UseName[i];
							i++;
						}
					}
					UseName = tempUseName;
				}
				return UseName;
			}
		}

		// SubExpr is DeclRefExpr
		else if (isa<DeclRefExpr>(e)) {
			DeclRefExpr *dre = cast<DeclRefExpr>(e);
			Expr * base = dre->IgnoreParenCasts();
			std::string UseName = Expr2String(base);
			return UseName;
		}
		
		// SubExpr is UnaryOperator with asterisk *
		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e))->getOpcode() == UO_Deref) {
			UnaryOperator *uo = cast<UnaryOperator>(e);
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			return "*" + ExprWOSideEffect(se);
		}
		
		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e)->isIncrementDecrementOp())) {
			UnaryOperator *uo = cast<UnaryOperator>(e);
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			if (uo->isPrefix() && uo->isIncrementOp())
				return "(" + ExprWOSideEffect(se) + "+1)";
			else if (uo->isPrefix() && uo->isDecrementOp())
				return "(" + ExprWOSideEffect(se) + "-1)";
			else
				return ExprWOSideEffect(se);
		}
		
		else if (isa<BinaryOperator>(e)) {
			BinaryOperator *bo = cast<BinaryOperator>(e);
			if (bo->isAssignmentOp() && isa<CompoundAssignOperator>(bo)) {
				CompoundAssignOperator *cao = cast<CompoundAssignOperator>(bo);
				return "(" + ExprWOSideEffect(cao->getLHS()) 
					+ opOfCompoundAssignOperator(cao) + ExprWOSideEffect(cao->getRHS()) + ")";
			}
			else if	(bo->isAssignmentOp()) {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				return ExprWOSideEffect(bo_lhs);
			}
			else {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				Expr *bo_rhs = bo->getRHS()->IgnoreParenCasts();
				std::string op = bo->getOpcodeStr().str();
				return "(" + ExprWOSideEffect(bo_lhs) + op + ExprWOSideEffect(bo_rhs) + ")";
			}
		}
		return Expr2String(e);
	}
	
	std::string EscapeSeqHandled(std::string Name) {
		std::string newName = "";
		for (unsigned i = 0; i < Name.length(); i++) {
			if (Name[i] == '\n')
				newName += "\\n";
			else if (Name[i] == '\t')
				newName += "\\t";
			else if (Name[i] == '\"')
				newName += "\\\"";
			else if (Name[i] == '\\')
				newName += "\\\\";
			else
				newName += string(1, Name.at(i));
		}
		return newName;
	}

	bool insertFilePointer(SourceLocation StartLoc) {
		std::string probeFront = "FILE * ___FP;\n";
		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		return true;
	}

	bool initializeFilePointer(SourceLocation StartLoc) {
		std::string probeFront = "___FP = fopen(\"TRACE\", \"w\");\n";
		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		return true;
	}

	bool insertCallExprProbe(CallExpr *ce, std::string CalleeFuncName, SourceLocation StartLoc, SourceLocation EndLoc) {
		std::string probeFront = "((fprintf(___FP,\"CALL: %s\\t%s\\t%d\\n\",\""+functionName+"\",\"" + CalleeFuncName+"\"," + std::to_string(ce->getNumArgs()) + ")";
		
		BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr);
		bool insideCondition = false;
		insideCondition = true;
		for (unsigned i = 0; i < condStartLocVec.size(); i++) {
			if ((isBefore(condStartLocVec.back(), StartLoc) || condStartLocVec.back() == StartLoc)
				&& (isBefore(EndLoc, condEndLocVec.back()) || EndLoc == condEndLocVec.back())) {
				insideCondition = true;
				break;
			}
		}
		if (insideCondition) {
			probeFront += ",fprintf(___FP,\"RETURN VALUE: %s\\t%s\\t%lu";
			probeFront += ("\\n\",\""+CalleeFuncName+"\",\"" + functionName +"\",sizeof("+ce->getType().getAsString()+"))");
		} 
		probeFront += ")?";
		std::string probeBack = ": 0)";

		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		MyRewriter.InsertTextBefore(EndLoc, probeBack);

		return true;
	}

	bool insertProbeCallExprInAssignment(CallExpr * ce, std::string lhsName, std::string CalleeFuncName, SourceLocation StartLoc, SourceLocation EndLoc) {
		if (notRegisterVariable(lhsName)) {
			std::string probeFront, probeBack;
			probeFront = "(fprintf(___FP,\"ASSIGN: %s\\t%p\\t%lu\\t%s\\t%lu\\t%s\\t%s\\t%d\\n\",\"";
			probeFront += (EscapeSeqHandled(lhsName)+"\",");
			if (lhsName.at(0) != '&')
				probeFront += "&";
			probeFront += (lhsName+",(unsigned long)sizeof("+lhsName+"),\"");
			probeFront += (CalleeFuncName+"\",sizeof("+ce->getType().getAsString()+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,StartLoc))+"),");
			probeBack= ")";

			MyRewriter.InsertTextAfter(StartLoc, probeFront);
			MyRewriter.InsertTextBefore(EndLoc, probeBack);
		}

		return true;
	}

	bool insertAssignmentProbe(std::string lhsName, std::string rhsName, SourceLocation StartLoc, SourceLocation EndLoc) {
		if (notRegisterVariable(lhsName)) {
			std::string probeFront, probeBack;
			probeFront = "(fprintf(___FP,\"ASSIGN: %s\\t%p\\t%lu\\t%s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
			probeFront += (EscapeSeqHandled(lhsName)+"\",");
			if (lhsName.at(0) != '&')
				probeFront += "&";
			probeFront += (lhsName+",(unsigned long)sizeof("+lhsName+"),\""+EscapeSeqHandled(rhsName)+"\",");
			if (rhsName.at(0) != '&') 
				probeFront += "&";
			probeFront += (rhsName + ",(unsigned long)sizeof(");
			probeFront += (rhsName+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,StartLoc))+",");
			probeFront += "(unsigned long)0),";
			probeBack= ")";

			MyRewriter.InsertTextAfter(StartLoc, probeFront);
			MyRewriter.InsertTextBefore(EndLoc, probeBack);
		}
		return true;
	}

	// TODO
	// DEF/USE: variable name, address, size, file, function name, line, top level
	bool insertProbeFront(std::string Name, SourceLocation StartLoc, SourceLocation EndLoc,
			bool use_flag, int numAsterisks, int existingAsterisks, bool top_level) {
		BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr);
		bool insideCondition = false;
		insideCondition = true;
		for (unsigned i = 0; i < condStartLocVec.size(); i++) {
			if ((isBefore(condStartLocVec.back(), StartLoc) || condStartLocVec.back() == StartLoc)
				&& (isBefore(EndLoc, condEndLocVec.back()) || EndLoc == condEndLocVec.back())) {
				insideCondition = true;
				break;
			}
		}
		std::string probeFront, probeBack;
		if (use_flag) {
			if (insideCondition && !isFunctionArgument(StartLoc, EndLoc)) {
				probeFront = "(fprintf(___FP,\"USE: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
			} else {
				return false;
				//probeFront = "(fprintf(___FP,\"USE: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\tNOT\\n\",\"";
			}
		}
		else {
			probeFront = "(fprintf(___FP,\"DEF: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
		}

		for (int i = 0; i < existingAsterisks; i++)
			probeFront += "*";
		probeFront += (EscapeSeqHandled(Name)+"\",");
		if (Name.at(0) != '&')
			probeFront += "&";
		for (int i = 0; i < existingAsterisks; i++)
			probeFront += "*";
		probeFront += (Name + ",(unsigned long)sizeof(");
		for (int i = 0; i < existingAsterisks; i++) 
			probeFront += "*";
		probeFront += (Name+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,StartLoc))+",");
		if (top_level)
			probeFront += "(unsigned long)sizeof(int*)),";
		else
			probeFront += "(unsigned long)0),";
		probeBack= ")";

		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		MyRewriter.InsertTextBefore(EndLoc, probeBack);

		return true;
	}

	bool insertProbeArgument(std::string Name, SourceLocation StartLoc, SourceLocation EndLoc) {
		std::string probeFront, probeBack;
		probeFront = "(fprintf(___FP,\"ARGUMENT: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
		probeFront += (EscapeSeqHandled(Name)+"\",&");
		probeFront += (Name + ",(unsigned long)sizeof(");
		probeFront += (Name+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,StartLoc))+",");
		probeFront += "(unsigned long)0),";
		probeBack= ")";

		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		MyRewriter.InsertTextBefore(EndLoc, probeBack);

		return true;
	}

	bool insertInitializationProbe(std::string Name, SourceLocation EndLoc, int numAsterisks) {
		std::string probeBack= "fprintf(___FP,\"DEF: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t0\\n\",\""+EscapeSeqHandled(Name)+"\",&";
		probeBack += (Name+",(unsigned long)sizeof(");
		probeBack += (Name+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,EndLoc))+");");
		MyRewriter.InsertTextBefore(EndLoc, probeBack);

		return true;
	}

	bool insertProbeBack(std::string Name, SourceLocation StartLoc, SourceLocation EndLoc,
			bool use_flag, int numAsterisks, int existingAsterisks, bool top_level) {
		BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr);
		bool insideCondition = false;
		for (unsigned i = 0; i < condStartLocVec.size(); i++) {
			if ((isBefore(condStartLocVec.back(), StartLoc) || condStartLocVec.back() == StartLoc)
				&& (isBefore(EndLoc, condEndLocVec.back()) || EndLoc == condEndLocVec.back())) {
				insideCondition = true;
				break;
			}
		}
		std::string probeFront = "(";
		std::string probeBack;
		if (use_flag) {
			if (insideCondition && !isFunctionArgument(StartLoc, EndLoc)) {
				probeBack = ",fprintf(___FP,\"USE: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
			} else {
				return false;
				//probeBack = ",fprintf(___FP,\"USE: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\tNOT\\n\",\"";
			}
		}
		else
			probeBack = ",fprintf(___FP,\"DEF: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t%lu\\n\",\"";
		for (int i = 0; i < existingAsterisks; i++)
			probeBack += "*";
		probeBack += (EscapeSeqHandled(Name) + "\",");
		if (Name.at(0) != '&')
			probeFront += "&";
		for (int i = 0; i < existingAsterisks; i++)
			probeBack += "*";
		probeBack += (Name + ",(unsigned long)sizeof(");
		for (int i = 0; i < existingAsterisks; i++)
			probeBack += "*";
		probeBack += (Name + "),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,StartLoc))+",");
		if (top_level)
			probeBack += "(unsigned long)sizeof(int*)),";
		else
			probeBack += "(unsigned long)0),";
		for (int i = 0; i < existingAsterisks; i++)
			probeBack += "*";
		probeBack += (Name + ")");

		MyRewriter.InsertTextAfter(StartLoc, probeFront);
		MyRewriter.InsertTextBefore(EndLoc, probeBack);
		
		return true;
	}

	bool insertProbeLocalVar(std::string Name, SourceLocation EndLoc, int numAsterisks, int param_id) {
		std::string probeBack= "fprintf(___FP,\"LOCAL: %s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t0\\n\",\""+EscapeSeqHandled(Name)+"\",&";
		probeBack += (Name+",(unsigned long)sizeof(");
		probeBack += (Name+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,EndLoc))+");");
		if (param_id != 0) {
			probeBack += "fprintf(___FP,\"PARAM: %d\\t%s\\t%p\\t%lu\\t%s\\t%s\\t%d\\t0\\n\","+std::to_string(param_id)+",\""+EscapeSeqHandled(Name)+"\",&";
			probeBack += (Name+",(unsigned long)sizeof(");
			probeBack += (Name+"),__FILE__,\""+functionName+"\","+std::to_string(GetLineNumber(*ptrMySourceMgr,EndLoc))+");");
		}
		MyRewriter.InsertTextBefore(EndLoc, probeBack);

		return true;
	}

	bool insertProbeFuncArgIsCallExpr(
		std::string target_fname, 
		CallExpr * ce, 
		SourceLocation start_loc, 
		SourceLocation end_loc,
		int arg_id) 
	{
		if (ce->getDirectCallee() != NULL) { 
			std::string probe_front = "(fprintf(___FP, \"ARG: %d\\t%s\\t%s\\n\","+std::to_string(arg_id)+",\""+ce->getDirectCallee()->getNameInfo().getName().getAsString()+"\",\""+target_fname+"\"),";
			std::string probe_back = ")";

			MyRewriter.InsertTextAfter(start_loc, probe_front);
			MyRewriter.InsertTextBefore(end_loc, probe_back);
		} else {
			std::string probe_front = "(fprintf(___FP, \"ARG: %d\\t%s\\t%s\\n\","+std::to_string(arg_id)+",\"CALLBACK\",\""+target_fname+"\"),";
			std::string probe_back = ")";

			MyRewriter.InsertTextAfter(start_loc, probe_front);
			MyRewriter.InsertTextBefore(end_loc, probe_back);

		}
		return true;
	}

	bool insertProbeFuncArgIsLiteral(
		std::string target_fname, 
		std::string caller_name,
		std::string literal_name, 
		SourceLocation start_loc,
		SourceLocation end_loc,
		int arg_id) 
	{
		std::string probe_front = "(fprintf(___FP, \"ARG: %d\\t%s\\t%s\\n\","+std::to_string(arg_id)+",\""+caller_name+"\",\""+target_fname+"\"),";
		std::string probe_back = ")";

		MyRewriter.InsertTextAfter(start_loc, probe_front);
		MyRewriter.InsertTextBefore(end_loc, probe_back);
		return true;
	}

	bool insertProbeFuncArgIsVariable(
		std::string target_fname,
		std::string caller_name, 
		std::string var_name, 
		SourceLocation start_loc, 
		SourceLocation end_loc,
		int arg_id) 
	{
		std::string probe_front = "(fprintf(___FP, \"ARG: %s\\t%d\\t%s\\t%p\\t%s\\t%s\\t%d\\n\",\""+target_fname+"\","+std::to_string(arg_id)+",\""+ var_name + "\",";
		if (var_name[0] != '&') {
			probe_front += "&";
		}
		probe_front += (var_name + ",__FILE__,\"" + caller_name + "\","+std::to_string(GetLineNumber(*ptrMySourceMgr,start_loc))+"),");
		std::string probe_back = ")";

		MyRewriter.InsertTextAfter(start_loc, probe_front);
		MyRewriter.InsertTextBefore(end_loc, probe_back);
		return true;
	}

	bool ExprIsPointer(Expr *e) {
		return e->getType().getCanonicalType().getTypePtr()->isPointerType();
	}

	bool ExprIsArray(Expr *e) {
		return e->getType().getCanonicalType().getTypePtr()->isArrayType();
	}

	bool ExprIsStructure(Expr *e) {
		return e->getType().getCanonicalType().getTypePtr()->isStructureType();
	}

	bool notRegisterVariable(std::string varName){
		return std::find(registerVariable.begin(), registerVariable.end(), varName) == registerVariable.end();
	}

	bool isBitField(MemberExpr * me) {
		return me->refersToBitField();
	}

	bool ExprIsIncomplete(Expr * e) {
		return e->getType().getCanonicalType().getTypePtr()->isIncompleteType();
	}

	int numAsterisk(QualType x) {
		if (x.getTypePtr()->isIncompleteType()) return 0;
		int i = 1;
		if (x.getCanonicalType().getTypePtr()->isPointerType()) {
			i += numAsterisk(x.getCanonicalType().getTypePtr()->getPointeeType());
		}
		return i;
	}

	SourceLocation endLocExpr(Expr *e) {
		if (isa<BinaryOperator>(e)) {
			BinaryOperator *bo = cast<BinaryOperator>(e);
			if (bo->getRHS()->getLocStart().isMacroID()) {	//swtv-kaist/MUSIC/music_utility.cpp
				pair<SourceLocation, SourceLocation> expansionRange =
				MyRewriter.getSourceMgr().getImmediateExpansionRange(bo->getRHS()->getLocEnd());
				return clang::Lexer::getLocForEndOfToken(
						ptrMySourceMgr->getExpansionLoc(expansionRange.second), 0, *ptrMySourceMgr,
						MyLangOpts);
			} else {
				return bo->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(bo->getSourceRange()));
			}
		}
		else {
			if (e->getLocStart().isMacroID()) {	//swtv-kaist/MUSIC/music_utility.cpp
				pair<SourceLocation, SourceLocation> expansionRange =
				MyRewriter.getSourceMgr().getImmediateExpansionRange(e->getLocEnd());
				return clang::Lexer::getLocForEndOfToken(
						ptrMySourceMgr->getExpansionLoc(expansionRange.second), 0, *ptrMySourceMgr,
						MyLangOpts);
			} else {
				return e->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(e->getSourceRange()));
			}
		}
	}
	
	Expr * removeParentheses(Expr * e) {
		if (isa<ParenExpr>(e)) {
			ParenExpr *ParenExprPtr = cast<ParenExpr>(e);
			e = removeParentheses(ParenExprPtr->getSubExpr());
		}
		return e;
	}

	std::string insertUseProbeSubExpr(Expr * e, SourceLocation startLoc, SourceLocation endLoc, bool top_level) {

		// USE
		defStartLocList.push_back(e->getLocStart());
		if (isa<ImplicitCastExpr>(e) || isa<CStyleCastExpr>(e)) {
			ImplicitCastExpr *ice = cast<ImplicitCastExpr>(e);
			Expr *se = ice->getSubExpr()->IgnoreParenCasts();
			return insertUseProbeSubExpr(se, startLoc, endLoc, top_level);
		}

		// SubExpr is Array
		else if (isa<ArraySubscriptExpr>(e)) {
			ArraySubscriptExpr *asep = cast<ArraySubscriptExpr>(e);

			Expr * base = asep->getBase()->IgnoreParenCasts();
			Expr * idx = asep->getIdx()->IgnoreParenCasts();

			SourceLocation idxStartLoc = idx->getLocStart();
			SourceLocation idxEndLoc = endLocExpr(idx);
			insertUseProbeSubExpr(base, startLoc, endLoc, true);
			insertUseProbeSubExpr(idx, idxStartLoc, idxEndLoc, false);

			std::string array_name = ExprWOSideEffect(asep);

			insertProbeFront(array_name, startLoc, endLoc, true, 0, 0, top_level);
			return array_name;
		}

		// SubExpr is Structure Member
		else if (isa<MemberExpr>(e)) {
			MemberExpr *me = cast<MemberExpr>(e);

			Expr * base = me->getBase()->IgnoreParenCasts();

			std::string baseName = insertUseProbeSubExpr(base, startLoc, endLoc, true);
			if (!isBitField(me)) {
				std::string MemberExprName = Expr2String(me);
				std::string UseName = "";
				unsigned int i = 0, j = 0;
				int numParentheses = 0;
				while (i < baseName.length() && j < MemberExprName.length()) {
					if (baseName[i] == MemberExprName[j] && MemberExprName[j] != '(' && MemberExprName[j] != ')') {
						UseName += baseName[i];
						i++, j++;
					} else if (MemberExprName[j] == '(' || MemberExprName[j] == ')') {
						if (MemberExprName[j] == '(') numParentheses += 1;
						else numParentheses -= 1;
						UseName += MemberExprName[j];
						j++;
					} else if (baseName[i] == '(' || baseName[i] == ')') {
						UseName += baseName[i];
						i++;
					} else
						j++;
				}
				while (i < baseName.length()) {
					UseName += baseName[i];
					i++;
				}
				if (numParentheses > 0) {
					j = MemberExprName.length()-1;
					while (numParentheses != 0) {
						if (MemberExprName[j] == ')') {
							numParentheses -= 1;
							if (numParentheses == 0) break;
						}
						j--;
					}
				}
				while (j < MemberExprName.length()) {
					UseName += MemberExprName[j];
					j++;
				}
				i = 0;
				int continuous_open_bracket = 0;
				int continuous_close_bracket = 0;
				while (i < UseName.length()) {
					if (UseName[i] == '(' && UseName[i+1] == '(') {
						continuous_open_bracket++;
						i += 2;
					} else if (UseName[i] == ')' && UseName[i+1] == ')') {
						continuous_close_bracket++;
						i += 2;
					} else
						i++;
				}

				if (continuous_open_bracket == continuous_close_bracket && continuous_open_bracket > 0) {
					std::string tempUseName = "";
					i = 0;
					while (i < UseName.length()) {
						if (UseName[i] == '(' && UseName[i+1] == '(') {
							tempUseName += '(';
							i += 2;
						} else if (UseName[i] == ')' && UseName[i+1] == ')') {
							tempUseName += ')';
							i += 2;
						} else {
							tempUseName += UseName[i];
							i++;
						}
					}
					UseName = tempUseName;
				}

				i = 0;
				if (ExprIsPointer(me))
					i = numAsterisk(me->getType().getCanonicalType().getTypePtr()->getPointeeType());
				insertProbeFront(UseName, startLoc, endLoc, true, i, 0, top_level);
				return UseName;
			}
		}

		// SubExpr is DeclRefExpr
		else if (isa<DeclRefExpr>(e)) {
			DeclRefExpr *dre = cast<DeclRefExpr>(e);

			// USE
			// defStartLocList.push_back(dre->getLocStart());

			Expr * base = dre->IgnoreParenCasts();
			std::string UseName = Expr2String(base);

			if (ExprIsIncomplete(base)) {
				return Expr2String(e);
			} else if (notRegisterVariable(UseName) && ExprIsPointer(base)) {
				int i = numAsterisk(base->getType().getCanonicalType().getTypePtr()->getPointeeType());
				insertProbeFront(UseName, startLoc, endLoc, true, i, 0, top_level);
			} else if (notRegisterVariable(UseName) && (ExprIsArray(base) || ExprIsStructure(base))) {
				// By adding this condition, no difference between static allocation and dynamic allocation of memory
				// sizeof(arr[]) = 12 vs sizeof(arr) = 8
			} else if (notRegisterVariable(UseName)) {
				insertProbeFront(UseName, startLoc, endLoc, true, 0, 0, top_level);
			}
			return UseName;
		}
		
		// SubExpr is UnaryOperator with asterisk *
		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e))->getOpcode() == UO_Deref) {
			UnaryOperator *uo = cast<UnaryOperator>(e);
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
//			return "*" + insertUseProbeSubExpr(se, startLoc, endLoc, true);
			// 02-27
			std::string UseName = "*" + ExprWOSideEffect(se);
			insertProbeFront(UseName, startLoc, endLoc, true, 0, 0, top_level);
			return UseName;
		}
		
		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e)->isIncrementDecrementOp())) {
			UnaryOperator *uo = cast<UnaryOperator>(e);
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			return ExprWOSideEffect(se);
		}

		else if (isa<BinaryOperator>(e)) {
			BinaryOperator *bo = cast<BinaryOperator>(e);
			if (bo->isAssignmentOp()) {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				return ExprWOSideEffect(bo_lhs);
			}
			else {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				Expr *bo_rhs = bo->getRHS()->IgnoreParenCasts();
				// defStartLocList.push_back(bo_rhs->getLocStart());
				std::string op = bo->getOpcodeStr().str();
				return "(" + insertUseProbeSubExpr(bo_lhs, startLoc, endLoc, top_level) + op + insertUseProbeSubExpr(bo_rhs, startLoc, endLoc, top_level) + ")";
			}
		}
		return Expr2String(e);
	}

	std::string insertProbeBinaryOperatorDef(BinaryOperator *bo, Expr * bo_lhs, int existing_asterisk) {

		SourceLocation boStartLoc = bo->getLocStart();
		SourceLocation boEndLoc = endLocExpr(bo);
		
		// USE
		if (existing_asterisk == 0) {
			defStartLocList.push_back(boStartLoc);
			defStartLocList.push_back(bo_lhs->getLocStart());
		}
		// Primitive Variable e.g., x = 1
		if (isa<DeclRefExpr>(bo_lhs)) {
			DeclRefExpr *DeclRefExprPtr = cast<DeclRefExpr>(bo_lhs);
			std::string DefName = DeclRefExprPtr->getNameInfo().getName().getAsString();
			if (ExprIsIncomplete(bo_lhs)) {
				return DefName;
			}
			else if (bo_lhs->getType().getCanonicalType().getTypePtr()->isNullPtrType()) {
				return DefName;
			}
			else if (notRegisterVariable(DefName) && ExprIsPointer(DeclRefExprPtr)) {
				int i = numAsterisk(DeclRefExprPtr->getType().getCanonicalType().getTypePtr()->getPointeeType());
				insertProbeFront(DefName, boStartLoc, boEndLoc, false, i, existing_asterisk, false);
			}
			else if (notRegisterVariable(DefName)) {
				insertProbeFront(DefName, boStartLoc, boEndLoc, false, 0, existing_asterisk, false);
			}
			return DefName;
		}

		// Array e.g., a[i], A.x[0]
		else if (isa<ArraySubscriptExpr>(bo_lhs)) {
			ArraySubscriptExpr *ArraySubscriptExprPtr = cast<ArraySubscriptExpr>(bo_lhs);

			Expr * base = ArraySubscriptExprPtr->getBase()->IgnoreParenCasts();
			Expr * idx = ArraySubscriptExprPtr->getIdx()->IgnoreParenCasts();

			SourceLocation idxStartLoc = idx->getLocStart();
			SourceLocation idxEndLoc = endLocExpr(idx);
			insertUseProbeSubExpr(base, boStartLoc, boEndLoc, true);
			insertUseProbeSubExpr(idx, idxStartLoc, idxEndLoc, false);

			std::string array_name = ExprWOSideEffect(ArraySubscriptExprPtr);
			insertProbeFront(ExprWOSideEffect(ArraySubscriptExprPtr), boStartLoc, boEndLoc, false, 0, existing_asterisk, false);
			return array_name;
		}

		// Structure Member Variable A.x
		else if (isa<MemberExpr>(bo_lhs)) {
			MemberExpr *MemberExprPtr = cast<MemberExpr>(bo_lhs);
			Expr * base = MemberExprPtr->getBase()->IgnoreParenCasts();
			std::string baseName = insertUseProbeSubExpr(base, boStartLoc, boEndLoc, true);
			if (!isBitField(MemberExprPtr)) {
				std::string MemberExprName = Expr2String(MemberExprPtr);
				std::string DefName = "";
				unsigned int i = 0, j = 0;
				int numParentheses = 0;
				while (i < baseName.length() && j < MemberExprName.length()) {
					if (baseName[i] == MemberExprName[j] && MemberExprName[j] != '(' && MemberExprName[j] != ')') {
						DefName += baseName[i];
						i++, j++;
					} else if (MemberExprName[j] == '(' || MemberExprName[j] == ')') {
						if (MemberExprName[j] == '(') numParentheses += 1;
						else numParentheses -= 1;
						DefName += MemberExprName[j];
						j++;
					} else if (baseName[i] == '(' || baseName[i] == ')') {
						DefName += baseName[i];
						i++;
					} else
						j++;
				}
				while (i < baseName.length()) {
					DefName += baseName[i];
					i++;
				}
				if (numParentheses > 0) {
					j = MemberExprName.length()-1;
					while (numParentheses != 0) {
						if (MemberExprName[j] == ')') {
							numParentheses -= 1;
							if (numParentheses == 0) break;
						}
						j--;
					}
				}
				while (j < MemberExprName.length()) {
					DefName += MemberExprName[j];
					j++;
				}
				i = 0;
				int continuous_open_bracket = 0;
				int continuous_close_bracket = 0;
				while (i < DefName.length()) {
					if (DefName[i] == '(' && DefName[i+1] == '(') {
						continuous_open_bracket++;
						i += 2;
					} else if (DefName[i] == ')' && DefName[i+1] == ')') {
						continuous_close_bracket++;
						i += 2;
					} else
						i++;
				}

				if (continuous_open_bracket == continuous_close_bracket && continuous_open_bracket > 0) {
					std::string tempDefName = "";
					i = 0;
					while (i < DefName.length()) {
						if (DefName[i] == '(' && DefName[i+1] == '(') {
							tempDefName += '(';
							i += 2;
						} else if (DefName[i] == ')' && DefName[i+1] == ')') {
							tempDefName += ')';
							i += 2;
						} else {
							tempDefName += DefName[i];
							i++;
						}
					}
					DefName = tempDefName;
				}

				i = 0;
				if (ExprIsPointer(MemberExprPtr)) {
					i = numAsterisk(MemberExprPtr->getType().getCanonicalType().getTypePtr()->getPointeeType());
					insertProbeFront(DefName, boStartLoc, boEndLoc, false, i, existing_asterisk, false);
				} else {
					insertProbeFront(DefName, boStartLoc, boEndLoc, false, i, existing_asterisk, false);
				}
				return DefName;
			}
		}

		// Pointer Type with Asterisk *
		else if (isa<UnaryOperator>(bo_lhs) && (cast<UnaryOperator>(bo_lhs))->getOpcode() == UO_Deref) {
			UnaryOperator *UnaryOperatorPtr = cast<UnaryOperator>(bo_lhs);
			existing_asterisk += 1;
			Expr *SubExpr = UnaryOperatorPtr->getSubExpr()->IgnoreParenCasts();
				
			// *p++ or *p+=2
			if (isa<UnaryOperator>(SubExpr)) {
				UnaryOperator *uo = cast<UnaryOperator>(SubExpr);
				if (uo->isIncrementDecrementOp()) {
					Expr * se = uo->getSubExpr()->IgnoreParenCasts();
					if (uo->isPrefix()) {
						std::string DefName = ExprWOSideEffect(se);
						insertProbeFront(DefName, boStartLoc, boEndLoc, false, 0, existing_asterisk, false);
					} else {
						std::string DefName = ExprWOSideEffect(se);
						insertProbeFront(DefName, boStartLoc, boEndLoc, false, 0, existing_asterisk, false);
					}
				}
			} else if (isa<CompoundAssignOperator>(SubExpr)) {

			} else {
				insertProbeBinaryOperatorDef(bo, SubExpr, existing_asterisk);
			} 
		}
		return Expr2String(bo_lhs);
	}

	// DEF: Binary Operator (Not compound)
	bool VisitBinaryOperator(BinaryOperator *bo) {
		// x = 2
		if (bo->isAssignmentOp() && !isa<CompoundAssignOperator>(bo)) {
			// Insert probe for DEF variable
			Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
			Expr *bo_rhs = bo->getRHS()->IgnoreParenCasts();
			std::string lhsName = insertProbeBinaryOperatorDef(bo, bo_lhs, 0);
			lhsName = ExprWOSideEffect(bo_lhs);

			if (addressable(bo_lhs, lhsName)) {
				// Save RHS start location and end location to handle variable assignment
				BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
				SourceLocation assignRhsStartLoc, assignRhsEndLoc;

				SourceRange exprRange = bo_rhs->getSourceRange();
				int rangeSize = MyRewriter.getRangeSize(exprRange);

				assignRhsStartLoc = bo_rhs->getLocStart();
				assignRhsEndLoc = bo_rhs->getLocStart().getLocWithOffset(rangeSize);

				// MAYBE this loop can be erased
				while (true) {
					if (!assignRhsEndLocVec.empty() && isBefore(assignRhsEndLocVec.front(), assignRhsStartLoc)) {
						assignRhsStartLocVec.erase(assignRhsStartLocVec.begin());
						assignRhsEndLocVec.erase(assignRhsEndLocVec.begin());
						assignLhsNameVec.erase(assignLhsNameVec.begin());
					} else 
						break;
				}
				assignRhsStartLocVec.push_back(assignRhsStartLoc);
				assignRhsEndLocVec.push_back(assignRhsEndLoc);
				assignLhsNameVec.push_back(lhsName);
			}
		}
		return true;
	}

	// DEF: Declaration
	bool VisitDeclStmt(DeclStmt *ds) {
		SourceLocation EndLoc = ds->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(ds->getSourceRange()));
		for (DeclGroupRef::const_iterator b = ds->decl_begin(), e = ds->decl_end(); b != e; ++b) {
			if (isa<VarDecl>(*b) && !global_variable) {
				VarDecl *VarDeclPtr = cast<VarDecl>(*b);
				std::string DefName = VarDeclPtr->getNameAsString();

				// Register Type
				if (VarDeclPtr->getStorageClass() == SC_Register) {
					registerVariable.push_back(DefName);
				}
				else if (VarDeclPtr->getType().getCanonicalType().getTypePtr()->isIncompleteType()) {
					continue;
				}
				else if (VarDeclPtr->getType().getCanonicalType().getTypePtr()->isNullPtrType()) {
					continue;
				}
				else if (VarDeclPtr->getType().getCanonicalType().getTypePtr()->isPointerType()) {
					int i = numAsterisk(VarDeclPtr->getType().getCanonicalType().getTypePtr()->getPointeeType());
					if (VarDeclPtr->hasInit()) {
						insertInitializationProbe(DefName, EndLoc, i);
						
						if (notRegisterVariable(DefName)) {
							BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
							SourceLocation assignRhsStartLoc, assignRhsEndLoc;

							Expr* initExpr = VarDeclPtr->getInit();
							SourceRange exprRange = initExpr->getSourceRange();
							int rangeSize = MyRewriter.getRangeSize(exprRange);
	
							assignRhsStartLoc = initExpr->getLocStart();
							assignRhsEndLoc = initExpr->getLocStart().getLocWithOffset(rangeSize);

							assignRhsStartLocVec.push_back(assignRhsStartLoc);
							assignRhsEndLocVec.push_back(assignRhsEndLoc);
							assignLhsNameVec.push_back(DefName);
						}
					}
					if (VarDeclPtr->hasLocalStorage()) {
						insertProbeLocalVar(DefName, EndLoc, i, 0);
					}
				}
				else {
					if (VarDeclPtr->hasInit()) {
						insertInitializationProbe(DefName, EndLoc, 0);

						if (notRegisterVariable(DefName)) {
							BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
							SourceLocation assignRhsStartLoc, assignRhsEndLoc;

							Expr* initExpr = VarDeclPtr->getInit();
							SourceRange exprRange = initExpr->getSourceRange();
							int rangeSize = MyRewriter.getRangeSize(exprRange);
	
							assignRhsStartLoc = initExpr->getLocStart();
							assignRhsEndLoc = initExpr->getLocStart().getLocWithOffset(rangeSize);

							assignRhsStartLocVec.push_back(assignRhsStartLoc);
							assignRhsEndLocVec.push_back(assignRhsEndLoc);
							assignLhsNameVec.push_back(DefName);
						}
					}
					if (VarDeclPtr->hasLocalStorage()) {
						insertProbeLocalVar(DefName, EndLoc, 0, 0);
					}
				}
			}
		}
		return true;
	}

	bool VisitUnaryOperator(UnaryOperator * uo) {

		SourceLocation uoStartLoc = uo->getLocStart();
		SourceLocation uoEndLoc = uo->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(uo->getSourceRange()));

		// USE
		//defStartLocList.push_back(uoStartLoc);

		// DEF & USE: Increment or Decrement Unary Operator
		if (uo->isIncrementDecrementOp()) {
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();

			std::string Name = Expr2String(se);
			if (notRegisterVariable(Name)) {
				if (ExprIsPointer(se)) {
					//int i = numAsterisk(se->getType().getCanonicalType().getTypePtr()->getPointeeType());
					if (uo->isPrefix()) {
						Name = insertUseProbeSubExpr(se, uoStartLoc, uoEndLoc, false);
					} else {
						Name = insertUseProbeSubExpr(se, uoStartLoc, uoEndLoc, false);
					}
				}
				else {
					if (uo->isPrefix()) {
						Name = insertUseProbeSubExpr(se, uoStartLoc, uoEndLoc, false);
					} else if (uo->isPostfix()) {
						// (USE: &i, (DEF: &i, i++))
						Name = insertUseProbeSubExpr(se, uoStartLoc, uoEndLoc, false);
					}
				}
			}
		}

		// USE: Address Unary Operator &
		else if (uo->getOpcode() == UO_AddrOf) {
			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			insertUseProbeSubExpr(se, uoStartLoc, uoEndLoc, false);
			// ASSUMPTION: int *x = &y; The value of y does not effect the value of x.
		}

		else if (uo->getOpcode() == UO_Deref) {
			if (std::find(defStartLocList.begin(), defStartLocList.end(), uoStartLoc) == defStartLocList.end()) {
				insertUseProbeSubExpr(uo, uoStartLoc, uoEndLoc, false);

				if (!isFunctionArgument(uoStartLoc, uoEndLoc)) {
					vector<std::string> lhsNameVec = lhsOfAssignmentOperator(uoStartLoc, uoEndLoc);
					std::string lhsName;

					for (unsigned i = 0; i < lhsNameVec.size(); i++) {
						lhsName = lhsNameVec[i];
						std::string rhsName = ExprWOSideEffect(uo);
						if (isa<MemberExpr>(uo)) {
							MemberExpr *me = cast<MemberExpr>(uo);
							if (!isBitField(me) && notRegisterVariable(rhsName) && !ExprIsIncomplete(me))
								insertAssignmentProbe(lhsName, rhsName, uoStartLoc, uoEndLoc);
						}
						else if (notRegisterVariable(rhsName) && !ExprIsIncomplete(uo))
							insertAssignmentProbe(lhsName, rhsName, uoStartLoc, uoEndLoc);
					}
				}
			}
		}

		return true;
	}

	// DEF&USE: Compound Assignment Operator
	bool VisitCompoundAssignOperator(CompoundAssignOperator *cao) {

		SourceLocation caoStartLoc = cao->getLocStart();
		//SourceLocation caoEndLoc = endLocExpr(cao);

		// USE
		defStartLocList.push_back(caoStartLoc);
		
		Expr * se = cao->getLHS()->IgnoreParenCasts();
		std::string Name = ExprWOSideEffect(se);
		
		if (notRegisterVariable(Name) && ExprIsPointer(se)) {

			if (addressable(se, Name)) {
				// Save RHS start location and end location to handle variable assignment
				BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
				SourceLocation assignRhsStartLoc, assignRhsEndLoc;

				Expr *cao_rhs = cao->getRHS();
				SourceRange exprRange = cao_rhs->getSourceRange();
				int rangeSize = MyRewriter.getRangeSize(exprRange);
	
				assignRhsStartLoc = cao_rhs->getLocStart();
				assignRhsEndLoc = cao_rhs->getLocStart().getLocWithOffset(rangeSize);

				while (true) {
					if (!assignRhsEndLocVec.empty() && isBefore(assignRhsEndLocVec.front(), assignRhsStartLoc)) {
						assignRhsStartLocVec.erase(assignRhsStartLocVec.begin());
						assignRhsEndLocVec.erase(assignRhsEndLocVec.begin());
						assignLhsNameVec.erase(assignLhsNameVec.begin());
					} else 
						break;
				}
				assignRhsStartLocVec.push_back(assignRhsStartLoc);
				assignRhsEndLocVec.push_back(assignRhsEndLoc);
				assignLhsNameVec.push_back(Name);
			}
		}
		
		else if (isa<MemberExpr>(se)) {
			MemberExpr *me = cast<MemberExpr>(se);
			if (!isBitField(me)) {

				if (addressable(se, Name)) {
					// Save RHS start location and end location to handle variable assignment
					BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
					SourceLocation assignRhsStartLoc, assignRhsEndLoc;

					Expr *cao_rhs = cao->getRHS();
					SourceRange exprRange = cao_rhs->getSourceRange();
					int rangeSize = MyRewriter.getRangeSize(exprRange);

					assignRhsStartLoc = cao_rhs->getLocStart();
					assignRhsEndLoc = cao_rhs->getLocStart().getLocWithOffset(rangeSize);

					while (true) {
						if (!assignRhsEndLocVec.empty() && isBefore(assignRhsEndLocVec.front(), assignRhsStartLoc)) {
							assignRhsStartLocVec.erase(assignRhsStartLocVec.begin());
							assignRhsEndLocVec.erase(assignRhsEndLocVec.begin());
							assignLhsNameVec.erase(assignLhsNameVec.begin());
						} else 
							break;
					}
					assignRhsStartLocVec.push_back(assignRhsStartLoc);
					assignRhsEndLocVec.push_back(assignRhsEndLoc);
					assignLhsNameVec.push_back(Name);
				}
			}
		}
		else if (notRegisterVariable(Name)) {

			if (addressable(se, Name)) {
				// Save RHS start location and end location to handle variable assignment
				BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 
				SourceLocation assignRhsStartLoc, assignRhsEndLoc;

				Expr *cao_rhs = cao->getRHS();
				SourceRange exprRange = cao_rhs->getSourceRange();
				int rangeSize = MyRewriter.getRangeSize(exprRange);

				assignRhsStartLoc = cao_rhs->getLocStart();
				assignRhsEndLoc = cao_rhs->getLocStart().getLocWithOffset(rangeSize);

				while (true) {
					if (!assignRhsEndLocVec.empty() && isBefore(assignRhsEndLocVec.front(), assignRhsStartLoc)) {
						assignRhsStartLocVec.erase(assignRhsStartLocVec.begin());
						assignRhsEndLocVec.erase(assignRhsEndLocVec.begin());
						assignLhsNameVec.erase(assignLhsNameVec.begin());
					} else 
						break;
				}
				assignRhsStartLocVec.push_back(assignRhsStartLoc);
				assignRhsEndLocVec.push_back(assignRhsEndLoc);
				assignLhsNameVec.push_back(Name);
			}
		}
		return true;
	}

	bool VisitFunctionDecl(FunctionDecl *f) {
		// Only function definitions (with bodies), not declarations.
		if (f->hasBody() && f->isThisDeclarationADefinition()) {
			registerVariable.clear();
			if (first_function_body) {
				insertFilePointer(f->getLocStart());
				first_function_body = false;
			}

			functionName = f->getName().data();
			if (functionName == "main") 
				initializeFilePointer(f->getBody()->getLocStart().getLocWithOffset(1));
			
			unsigned param_id = 1;
			for (ArrayRef<ParmVarDecl *>::const_iterator b = f->param_begin(), e = f->param_end(); b != e; ++b) {
				const ParmVarDecl *ParmVarDeclPtr = cast<ParmVarDecl>(*b);
				std::string Name = ParmVarDeclPtr->getNameAsString();

				if (Name != "" && functionName != "main") {
					if (ParmVarDeclPtr->getStorageClass() == SC_Register)
						registerVariable.push_back(Name);
					else {/*if (!(ParmVarDeclPtr->getType().getCanonicalType().getTypePtr()->isPointerType())) */
						insertProbeLocalVar(Name, f->getBody()->getLocStart().getLocWithOffset(1), 0, param_id);
						param_id += 1;
					}
				}
			}
		}
		return true;
	}

	bool VisitMemberExpr(MemberExpr* me) {
		if (!global_variable) {
			SourceLocation StartLoc = me->getLocStart();
			SourceLocation EndLoc = me->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(me->getSourceRange()));

			if (std::find(defStartLocList.begin(), defStartLocList.end(), StartLoc) == defStartLocList.end()) {
				insertUseProbeSubExpr(me, StartLoc, EndLoc, false);

				if (!isFunctionArgument(StartLoc, EndLoc)) {
					vector<std::string> lhsNameVec = lhsOfAssignmentOperator(StartLoc, EndLoc);
					std::string lhsName;

					for (unsigned i = 0; i < lhsNameVec.size(); i++) {
						lhsName = lhsNameVec[i];
						if (lhsName != "") {
							std::string rhsName = ExprWOSideEffect(me);
							if (!isBitField(me) && notRegisterVariable(rhsName) && !ExprIsIncomplete(me)) { 
								// CANDIDATE3
								insertAssignmentProbe(lhsName, rhsName, StartLoc, EndLoc);
							}
						}
					}
				}
			}
		}
		return true;
	}

	bool VisitArraySubscriptExpr(ArraySubscriptExpr* ase) {
		if (!global_variable) {
			SourceLocation StartLoc = ase->getLocStart();
			SourceLocation EndLoc = ase->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(ase->getSourceRange()));

			if (std::find(defStartLocList.begin(), defStartLocList.end(), StartLoc) == defStartLocList.end()) {
				insertUseProbeSubExpr(ase, StartLoc, EndLoc, false);

				if (!isFunctionArgument(StartLoc, EndLoc)) {
					vector<std::string> lhsNameVec = lhsOfAssignmentOperator(StartLoc, EndLoc);
					std::string lhsName;

					for (unsigned i = 0; i < lhsNameVec.size(); i++) {
						lhsName = lhsNameVec[i];
						std::string rhsName = ExprWOSideEffect(ase);
						if (isa<MemberExpr>(ase)) {
							MemberExpr *me = cast<MemberExpr>(ase);
							if (!isBitField(me) && notRegisterVariable(rhsName) && !ExprIsIncomplete(me))
								// CANDIDATE4
								insertAssignmentProbe(lhsName, rhsName, StartLoc, EndLoc);
						}
						else if (notRegisterVariable(rhsName) && !ExprIsIncomplete(ase))
							// CANDIDATE5
							insertAssignmentProbe(lhsName, rhsName, StartLoc, EndLoc);
					}
				}
			}
		}
		return true;
	}

	bool VisitDeclRefExpr(DeclRefExpr* dre) {
		if (!global_variable) {
			SourceLocation StartLoc = dre->getLocStart();
			SourceLocation EndLoc = dre->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(dre->getSourceRange()));

			if (isa<EnumConstantDecl>(dre->getDecl()) || isa<FunctionDecl>(dre->getDecl())) 
				return true;

			if (std::find(defStartLocList.begin(), defStartLocList.end(), StartLoc) == defStartLocList.end()) {
				insertUseProbeSubExpr(dre, StartLoc, EndLoc, false);

				if (!isFunctionArgument(StartLoc, EndLoc)) {
					vector<std::string> lhsNameVec = lhsOfAssignmentOperator(StartLoc, EndLoc);
					std::string lhsName; 

					for (unsigned i = 0; i < lhsNameVec.size(); i++) {
						lhsName = lhsNameVec[i];
						std::string rhsName = ExprWOSideEffect(dre);
						if (isa<MemberExpr>(dre)) {
							MemberExpr *me = cast<MemberExpr>(dre);
							if (!isBitField(me) && notRegisterVariable(rhsName) && !ExprIsIncomplete(me))
								insertAssignmentProbe(lhsName, rhsName, StartLoc, EndLoc);
						}
						else if (notRegisterVariable(rhsName) && !ExprIsIncomplete(dre))
							insertAssignmentProbe(lhsName, rhsName, StartLoc, EndLoc);
					}
				}
			}
		}
		return true;
	}

	void DoNotInsertProbe(Expr * e) {

		if (isa<ImplicitCastExpr>(e) || isa<CStyleCastExpr>(e)) {
			ImplicitCastExpr *ice = cast<ImplicitCastExpr>(e);
			Expr *se = ice->getSubExpr()->IgnoreParenCasts();
			DoNotInsertProbe(se);
		}

		// SubExpr is Array
		else if (isa<ArraySubscriptExpr>(e)) {
			ArraySubscriptExpr *asep = cast<ArraySubscriptExpr>(e);

			Expr * base = asep->getBase()->IgnoreParenCasts();
			Expr * idx = asep->getIdx()->IgnoreParenCasts();

			DoNotInsertProbe(base);
			DoNotInsertProbe(idx);
		}

		// SubExpr is Structure Member
		else if (isa<MemberExpr>(e)) {
			MemberExpr *me = cast<MemberExpr>(e);

			Expr * base = me->getBase()->IgnoreParenCasts();
			DoNotInsertProbe(base);
		}

		// SubExpr is DeclRefExpr
		else if (isa<DeclRefExpr>(e)) {
			DeclRefExpr *dre = cast<DeclRefExpr>(e);
			// USE
			defStartLocList.push_back(dre->getLocStart());
		}

		// SubExpr is UnaryOperator with asterisk *
		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e))->getOpcode() == UO_Deref) {
			UnaryOperator *uo = cast<UnaryOperator>(e);

			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			DoNotInsertProbe(se);
		}

		else if (isa<UnaryOperator>(e) && (cast<UnaryOperator>(e)->isIncrementDecrementOp())) {
			UnaryOperator *uo = cast<UnaryOperator>(e);

			Expr * se = uo->getSubExpr()->IgnoreParenCasts();
			DoNotInsertProbe(se);
		}

		else if (isa<BinaryOperator>(e)) {
			BinaryOperator *bo = cast<BinaryOperator>(e);
			if (bo->isAssignmentOp()) {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				DoNotInsertProbe(bo_lhs);
			}
			else {
				Expr *bo_lhs = bo->getLHS()->IgnoreParenCasts();
				Expr *bo_rhs = bo->getRHS()->IgnoreParenCasts();
				DoNotInsertProbe(bo_lhs);
				DoNotInsertProbe(bo_rhs);
			}
		}
	}

	bool VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr * ueotte) {
		Expr * e = ueotte->getArgumentExpr()->IgnoreParenCasts();
		DoNotInsertProbe(e);
		return true;
	}

	bool VisitCallExpr(CallExpr *ce) {
		FunctionDecl *CalleeFuncDecl = ce->getDirectCallee();
		std::string parameters = "";
		std::string CalleeFuncName;
		if (CalleeFuncDecl != NULL) {
			CalleeFuncName = CalleeFuncDecl->getNameInfo().getName().getAsString();
			SourceLocation ceStartLoc = ce->getLocStart();
			SourceLocation ceEndLoc = ce->getLocEnd().getLocWithOffset(1);
			insertCallExprProbe(ce, CalleeFuncName, ceStartLoc, ceEndLoc);

			vector<std::string> lhsNameVec = lhsOfAssignmentOperator(ceStartLoc, ceEndLoc);
			std::string lhsName;

			for (unsigned i = 0; i < lhsNameVec.size(); i++) {
				lhsName = lhsNameVec[i];
				insertProbeCallExprInAssignment(ce, lhsName, CalleeFuncName, ceStartLoc, ceEndLoc);
			}
		} else
			CalleeFuncName = "CALLBACK";

		unsigned arg_id = 1;
		for (CallExpr::arg_iterator b = ce->arg_begin(), e = ce->arg_end(); b != e; ++b) {
			Expr * expr = cast<Expr>(*b)->IgnoreParenCasts();

			SourceLocation argumentStartLoc = expr->getLocStart();
			SourceLocation argumentEndLoc = expr->getLocStart().getLocWithOffset(MyRewriter.getRangeSize(expr->getSourceRange()));
			//SourceLocation argumentEndLoc = endLocExpr(expr);

			BeforeThanCompare<SourceLocation> isBefore(*ptrMySourceMgr); 

			argumentStartLocVec.push_back(argumentStartLoc);
			argumentEndLocVec.push_back(argumentEndLoc);

			defStartLocList.push_back(expr->getLocStart());

			if (isa<CallExpr>(expr)) {
				insertProbeFuncArgIsCallExpr(CalleeFuncName, cast<CallExpr>(expr), argumentStartLoc, argumentEndLoc, arg_id);
				//cout << "CallExpr" << endl;
			} else if (isa<IntegerLiteral>(expr)
				|| isa<FloatingLiteral>(expr)
				|| isa<CharacterLiteral>(expr)) {
				insertProbeFuncArgIsLiteral(CalleeFuncName, functionName, ExprWOSideEffect(expr), argumentStartLoc, argumentEndLoc, arg_id);
				//cout << "Literal" << endl;
			} else if (!expr->isRValue() &&
				(isa<DeclRefExpr>(expr)
				|| isa<ArraySubscriptExpr>(expr)
				|| isa<MemberExpr>(expr)
				|| (isa<UnaryOperator>(expr) && !(cast<UnaryOperator>(expr)->isIncrementDecrementOp() && cast<UnaryOperator>(expr)->isPrefix())))) {
				insertProbeFuncArgIsVariable(CalleeFuncName, functionName, ExprWOSideEffect(expr), argumentStartLoc, argumentEndLoc, arg_id);
				//cout << "Variable" << endl;
			} else {
				insertProbeFuncArgIsLiteral(CalleeFuncName, functionName, ExprWOSideEffect(expr), argumentStartLoc, argumentEndLoc, arg_id);
				//cout << "Newly Defined in " << functionName << endl;
			}
			arg_id += 1;
		}

		return true;
	}

private:
  std::string CallerFuncName;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer() : Visitor() {}

  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
		global_variable = false;
		if (isa<EnumDecl>(*b) || isa<RecordDecl>(*b))
			continue;
		else if (isa<VarDecl>(*b)) {
			global_variable = true;
		}
		Visitor.TraverseDecl(*b);
    }
    return true;
  }

private:
  MyASTVisitor Visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {


  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {

	MyLangOpts = CI.getLangOpts();
    ptrMySourceMgr= &(CI.getSourceManager());
    MyRewriter= Rewriter(*ptrMySourceMgr, MyLangOpts);

    return llvm::make_unique<MyASTConsumer>();
  }

};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MyOptionCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());
  int rtn_flag;

  // ClangTool::run accepts a FrontendActionFactory, which is then used to
  // create new objects implementing the FrontendAction interface. Here we use
  // the helper newFrontendActionFactory to create a default factory that will
  // return a new MyFrontendAction object every time.
  // To further customize this, we could create our own factory class.
  rtn_flag = Tool.run(newFrontendActionFactory<MyFrontendAction>().get());

  const RewriteBuffer *RewriteBuf = MyRewriter.getRewriteBufferFor
     ((*ptrMySourceMgr).getMainFileID());
  ofstream out_file ("output.c");
  out_file << string(RewriteBuf->begin(), RewriteBuf->end());
  out_file.close();

  return rtn_flag;
}
