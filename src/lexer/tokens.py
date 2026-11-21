
from enum import Enum, auto


class TokenType(Enum):

    # Keywords and identifiers
    KEYWORD = auto()
    IDENTIFIER = auto()

    # Literals
    INTEGER = auto()
    STRING = auto()
    CHAR = auto()

    # Arithmetic operators
    PLUS = auto()
    MINUS = auto()
    MULTIPLY = auto()
    DIVIDE = auto()
    MODULO = auto()

    # Relational operators
    LESS = auto()
    LESS_EQUAL = auto()
    GREATER = auto()
    GREATER_EQUAL = auto()
    EQUAL = auto()
    NOT_EQUAL = auto()

    # Logical operators
    NOT = auto()
    AND = auto()
    OR = auto()

    # Assignment operator
    ASSIGN = auto()

    # Parentheses
    LPAREN = auto()
    RPAREN = auto()

    # Braces
    LBRACE = auto()
    RBRACE = auto()

    # Square brackets
    LBRACKET = auto()
    RBRACKET = auto()

    # Other separators
    SEMICOLON = auto()
    COMMA = auto()

    # End of file
    EOF = auto()


class Token:

    def __init__(
        self,
        token_type,
        lexeme,
        file_path="<input>",
        line=1,
        column=1
    ):
        self.type = token_type
        self.lexeme = lexeme
        self.file_path = file_path
        self.line = line
        self.column = column

    def __repr__(self):
        return (
            f"{self.type.name:<15} "
            f"{self.lexeme:<12} "
            f"{self.line}:{self.column}"
        )
