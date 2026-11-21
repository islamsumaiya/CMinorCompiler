
class SemanticError(Exception):
    pass


class SemanticAnalyzer:
    def __init__(self):
        self.scopes = [{}]
        self.functions = {}
        self.current_function = None

    # Report semantic errors
    def error(self, message):
        raise SemanticError(message)

    # Enter a new scope
    def enter_scope(self):
        self.scopes.append({})

    # Exit current scope
    def exit_scope(self):
        if len(self.scopes) > 1:
            self.scopes.pop()

    # Declare a variable
    def declare_variable(self, name, data_type):
        current_scope = self.scopes[-1]

        if name in current_scope:
            self.error(
                f"Variable '{name}' is already declared"
            )

        current_scope[name] = data_type

    # Check whether a variable exists
    def lookup_variable(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]

        self.error(
            f"Variable '{name}' is not declared"
        )

    # Main analysis method
    def analyze(self, ast):
        if ast.get("type") != "Program":
            self.error("Invalid program AST")

        # First register all functions
        for declaration in ast.get("declarations", []):
            if declaration.get("type") == "Function":
                name = declaration["name"]

                if name in self.functions:
                    self.error(
                        f"Function '{name}' is already declared"
                    )

                self.functions[name] = declaration

        # Then analyze function bodies
        for declaration in ast.get("declarations", []):
            if declaration.get("type") == "Function":
                self.analyze_function(declaration)

        return True

    # Analyze a function
    def analyze_function(self, function):
        self.current_function = function

        self.enter_scope()

        # Declare function parameters
        for parameter in function.get("parameters", []):
            name = parameter["name"]
            data_type = parameter["data_type"]

            self.declare_variable(name, data_type)

        # Analyze function body
        self.analyze_statement(function.get("body"))

        self.exit_scope()

        self.current_function = None

    # Analyze statements
    def analyze_statement(self, statement):
        if statement is None:
            return

        node_type = statement.get("type")

        # Block
        if node_type == "Block":
            self.enter_scope()

            for item in statement.get("statements", []):
                self.analyze_statement(item)

            self.exit_scope()

        # Variable declaration
        elif node_type == "Declaration":
            data_type = statement["data_type"]

            for variable in statement.get("declarations", []):
                name = variable["name"]

                self.declare_variable(name, data_type)

                initializer = variable.get("initializer")

                if initializer is not None:
                    self.analyze_expression(initializer)

        # Return
        elif node_type == "Return":
            value = statement.get("value")

            if value is not None:
                self.analyze_expression(value)

        # Expression statement
        elif node_type == "ExpressionStatement":
            expression = statement.get("expression")

            if expression is not None:
                self.analyze_expression(expression)

        # If
        elif node_type == "If":
            self.analyze_expression(statement["condition"])

            self.analyze_statement(
                statement.get("then_branch")
            )

            if statement.get("else_branch") is not None:
                self.analyze_statement(
                    statement["else_branch"]
                )

        # While
        elif node_type == "While":
            self.analyze_expression(statement["condition"])

            self.analyze_statement(
                statement.get("body")
            )

        # For
        elif node_type == "For":
            self.enter_scope()

            initializer = statement.get("initializer")

            if initializer is not None:
                if initializer.get("type") in (
                    "Declaration",
                    "ExpressionStatement"
                ):
                    self.analyze_statement(initializer)
                else:
                    self.analyze_expression(initializer)

            condition = statement.get("condition")

            if condition is not None:
                self.analyze_expression(condition)

            update = statement.get("update")

            if update is not None:
                self.analyze_expression(update)

            self.analyze_statement(statement.get("body"))

            self.exit_scope()

        # Break, Continue and Empty
        elif node_type in ("Break", "Continue", "Empty"):
            return

        else:
            self.error(
                f"Unknown statement type: {node_type}"
            )

    # Analyze expressions
    def analyze_expression(self, expression):
        if expression is None:
            return

        node_type = expression.get("type")

        # Identifier
        if node_type == "Identifier":
            self.lookup_variable(expression["name"])

        # Literal values
        elif node_type in (
            "Integer",
            "String",
            "Char",
            "Character"
        ):
            return

        # Binary expression
        elif node_type == "Binary":
            self.analyze_expression(expression["left"])
            self.analyze_expression(expression["right"])

        # Unary expression
        elif node_type == "Unary":
            self.analyze_expression(expression["operand"])

        # Assignment
        elif node_type == "Assignment":
            target = expression.get(
                "target",
                expression.get("left")
            )

            value = expression.get(
                "value",
                expression.get("right")
            )

            if target is not None:
                self.analyze_expression(target)

            if value is not None:
                self.analyze_expression(value)

        # Function calls
        elif node_type in ("Call", "FunctionCall"):

            if node_type == "FunctionCall":
                function_node = expression["function"]

                if function_node.get("type") != "Identifier":
                    self.error("Invalid function name")

                name = function_node["name"]

            else:
                name = expression["name"]

            if name not in self.functions:
                self.error(
                    f"Function '{name}' is not declared"
                )

            expected = len(
                self.functions[name].get("parameters", [])
            )

            arguments = expression.get("arguments", [])
            actual = len(arguments)

            if expected != actual:
                self.error(
                    f"Function '{name}' expects {expected} "
                    f"arguments, but got {actual}"
                )

            for argument in arguments:
                self.analyze_expression(argument)

        # Array access
        elif node_type in ("ArrayAccess", "ArrayIndex"):
            array_name = expression.get("name")

            if array_name is not None:
                self.lookup_variable(array_name)
            else:
                array_node = expression.get("array")

                if array_node is not None:
                    self.analyze_expression(array_node)

            index = expression.get(
                "index",
                expression.get("subscript")
            )

            if index is not None:
                self.analyze_expression(index)

        else:
            self.error(
                f"Unknown expression type: {node_type}"
            )


# Function called by main.py
def analyze(ast):
    analyzer = SemanticAnalyzer()
    return analyzer.analyze(ast)
