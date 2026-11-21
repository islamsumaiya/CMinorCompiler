
from src.lexer.tokens import TokenType


class ParserError(Exception):
    pass


class Parser:

    def __init__(self, tokens):
        self.tokens = tokens
        self.position = 0

    def current(self):
        if self.position >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[self.position]

    def peek(self, offset=1):
        index = self.position + offset

        if index >= len(self.tokens):
            return self.tokens[-1]

        return self.tokens[index]

    def advance(self):
        token = self.current()

        if token.type != TokenType.EOF:
            self.position += 1

        return token

    def check(self, token_type):
        return self.current().type == token_type

    def match(self, token_type):
        if self.check(token_type):
            return self.advance()

        return None

    def error(self, message):
        token = self.current()

        raise ParserError(
            f"{token.file_path}:{token.line}:{token.column}: "
            f"{message}"
        )

    def expect(self, token_type, message=None):
        token = self.match(token_type)

        if token is None:
            self.error(
                message or f"Expected {token_type.name}"
            )

        return token

    def expect_keyword(self, keyword):
        token = self.current()

        if (
            token.type == TokenType.KEYWORD
            and token.lexeme == keyword
        ):
            return self.advance()

        self.error(f"Expected keyword '{keyword}'")

    # -----------------------------
    # PROGRAM
    # -----------------------------

    def parse(self):
        declarations = []

        while not self.check(TokenType.EOF):
            declarations.append(
                self.parse_external_declaration()
            )

        self.advance()

        return {
            "type": "Program",
            "declarations": declarations
        }

    # -----------------------------
    # TYPE SPECIFIER
    # -----------------------------

    def parse_type(self):
        token = self.current()

        if (
            token.type == TokenType.KEYWORD
            and token.lexeme in ("int", "void", "char")
        ):
            self.advance()
            return token.lexeme

        self.error("Expected type specifier")

    # -----------------------------
    # FUNCTION / GLOBAL VARIABLE
    # -----------------------------

    def parse_external_declaration(self):
        data_type = self.parse_type()

        name = self.expect(
            TokenType.IDENTIFIER,
            "Expected identifier"
        )

        # Function declaration or definition
        if self.match(TokenType.LPAREN):
            parameters = self.parse_parameters()

            self.expect(
                TokenType.RPAREN,
                "Expected ')' after parameters"
            )

            body = self.parse_block()

            return {
                "type": "Function",
                "return_type": data_type,
                "name": name.lexeme,
                "parameters": parameters,
                "body": body
            }

        # Global variable declaration
        declarations = [
            self.parse_declarator(name)
        ]

        while self.match(TokenType.COMMA):
            identifier = self.expect(
                TokenType.IDENTIFIER
            )

            declarations.append(
                self.parse_declarator(identifier)
            )

        self.expect(
            TokenType.SEMICOLON,
            "Expected ';' after declaration"
        )

        return {
            "type": "Declaration",
            "data_type": data_type,
            "declarations": declarations
        }

    # -----------------------------
    # PARAMETERS
    # -----------------------------

    def parse_parameters(self):
        parameters = []

        if (
            self.check(TokenType.KEYWORD)
            and self.current().lexeme == "void"
            and self.peek().type == TokenType.RPAREN
        ):
            self.advance()
            return parameters

        if self.check(TokenType.RPAREN):
            return parameters

        while True:
            data_type = self.parse_type()

            name = self.expect(
                TokenType.IDENTIFIER,
                "Expected parameter name"
            )

            is_array = False

            if self.match(TokenType.LBRACKET):
                self.expect(
                    TokenType.RBRACKET,
                    "Expected ']' in array parameter"
                )
                is_array = True

            parameters.append({
                "type": "Parameter",
                "data_type": data_type,
                "name": name.lexeme,
                "is_array": is_array
            })

            if not self.match(TokenType.COMMA):
                break

        return parameters

    # -----------------------------
    # VARIABLE DECLARATIONS
    # -----------------------------

    def parse_declarator(self, name):
        is_array = False

        if self.match(TokenType.LBRACKET):
            is_array = True

            if not self.check(TokenType.RBRACKET):
                self.parse_expression()

            self.expect(
                TokenType.RBRACKET,
                "Expected ']'"
            )

        initializer = None

        if self.match(TokenType.ASSIGN):
            initializer = self.parse_expression()

        return {
            "type": "Variable",
            "name": name.lexeme,
            "is_array": is_array,
            "initializer": initializer
        }

    def parse_variable_declaration(self):
        data_type = self.parse_type()

        declarations = []

        name = self.expect(TokenType.IDENTIFIER)

        declarations.append(
            self.parse_declarator(name)
        )

        while self.match(TokenType.COMMA):
            name = self.expect(TokenType.IDENTIFIER)

            declarations.append(
                self.parse_declarator(name)
            )

        self.expect(
            TokenType.SEMICOLON,
            "Expected ';' after variable declaration"
        )

        return {
            "type": "Declaration",
            "data_type": data_type,
            "declarations": declarations
        }

    # -----------------------------
    # BLOCK
    # -----------------------------

    def parse_block(self):
        self.expect(
            TokenType.LBRACE,
            "Expected '{'"
        )

        statements = []

        while (
            not self.check(TokenType.RBRACE)
            and not self.check(TokenType.EOF)
        ):
            statements.append(
                self.parse_statement()
            )

        self.expect(
            TokenType.RBRACE,
            "Expected '}'"
        )

        return {
            "type": "Block",
            "statements": statements
        }

    # -----------------------------
    # STATEMENTS
    # -----------------------------

    def parse_statement(self):
        if self.check(TokenType.LBRACE):
            return self.parse_block()

        if self.check(TokenType.KEYWORD):

            keyword = self.current().lexeme

            if keyword in ("int", "char", "void"):
                return self.parse_variable_declaration()

            if keyword == "if":
                return self.parse_if()

            if keyword == "while":
                return self.parse_while()

            if keyword == "for":
                return self.parse_for()

            if keyword == "return":
                return self.parse_return()

            if keyword == "break":
                self.advance()
                self.expect(TokenType.SEMICOLON)
                return {"type": "Break"}

            if keyword == "continue":
                self.advance()
                self.expect(TokenType.SEMICOLON)
                return {"type": "Continue"}

        if self.match(TokenType.SEMICOLON):
            return {"type": "EmptyStatement"}

        expression = self.parse_expression()

        self.expect(
            TokenType.SEMICOLON,
            "Expected ';' after expression"
        )

        return {
            "type": "ExpressionStatement",
            "expression": expression
        }

    # -----------------------------
    # IF / ELSE
    # -----------------------------

    def parse_if(self):
        self.expect_keyword("if")
        self.expect(TokenType.LPAREN)

        condition = self.parse_expression()

        self.expect(TokenType.RPAREN)

        then_branch = self.parse_statement()

        else_branch = None

        if (
            self.check(TokenType.KEYWORD)
            and self.current().lexeme == "else"
        ):
            self.advance()
            else_branch = self.parse_statement()

        return {
            "type": "If",
            "condition": condition,
            "then_branch": then_branch,
            "else_branch": else_branch
        }

    # -----------------------------
    # WHILE
    # -----------------------------

    def parse_while(self):
        self.expect_keyword("while")
        self.expect(TokenType.LPAREN)

        condition = self.parse_expression()

        self.expect(TokenType.RPAREN)

        body = self.parse_statement()

        return {
            "type": "While",
            "condition": condition,
            "body": body
        }

    # -----------------------------
    # FOR
    # -----------------------------

    def parse_for(self):
        self.expect_keyword("for")
        self.expect(TokenType.LPAREN)

        initializer = None
        condition = None
        update = None

        if not self.check(TokenType.SEMICOLON):
            initializer = self.parse_expression()

        self.expect(TokenType.SEMICOLON)

        if not self.check(TokenType.SEMICOLON):
            condition = self.parse_expression()

        self.expect(TokenType.SEMICOLON)

        if not self.check(TokenType.RPAREN):
            update = self.parse_expression()

        self.expect(TokenType.RPAREN)

        body = self.parse_statement()

        return {
            "type": "For",
            "initializer": initializer,
            "condition": condition,
            "update": update,
            "body": body
        }

    # -----------------------------
    # RETURN
    # -----------------------------

    def parse_return(self):
        self.expect_keyword("return")

        value = None

        if not self.check(TokenType.SEMICOLON):
            value = self.parse_expression()

        self.expect(TokenType.SEMICOLON)

        return {
            "type": "Return",
            "value": value
        }

    # -----------------------------
    # EXPRESSIONS
    # -----------------------------

    def parse_expression(self):
        return self.parse_assignment()

    def parse_assignment(self):
        left = self.parse_logical_or()

        if self.match(TokenType.ASSIGN):
            right = self.parse_assignment()

            return {
                "type": "Assignment",
                "operator": "=",
                "left": left,
                "right": right
            }

        return left

    def parse_logical_or(self):
        node = self.parse_logical_and()

        while self.match(TokenType.OR):
            right = self.parse_logical_and()

            node = {
                "type": "Binary",
                "operator": "||",
                "left": node,
                "right": right
            }

        return node

    def parse_logical_and(self):
        node = self.parse_equality()

        while self.match(TokenType.AND):
            right = self.parse_equality()

            node = {
                "type": "Binary",
                "operator": "&&",
                "left": node,
                "right": right
            }

        return node

    def parse_equality(self):
        node = self.parse_relational()

        while (
            self.check(TokenType.EQUAL)
            or self.check(TokenType.NOT_EQUAL)
        ):
            operator = self.advance().lexeme
            right = self.parse_relational()

            node = {
                "type": "Binary",
                "operator": operator,
                "left": node,
                "right": right
            }

        return node

    def parse_relational(self):
        node = self.parse_additive()

        while (
            self.check(TokenType.LESS)
            or self.check(TokenType.LESS_EQUAL)
            or self.check(TokenType.GREATER)
            or self.check(TokenType.GREATER_EQUAL)
        ):
            operator = self.advance().lexeme
            right = self.parse_additive()

            node = {
                "type": "Binary",
                "operator": operator,
                "left": node,
                "right": right
            }

        return node

    def parse_additive(self):
        node = self.parse_multiplicative()

        while (
            self.check(TokenType.PLUS)
            or self.check(TokenType.MINUS)
        ):
            operator = self.advance().lexeme
            right = self.parse_multiplicative()

            node = {
                "type": "Binary",
                "operator": operator,
                "left": node,
                "right": right
            }

        return node

    def parse_multiplicative(self):
        node = self.parse_unary()

        while (
            self.check(TokenType.MULTIPLY)
            or self.check(TokenType.DIVIDE)
            or self.check(TokenType.MODULO)
        ):
            operator = self.advance().lexeme
            right = self.parse_unary()

            node = {
                "type": "Binary",
                "operator": operator,
                "left": node,
                "right": right
            }

        return node

    # -----------------------------
    # UNARY
    # -----------------------------

    def parse_unary(self):
        if (
            self.check(TokenType.NOT)
            or self.check(TokenType.MINUS)
            or self.check(TokenType.PLUS)
        ):
            operator = self.advance().lexeme

            operand = self.parse_unary()

            return {
                "type": "Unary",
                "operator": operator,
                "operand": operand
            }

        return self.parse_postfix()

    # -----------------------------
    # FUNCTION CALLS / ARRAYS
    # -----------------------------

    def parse_postfix(self):
        node = self.parse_primary()

        while True:

            if self.match(TokenType.LPAREN):
                arguments = []

                if not self.check(TokenType.RPAREN):
                    arguments.append(self.parse_expression())

                    while self.match(TokenType.COMMA):
                        arguments.append(
                            self.parse_expression()
                        )

                self.expect(TokenType.RPAREN)

                node = {
                    "type": "FunctionCall",
                    "function": node,
                    "arguments": arguments
                }

            elif self.match(TokenType.LBRACKET):
                index = self.parse_expression()

                self.expect(TokenType.RBRACKET)

                node = {
                    "type": "ArrayAccess",
                    "array": node,
                    "index": index
                }

            else:
                break

        return node

    # -----------------------------
    # PRIMARY EXPRESSIONS
    # -----------------------------

    def parse_primary(self):
        token = self.current()

        if token.type == TokenType.INTEGER:
            self.advance()

            return {
                "type": "Integer",
                "value": int(token.lexeme)
            }

        if token.type == TokenType.STRING:
            self.advance()

            return {
                "type": "String",
                "value": token.lexeme
            }

        if token.type == TokenType.CHAR:
            self.advance()

            return {
                "type": "Character",
                "value": token.lexeme
            }

        if token.type == TokenType.IDENTIFIER:
            self.advance()

            return {
                "type": "Identifier",
                "name": token.lexeme
            }

        if self.match(TokenType.LPAREN):
            expression = self.parse_expression()

            self.expect(
                TokenType.RPAREN,
                "Expected ')'"
            )

            return expression

        self.error("Expected an expression")


def parse(tokens):
    parser = Parser(tokens)
    return parser.parse()
