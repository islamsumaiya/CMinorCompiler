
from .tokens import Token, TokenType


KEYWORDS = {
    "int", "void", "if", "else", "while",
    "for", "break", "continue", "return", "char"
}


OPERATORS = {
    "+": TokenType.PLUS,
    "-": TokenType.MINUS,
    "*": TokenType.MULTIPLY,
    "/": TokenType.DIVIDE,
    "%": TokenType.MODULO,
    "<": TokenType.LESS,
    "<=": TokenType.LESS_EQUAL,
    ">": TokenType.GREATER,
    ">=": TokenType.GREATER_EQUAL,
    "==": TokenType.EQUAL,
    "!=": TokenType.NOT_EQUAL,
    "!": TokenType.NOT,
    "&&": TokenType.AND,
    "||": TokenType.OR,
    "=": TokenType.ASSIGN,
}


SEPARATORS = {
    "(": TokenType.LPAREN,
    ")": TokenType.RPAREN,
    "{": TokenType.LBRACE,
    "}": TokenType.RBRACE,
    ";": TokenType.SEMICOLON,
    ",": TokenType.COMMA,
    "[": TokenType.LBRACKET,
    "]": TokenType.RBRACKET,
}


UNSUPPORTED_OPERATORS = (
    "++", "--", "+=", "-=", "*=", "/=", "%=",
    "<<", ">>", "&=", "|=", "^=", "->",
    "?", "&", "|", "^", "~"
)


VALID_ESCAPES = {
    "n", "t", "r", "0", "\\", '"', "'"
}


class LexerError(Exception):
    pass


class Lexer:

    def __init__(self, source, file_path="<input>"):
        self.source = source
        self.file_path = file_path
        self.position = 0
        self.line = 1
        self.column = 1
        self.line_has_code = False
        self.tokens = []

    def current(self):
        if self.position >= len(self.source):
            return "\0"
        return self.source[self.position]

    def peek(self, offset=1):
        pos = self.position + offset

        if pos >= len(self.source):
            return "\0"

        return self.source[pos]

    def advance(self):
        char = self.current()

        if char == "\r":
            self.position += 1

            if self.current() == "\n":
                self.position += 1

            self.line += 1
            self.column = 1
            self.line_has_code = False

        elif char == "\n":
            self.position += 1
            self.line += 1
            self.column = 1
            self.line_has_code = False

        else:
            self.position += 1
            self.column += 1

        return char

    def error(self, message, line=None, column=None):
        line = self.line if line is None else line
        column = self.column if column is None else column

        raise LexerError(
            f"{self.file_path}:{line}:{column}: {message}"
        )

    def add_token(self, token_type, lexeme, line, column):
        self.tokens.append(
            Token(
                token_type,
                lexeme,
                self.file_path,
                line,
                column
            )
        )

        self.line_has_code = True

    def scan_identifier(self):
        start = self.position
        line = self.line
        column = self.column

        while (
            self.current().isascii()
            and (
                self.current().isalnum()
                or self.current() == "_"
            )
        ):
            self.advance()

        lexeme = self.source[start:self.position]

        if lexeme in KEYWORDS:
            token_type = TokenType.KEYWORD
        else:
            token_type = TokenType.IDENTIFIER

        self.add_token(token_type, lexeme, line, column)

    def scan_number(self):
        start = self.position
        line = self.line
        column = self.column

        while (
            self.current().isascii()
            and (
                self.current().isalnum()
                or self.current() in "._"
            )
        ):
            self.advance()

        lexeme = self.source[start:self.position]

        if not lexeme.isdigit():
            self.error(
                f"Malformed or unsupported integer literal '{lexeme}'",
                line,
                column
            )

        if len(lexeme) > 1 and lexeme.startswith("0"):
            self.error(
                f"Leading-zero integer literal is unsupported: '{lexeme}'",
                line,
                column
            )

        if int(lexeme) > 2147483647:
            self.error(
                f"Integer literal exceeds 2147483647: '{lexeme}'",
                line,
                column
            )

        self.add_token(TokenType.INTEGER, lexeme, line, column)

    def scan_string(self):
        line = self.line
        column = self.column
        start = self.position

        self.advance()  # Opening double quote

        while self.current() != "\0":
            char = self.current()

            if char == '"':
                self.advance()

                lexeme = self.source[start:self.position]

                self.add_token(
                    TokenType.STRING,
                    lexeme,
                    line,
                    column
                )
                return

            if char in ("\n", "\r"):
                self.error(
                    "Unterminated string literal",
                    line,
                    column
                )

            if char == "\\":
                self.advance()

                if self.current() not in VALID_ESCAPES:
                    self.error(
                        "Invalid escape sequence",
                        line,
                        column
                    )

                self.advance()
                continue

            self.advance()

        self.error(
            "Unterminated string literal",
            line,
            column
        )

    def scan_char(self):
        line = self.line
        column = self.column
        start = self.position

        self.advance()  # Opening single quote

        if self.current() in ("\0", "\n", "\r", "'"):
            self.error(
                "Invalid character literal",
                line,
                column
            )

        if self.current() == "\\":
            self.advance()

            if self.current() not in VALID_ESCAPES:
                self.error(
                    "Invalid escape sequence",
                    line,
                    column
                )

            self.advance()

        else:
            if not self.current().isascii():
                self.error(
                    "Unsupported non-ASCII character literal",
                    line,
                    column
                )

            self.advance()

        if self.current() != "'":
            self.error(
                "Character literal must contain exactly one character",
                line,
                column
            )

        self.advance()

        lexeme = self.source[start:self.position]

        self.add_token(
            TokenType.CHAR,
            lexeme,
            line,
            column
        )

    def skip_line_comment(self):
        while self.current() not in ("\0", "\n", "\r"):
            self.advance()

    def skip_block_comment(self):
        start_line = self.line
        start_column = self.column

        self.advance()
        self.advance()

        while self.current() != "\0":

            if self.current() == "*" and self.peek() == "/":
                self.advance()
                self.advance()
                return

            self.advance()

        self.error(
            "Unterminated block comment",
            start_line,
            start_column
        )

    def scan_operator(self):
        line = self.line
        column = self.column

        for operator in sorted(
            UNSUPPORTED_OPERATORS,
            key=len,
            reverse=True
        ):
            if self.source.startswith(operator, self.position):
                self.error(
                    f"Unsupported operator '{operator}'",
                    line,
                    column
                )

        for length in (2, 1):
            operator = self.source[
                self.position:self.position + length
            ]

            if operator in OPERATORS:
                for _ in range(length):
                    self.advance()

                self.add_token(
                    OPERATORS[operator],
                    operator,
                    line,
                    column
                )

                return True

        return False

    def tokenize(self):

        while self.current() != "\0":

            char = self.current()

            # Whitespace
            if char in (" ", "\t", "\v", "\f", "\r", "\n"):
                self.advance()
                continue

            # Single-line comment
            if char == "/" and self.peek() == "/":
                self.skip_line_comment()
                continue

            # Multiline comment
            if char == "/" and self.peek() == "*":
                self.skip_block_comment()
                continue

            line = self.line
            column = self.column

            # Unsupported line splicing
            if char == "\\" and self.peek() in ("\n", "\r"):
                self.error(
                    "UNSUPPORTED_LINE_SPLICING",
                    line,
                    column
                )

            # Preprocessing directives
            if char == "#" or (
                char == "%" and self.peek() == ":"
            ):
                if not self.line_has_code:
                    self.error(
                        "UNSUPPORTED_PREPROCESSING_DIRECTIVE",
                        line,
                        column
                    )

                self.error(
                    f"Invalid character '{char}'",
                    line,
                    column
                )

            # Non-ASCII characters
            if not char.isascii():
                self.error(
                    f"Unsupported non-ASCII character '{char}'",
                    line,
                    column
                )

            # String literal
            if char == '"':
                self.scan_string()
                continue

            # Character literal
            if char == "'":
                self.scan_char()
                continue

            # Identifier or keyword
            if char.isalpha() or char == "_":
                self.scan_identifier()
                continue

            # Integer
            if char.isdigit():
                self.scan_number()
                continue

            # Separators
            if char in SEPARATORS:
                self.advance()

                self.add_token(
                    SEPARATORS[char],
                    char,
                    line,
                    column
                )

                continue

            # Operators
            if self.scan_operator():
                continue

            self.error(
                f"Unknown character '{char}'",
                line,
                column
            )

        self.tokens.append(
            Token(
                TokenType.EOF,
                "",
                self.file_path,
                self.line,
                self.column
            )
        )

        return self.tokens


def tokenize(source, file_path="<input>"):
    lexer = Lexer(source, file_path)
    return lexer.tokenize()
