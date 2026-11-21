
class IRGenerator:
    def __init__(self):
        self.instructions = []
        self.temp_count = 0
        self.label_count = 0
        self.loop_stack = []

    # Generate a new temporary variable
    def new_temp(self):
        self.temp_count += 1
        return f"t{self.temp_count}"

    # Generate a new label
    def new_label(self):
        self.label_count += 1
        return f"L{self.label_count}"

    # Add an instruction to the IR
    def emit(self, instruction):
        self.instructions.append(instruction)

    # Start IR generation
    def generate(self, ast):
        if ast.get("type") != "Program":
            raise Exception("Invalid AST: Expected Program")

        for declaration in ast.get("declarations", []):
            if declaration.get("type") == "Function":
                self.generate_function(declaration)

        return self.instructions

    # Generate a function
    def generate_function(self, function):
        name = function["name"]

        self.emit(f"FUNCTION {name}")

        # Store function parameter names
        for parameter in function.get("parameters", []):
            self.emit(f"PARAMETER {parameter['name']}")

        self.generate_statement(function["body"])

        self.emit(f"END FUNCTION {name}")

    # Generate statements
    def generate_statement(self, statement):
        if statement is None:
            return

        kind = statement.get("type")

        # Block
        if kind == "Block":
            for item in statement.get("statements", []):
                self.generate_statement(item)

        # Variable declaration
        elif kind == "Declaration":
            for variable in statement.get("declarations", []):
                name = variable["name"]
                initializer = variable.get("initializer")

                if initializer is not None:
                    value = self.generate_expression(initializer)
                    self.emit(f"{name} = {value}")

        # Return statement
        elif kind == "Return":
            value = statement.get("value")

            if value is None:
                self.emit("RETURN")
            else:
                result = self.generate_expression(value)
                self.emit(f"RETURN {result}")

        # Expression statement
        elif kind == "ExpressionStatement":
            expression = statement.get("expression")

            if expression is not None:
                self.generate_expression(expression)

        # If statement
        elif kind == "If":
            condition = self.generate_expression(
                statement["condition"]
            )

            else_label = self.new_label()
            end_label = self.new_label()

            self.emit(f"IF_FALSE {condition} GOTO {else_label}")

            self.generate_statement(statement["then_branch"])

            if statement.get("else_branch") is not None:
                self.emit(f"GOTO {end_label}")
                self.emit(f"LABEL {else_label}")

                self.generate_statement(statement["else_branch"])

                self.emit(f"LABEL {end_label}")
            else:
                self.emit(f"LABEL {else_label}")

        # While loop
        elif kind == "While":
            start_label = self.new_label()
            end_label = self.new_label()

            self.emit(f"LABEL {start_label}")

            condition = self.generate_expression(
                statement["condition"]
            )

            self.emit(f"IF_FALSE {condition} GOTO {end_label}")

            self.loop_stack.append((start_label, end_label))

            self.generate_statement(statement["body"])

            self.loop_stack.pop()

            self.emit(f"GOTO {start_label}")
            self.emit(f"LABEL {end_label}")

        # For loop
        elif kind == "For":
            initializer = statement.get("initializer")
            condition_node = statement.get("condition")
            update = statement.get("update")

            if initializer is not None:
                if isinstance(initializer, dict):
                    if initializer.get("type") in (
                        "Declaration",
                        "ExpressionStatement"
                    ):
                        self.generate_statement(initializer)
                    else:
                        self.generate_expression(initializer)

            start_label = self.new_label()
            update_label = self.new_label()
            end_label = self.new_label()

            self.emit(f"LABEL {start_label}")

            if condition_node is not None:
                condition = self.generate_expression(condition_node)
                self.emit(
                    f"IF_FALSE {condition} GOTO {end_label}"
                )

            self.loop_stack.append((update_label, end_label))

            self.generate_statement(statement["body"])

            self.loop_stack.pop()

            self.emit(f"LABEL {update_label}")

            if update is not None:
                self.generate_expression(update)

            self.emit(f"GOTO {start_label}")
            self.emit(f"LABEL {end_label}")

        # Break
        elif kind == "Break":
            if not self.loop_stack:
                raise Exception("'break' used outside a loop")

            _, end_label = self.loop_stack[-1]
            self.emit(f"GOTO {end_label}")

        # Continue
        elif kind == "Continue":
            if not self.loop_stack:
                raise Exception("'continue' used outside a loop")

            continue_label, _ = self.loop_stack[-1]
            self.emit(f"GOTO {continue_label}")

        # Empty statement
        elif kind == "Empty":
            pass

        else:
            raise Exception(
                f"Unsupported statement: {kind}"
            )

    # Generate expressions
    def generate_expression(self, expression):
        if expression is None:
            return None

        kind = expression.get("type")

        # Integer
        if kind == "Integer":
            return str(expression["value"])

        # String
        elif kind == "String":
            return repr(expression["value"])

        # Character
        elif kind in ("Char", "Character"):
            return repr(expression["value"])

        # Identifier
        elif kind == "Identifier":
            return expression["name"]

        # Binary expression
        elif kind == "Binary":
            left = self.generate_expression(expression["left"])
            right = self.generate_expression(expression["right"])

            temp = self.new_temp()

            operator = expression["operator"]

            self.emit(
                f"{temp} = {left} {operator} {right}"
            )

            return temp

        # Unary expression
        elif kind == "Unary":
            operand = self.generate_expression(
                expression["operand"]
            )

            operator = expression["operator"]

            temp = self.new_temp()

            self.emit(f"{temp} = {operator}{operand}")

            return temp

        # Assignment
        elif kind == "Assignment":
            value = self.generate_expression(
                expression.get("value", expression.get("right"))
            )

            target = expression.get(
                "target",
                expression.get("left")
            )

            if isinstance(target, dict):
                target = self.generate_expression(target)

            operator = expression.get("operator", "=")

            if operator == "=":
                self.emit(f"{target} = {value}")
            else:
                actual_operator = operator[:-1]
                self.emit(
                    f"{target} = {target} {actual_operator} {value}"
                )

            return target

        # Function call
        elif kind in ("Call", "FunctionCall"):

            if kind == "FunctionCall":
                function_node = expression["function"]

                if function_node.get("type") != "Identifier":
                    raise Exception("Invalid function name")

                function_name = function_node["name"]

            else:
                function_name = expression["name"]

            arguments = []

            for argument in expression.get("arguments", []):
                argument_value = self.generate_expression(argument)
                arguments.append(argument_value)

            for argument in arguments:
                self.emit(f"PARAM {argument}")

            temp = self.new_temp()

            self.emit(
                f"{temp} = CALL {function_name}, {len(arguments)}"
            )

            return temp

        # Array access
        elif kind in ("ArrayAccess", "ArrayIndex"):
            array_name = expression.get("name")

            if array_name is None:
                array_node = expression.get("array")
                if isinstance(array_node, dict):
                    array_name = self.generate_expression(array_node)
                else:
                    array_name = array_node

            index_node = expression.get(
                "index",
                expression.get("subscript")
            )

            index = self.generate_expression(index_node)

            temp = self.new_temp()

            self.emit(f"{temp} = {array_name}[{index}]")

            return temp

        else:
            raise Exception(
                f"Unsupported expression: {kind}"
            )


# Public function used by main.py
def generate_ir(ast):
    generator = IRGenerator()
    return generator.generate(ast)
