// David.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <set>
#include <map>
#include <algorithm> // for std::for_each
#include <iomanip> // for setw
#include <functional>

using namespace std;

enum BinaryOperator { AND, OR, IMPLIES };
enum UnaryOperator { NOT };
enum ExpressionType { Variable, Unary, Binary };

// would be cleaner to use std::variant here, see https://arne-mertz.de/2018/05/modern-c-features-stdvariant-and-stdvisit/ for examples
// but I'm trying to avoid introducing new concepts for you to worry about
// Example: 	auto expression = Expression(Expression(Expression("A"), AND, Expression("B")), IMPLIES, Expression(NOT, Expression("A")));
struct Expression {
	ExpressionType type;
	shared_ptr<Expression> expression1;
	shared_ptr<Expression> expression2;
	string variableName;
	BinaryOperator binaryOperator;
	UnaryOperator unaryOperator;
	Expression() : Expression("Uninitialized") { }
	Expression(string variableName) : variableName(variableName), type(Variable) { }
	Expression(UnaryOperator op, Expression expression) : unaryOperator(op), expression1(make_shared<Expression>()), type(Unary) { 
		(*this->expression1) = expression;
	}
	Expression(Expression expression1, BinaryOperator op, Expression expression2) : expression1(make_shared<Expression>()), binaryOperator(op), expression2(make_shared<Expression>()), type(Binary) {
		(*this->expression1) = expression1;
		(*this->expression2) = expression2;
	}
};

typedef pair<Expression, string> ParseOutput;

struct ParseFailure { };
struct ImpossibleFailure { }; // Mark things that should never happen

bool match(string input, string pattern) {
	return (input.size() >= pattern.size()) && (input.substr(0, pattern.size()) == pattern);
}
string consumeWhitespace(string input) {
	string output = input;;
	while (output[0] == ' ') {
		output = output.substr(1, output.size() - 1);
	}
	return output;
}
string consume(string pattern, string input) {
	if (!match(input, pattern)) {
		throw ParseFailure();
	}
	auto output = input.substr(pattern.size(), input.size() - pattern.size()); // get everything but the used-up part of input
	return consumeWhitespace(output);
}

bool isLetter(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

const int MaxDepth = 10; // horrible hack to get around problem with recursive-descent parser on this particular grammar
							// I told you parsing was complicated! (And not worth worrying about for this assignment.)
							// This parameter means that the parser will fail on "large" truth tables 
							// but it will never go into an infinite loop

ParseOutput ParseVariable(string input, int recursiveDepth) {
	if (recursiveDepth > MaxDepth) throw ParseFailure();
	input = consumeWhitespace(input);
	auto variableName = string();
	for (int i = 0; 0 < input.size(); ++i) {
		if (isLetter(input[i])) {
			variableName = variableName + input[i];
		}
		else break;
	}
	if (variableName.size() > 0) {
		return pair<Expression, string>(Expression(variableName), consume(variableName, input));
	}
	else throw ParseFailure();
}

ParseOutput ParseExpression(string input, int recursiveDepth); // forward declaration so we can call it recursively

ParseOutput ParseUnaryExpression(string input, int recursiveDepth) {
	if (recursiveDepth > MaxDepth) throw ParseFailure();
	if (match(input, "(")) {
		auto result = ParseExpression(consume("(", input), recursiveDepth);
		return pair<Expression, string>(result.first, consume(")", result.second));
	}
	auto result = ParseExpression(consume("!", input), recursiveDepth + 1);
	return pair<Expression, string>(Expression(NOT, result.first), result.second);
}

ParseOutput ParseBinaryExpression(string input, int recursiveDepth) {
	if (recursiveDepth > MaxDepth) throw ParseFailure();

	auto lhs = ParseExpression(input, recursiveDepth + 1);
	BinaryOperator op;
	string remaining;
	auto hadSuccess = false;
	try {
		remaining = consume("/\\", lhs.second); // Must escape \ in string, so /\ is written as "/\\"
		op = AND;
		hadSuccess = true;
	} catch(ParseFailure) { }
	if (!hadSuccess) {
		try {
			remaining = consume("\\/", lhs.second); // Must escape \ string, so \/ is written as "\\/"
			op = OR;
			hadSuccess = true;
		}
		catch (ParseFailure) {}
	}
	if (!hadSuccess) {
		remaining = consume("->", lhs.second); // this time if it fails we don't catch it, we just give up
		op = IMPLIES;
		hadSuccess = true;
	}
	// at this point we haven't thrown a ParseFailure, so the parse is valid so far. We try to get a second operand.

	auto rhs = ParseExpression(remaining, recursiveDepth+1);
	return pair<Expression, string>(Expression(lhs.first, op, rhs.first), rhs.second);
}

ParseOutput ParseExpression(string input, int recursiveDepth) {
	try {
		return ParseBinaryExpression(input, recursiveDepth + 1);
	}
	catch (ParseFailure) {}
	// try again as a unary expression
	try {
		return ParseUnaryExpression(input, recursiveDepth + 1);
	}
	catch (ParseFailure) {}
	// try just plain variable, and if that fails then give up
	return ParseVariable(input, recursiveDepth + 1);
}

// recursive-descent parser
Expression Parse(string input) {
	auto result = ParseExpression(input, 0);
	return result.first;
}

string render(Expression expr) {
	if (expr.type == Variable) return expr.variableName;
	else if (expr.type == Unary && expr.unaryOperator == NOT) return "NOT " + render(*expr.expression1);
	else if (expr.type == Binary) {
		if (expr.binaryOperator == AND)
			return render(*expr.expression1) + " AND " + render(*expr.expression2);
		else if (expr.binaryOperator == OR)
			return render(*expr.expression1) + " OR " + render(*expr.expression2);
		else if (expr.binaryOperator == IMPLIES)
			return render(*expr.expression1) + " -> " + render(*expr.expression2);
	}
	throw ImpossibleFailure();
}

void extractVariables(Expression expr, set<string>& variables) {
	if (expr.type == Variable) {
		variables.insert(expr.variableName);
	}
	else if (expr.type == Unary && expr.unaryOperator == NOT) {
		extractVariables(*expr.expression1, variables);
	}
	else if (expr.type == Binary) {
		extractVariables(*expr.expression1, variables);
		extractVariables(*expr.expression2, variables);
	}
	else throw ImpossibleFailure();
}

auto extractVariables(Expression expr) {
	set<string> retval;
	extractVariables(expr, retval);
	return retval;
}

auto generatePossibleAssignments(Expression expr) {
	auto vars = extractVariables(expr);
	auto accumulator = set<map<string, bool>>();
	accumulator.insert(map<string, bool>());
	while (!vars.empty()) {
		auto currentVariable = *vars.begin();
		vars.erase(currentVariable);		
		// need to make sure everything in accumulator turns into two things in the accumulator
		// I'm going to make a new variable 'fresh' and just copy stuff over, but there are easier ways
		auto fresh = set<map<string, bool>>();
		for (auto i = begin(accumulator); i != end(accumulator); ++i) {
			auto assignment = *i;
			auto withTrueValues = map<string, bool>();
			withTrueValues.insert(begin(assignment), end(assignment));
			withTrueValues.insert_or_assign(currentVariable, true);
			auto withFalseValues = map<string, bool>(begin(assignment), end(assignment));
			withFalseValues.insert(begin(assignment), end(assignment));
			withFalseValues.insert_or_assign(currentVariable, false);
			fresh.insert(withTrueValues);
			fresh.insert(withFalseValues);
		}
		accumulator = fresh; // copy back from fresh.
	}
	return accumulator;
}

auto evaluate(Expression expr, map<string, bool> assignment) {
	if (expr.type == Variable) return assignment[expr.variableName];
	else if (expr.type == Unary && expr.unaryOperator == NOT) return !evaluate(*expr.expression1, assignment);
	else if (expr.type == Binary) {
		auto lhs = evaluate(*expr.expression1, assignment);
		auto rhs = evaluate(*expr.expression2, assignment);
		if (expr.binaryOperator == AND)
			return lhs && rhs;
		else if (expr.binaryOperator == OR)
			return lhs || rhs;
		else if (expr.binaryOperator == IMPLIES)
			return (!lhs) || rhs;
	}
	throw ImpossibleFailure();
}

auto printTruthTable(Expression expr) {
	auto assignments = generatePossibleAssignments(expr);
	auto vars = extractVariables(expr);
	// print column headers
	for_each(begin(vars), end(vars), [](auto var) {
		cout << setw(10) << var;
		});
	cout << setw(10) << "Result" << endl;

	// print variable values and final value
	for_each(begin(assignments), end(assignments), [&expr, &vars](auto assignment) {
		// print column headers
		for_each(begin(vars), end(vars), [&assignment](auto var) {
			cout << setw(10) << assignment[var];
			});
		cout << setw(10) << evaluate(expr, assignment) << endl;

		});
}

int main()
{
	auto expr = Parse("(A /\\ B) -> C /\\ A"); // enter your own expression here
	cout << render(expr) << endl;
	printTruthTable(expr);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
