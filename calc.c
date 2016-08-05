#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))

typedef struct {
	char sym;
	int precedence;
	bool lassoc;
} Operator;

static Operator operators[] = {
	{ '^', 3, false },
	{ '*', 2, true },
	{ '/', 2, true },
	{ '+', 1, true },
	{ '-', 1, true },
};

typedef enum {
	FN_MAX,
	FN_SUM,
	FN_SQRT,
} FunctionId;

typedef struct {
	FunctionId id;
	char name[16];
	int arity;
} Function;

static Function functions[] = {
	{ FN_MAX,  "max",  2 },
	{ FN_SUM,  "sum",  3 },
	{ FN_SQRT, "sqrt", 1 },
};

typedef enum {
	TK_NUMBER,
	TK_OPERATOR,
	TK_FUNCTION,
	TK_LPAREN,
	TK_RPAREN,
	TK_COMMA,
} TokenType;

typedef struct {
	TokenType type;
	union {
		double number;
		Operator *operator;
		Function *function;
	};
} Token;

static void print_token(Token tk) {
	switch (tk.type) {
		case TK_NUMBER:
			printf("%g", tk.number);
			break;
		case TK_OPERATOR:
			printf("%c", tk.operator->sym);
			break;
		case TK_FUNCTION:
			printf("%s", tk.function->name);
			break;
		case TK_LPAREN:
			printf("(");
			break;
		case TK_RPAREN:
			printf(")");
			break;
		case TK_COMMA:
			printf(",");
			break;
	}
}

// Return codes:
// 1 got a token
// 0 end of input
// -1 error
static int eat_token(Token *tk, char **str) {
	char *at = *str;

	while (isspace(*at))
		at++;

	if (!*at)
		return 0;

	*tk = (Token){};

	if (isdigit(*at)) {
		tk->type = TK_NUMBER;

		char *end = NULL;
		tk->number = strtod(at, &end);
		at = end;
	} else if (isalpha(*at)) {
		tk->type = TK_FUNCTION;

		char name[sizeof(tk->function->name)] = {};
		for (int i = 0; isalnum(*at) && i < sizeof(name) - 1; i++)
			name[i] = *at++;

		for (int i = 0; i < countof(functions); i++) {
			if (!strcmp(name, functions[i].name))
				tk->function = functions + i;
		}
		if (!tk->function) {
			fprintf(stderr, "undefined function \"%s\"\n", name);
			return -1;
		}
	} else if (*at == '(')	{
		at++;
		tk->type = TK_LPAREN;
	} else if (*at == ')')	{
		at++;
		tk->type = TK_RPAREN;
	} else if (*at == ',')	{
		at++;
		tk->type = TK_COMMA;
	} else {
		Operator *op = NULL;
		for (int i = 0; i < countof(operators); i++) {
			if (operators[i].sym == *at) {
				op = operators + i;
				break;
			}
		}
		if (op) {
			tk->type = TK_OPERATOR;
			tk->operator = op;
			at++;
		} else {
			fprintf(stderr, "Invalid token at \"%s\"\n", at);
			return -1;
		}
	}

	*str = at;
	return 1;
}

typedef struct {
	Token *tokens;
	int cap;
	int len;
} TokenStack;

static inline void tk_push(TokenStack *stk, Token tk) {
	if (stk->len >= stk->cap) {
		fprintf(stderr, "too many tokens\n");
		exit(1);
	}
	stk->tokens[stk->len++] = tk;
}

static inline Token tk_pop(TokenStack *stk) {
	if (stk->len == 0) {
		fprintf(stderr, "pop on empty stack\n");
		exit(1);
	}
	Token tk = stk->tokens[stk->len - 1];
	stk->len--;
	return tk;
}

static inline Token *tk_top(TokenStack *stk) {
	Token *tk = NULL;
	if (stk->len)
		tk = stk->tokens + (stk->len - 1);
	return tk;
}

static void print_stack(TokenStack *stk) {
	for (int i = 0; i < stk->len; i++) {
		if (i)
			printf(" ");
		print_token(stk->tokens[i]);
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		return 1;
	}

	static Token operators_data[256];
	static Token output_data[256];

	TokenStack operators = { operators_data, countof(operators_data) };
	TokenStack output = { output_data, countof(output_data) };

	char *cursor = argv[1];
	int status = 0;
	Token token;

	while ((status = eat_token(&token, &cursor)) == 1) {
		switch (token.type) {
			case TK_NUMBER:
				tk_push(&output, token);
				break;
			case TK_OPERATOR: {
				Operator *op1 = token.operator;
				for (;;) {
					Token *top = tk_top(&operators);

					bool move = false;
					if (top && top->type == TK_OPERATOR) {
						Operator *op2 = top->operator;
						if (op1->lassoc) {
							move = (op1->precedence <= op2->precedence);
						} else {
							move = (op1->precedence < op2->precedence);
						}
					}
					if (move) {
						tk_push(&output, tk_pop(&operators));
					} else {
						break;
					}
				}
				tk_push(&operators, token);
			} break;
			case TK_FUNCTION:
			case TK_LPAREN:
				tk_push(&operators, token);
				break;
			case TK_RPAREN: {
				for (;;) {
					Token *top = tk_top(&operators);
					if (!top) {
						fprintf(stderr, "mismatched parens\n");
						return 1;
					}

					if (top->type == TK_LPAREN)
						break;

					tk_push(&output, tk_pop(&operators));
				}
				tk_pop(&operators); // pop the '('

				Token *top = tk_top(&operators);
				if (top && top->type == TK_FUNCTION)
					tk_push(&output, tk_pop(&operators));
			} break;
			case TK_COMMA:
				for (;;) {
					Token *top = tk_top(&operators);
					if (!top) {
						fprintf(stderr, "unexpected comma\n");
						return 1;
					}

					if (top->type == TK_LPAREN)
						break;

					tk_push(&output, tk_pop(&operators));
				}
				break;
			}

			print_token(token);
			printf("\toperators [");
			print_stack(&operators);
			printf("] output [");
			print_stack(&output);
			printf("]\n");
	}
	if (status == -1)
		return 1;

	Token *top = 0;
	while ((top = tk_top(&operators))) {
		if (top->type == TK_LPAREN || top->type == TK_RPAREN) {
			fprintf(stderr, "paren mismatch");
			return 1;
		}

		tk_push(&output, tk_pop(&operators));
	}

	printf("RPN: ");
	print_stack(&output);
	printf("\n");

	// Eval RPN

	// reuse empty operators stack
	TokenStack eval_stack = { operators.tokens, operators.cap };

	for (int i = 0; i < output.len; i++) {
		Token tk = output.tokens[i];

		switch (tk.type) {
			case TK_NUMBER:
				tk_push(&eval_stack, tk);
				break;
			case TK_OPERATOR: {
				Token rhs = tk_pop(&eval_stack);
				Token lhs = tk_pop(&eval_stack);
				assert(lhs.type == TK_NUMBER && rhs.type == TK_NUMBER);

				Token result = { TK_NUMBER };
				switch (tk.operator->sym) {
					case '^': result.number = pow(lhs.number, rhs.number); break;
					case '*': result.number = lhs.number * rhs.number; break;
					case '/': result.number = lhs.number / rhs.number; break;
					case '+': result.number = lhs.number + rhs.number; break;
					case '-': result.number = lhs.number - rhs.number; break;
					default: assert(!"unknown operator");
				}
				tk_push(&eval_stack, result);

				printf("> %g %c %g => [", lhs.number, tk.operator->sym, rhs.number);
				print_stack(&eval_stack);
				printf("]\n");
			} break;
			case TK_FUNCTION: {
				// printf("## %s arity = %d stack = [", tk.function->name, tk.function->arity);
				// print_stack(&eval_stack);
				// printf("]\n");
				Token result = { TK_NUMBER };

				double fargs[16];
				assert(tk.function->arity <= countof(fargs));

				// pop in reverse order
				for (int i = tk.function->arity - 1; i >= 0; i--) {
					Token tk = tk_pop(&eval_stack);
					assert(tk.type == TK_NUMBER);
					fargs[i] = tk.number;
				}

				switch (tk.function->id) {
					case FN_MAX: {
						result.number = fargs[0] > fargs[1] ? fargs[0] : fargs[1];
					} break;
					case FN_SUM: {
						result.number = fargs[0] + fargs[1] + fargs[2];
					} break;
					case FN_SQRT: {
						result.number = sqrt(fargs[0]);
					} break;
				}
				tk_push(&eval_stack, result);

				printf("> %s(", tk.function->name);
				for (int i = 0; i < tk.function->arity; i++) {
					if (i)
						printf(", ");
					printf("%g", fargs[i]);
				}
				printf(") => [");
				print_stack(&eval_stack);
				printf("]\n");
			} break;
			default:
				printf("Unexpected token on eval stack ");
				print_token(tk);
				printf("\n");
				assert(0);
		}
	}

	printf("result = [");
	print_stack(&eval_stack);
	printf("]\n");

	return 0;
}
