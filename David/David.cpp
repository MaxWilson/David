// David.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <set>
#include <map>
#include <algorithm> // for std::for_each

using namespace std;

enum BinaryOperator { AND, OR, IMPLIES };
enum UnaryOperator { NOT };
enum ExpressionType { Variable, Unary, Binary };

// would be cleaner to use std::variant here, see https://arne-mertz.de/2018/05/modern-c-features-stdvariant-and-stdvisit/ for examples
// but I'm trying to avoid introducing new concepts for you to worry about
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
	else throw ImpossibleFailure();
}

void extractVariablesHelper(Expression expr, set<string>& variables) {
	if (expr.type == Variable) {
		variables.insert(expr.variableName);
	}
	else if (expr.type == Unary && expr.unaryOperator == NOT) {
		extractVariablesHelper(*expr.expression1, variables);
	}
	else if (expr.type == Binary) {
		extractVariablesHelper(*expr.expression1, variables);
		extractVariablesHelper(*expr.expression2, variables);
	}
	else throw ImpossibleFailure();
}

auto extractVariables(Expression expr) {
	set<string> retval;
	extractVariablesHelper(expr, retval);
	return retval;
}

int main()
{
	auto expression = Expression(Expression(Expression("A"), AND, Expression("B")), IMPLIES, Expression(NOT, Expression("A")));
	cout << render(expression) << endl;
	auto variables = extractVariables(expression);
	for_each(begin(variables), end(variables), [](auto v) { cout << v << " ";  });
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
