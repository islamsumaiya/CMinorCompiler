
import re


class CodeGenerator:
    def __init__(self):
        self.output = []

    def generate(self, instructions):
        self.output = []

        functions = []
        current_name = None
        current_parameters = []
        current_instructions = []

        # Separate all functions
        for instruction in instructions:
            parts = instruction.split()

            if not parts:
                continue

            if parts[0] == "FUNCTION":
                current_name = parts[1]
                current_parameters = []
                current_instructions = []

            elif parts[0] == "END":
                if current_name is not None:
                    functions.append(
                        (
                            current_name,
                            current_parameters,
                            current_instructions
                        )
                    )

                current_name = None

            elif current_name is not None:
                if parts[0] == "PARAMETER":
                    current_parameters.append(parts[1])
                else:
                    current_instructions.append(instruction)

        # Generate Python code for every function
        for name, parameters, body in functions:
            self.generate_function(name, parameters, body)

        return "\n".join(self.output)

    def generate_function(self, name, parameters, instructions):

        # Find labels
        labels = {}

        for index, instruction in enumerate(instructions):
            parts = instruction.split()

            if parts[0] == "LABEL":
                labels[parts[1]] = index

        # Function definition
        parameter_text = ", ".join(parameters)

        self.output.append(
            f"def {name}({parameter_text}):"
        )

        if not instructions:
            self.output.append("    return None")
            self.output.append("")
            return

        self.output.append("    pc = 0")
        self.output.append("    pending_params = []")
        self.output.append("    while True:")

        self.output.append(
            f"        if pc >= {len(instructions)}:"
        )
        self.output.append("            return None")

        # Generate instruction handling
        for index, instruction in enumerate(instructions):

            parts = instruction.split()
            prefix = "if" if index == 0 else "elif"

            self.output.append(
                f"        {prefix} pc == {index}:"
            )

            # LABEL
            if parts[0] == "LABEL":
                self.output.append(
                    f"            pc = {index + 1}"
                )

            # GOTO
            elif parts[0] == "GOTO":
                target = labels[parts[1]]

                self.output.append(
                    f"            pc = {target}"
                )

            # IF_FALSE
            elif parts[0] == "IF_FALSE":
                condition = parts[1]
                target = labels[parts[3]]

                self.output.append(
                    f"            if not ({condition}):"
                )

                self.output.append(
                    f"                pc = {target}"
                )

                self.output.append("            else:")

                self.output.append(
                    f"                pc = {index + 1}"
                )

            # RETURN
            elif parts[0] == "RETURN":
                value = instruction[len("RETURN"):].strip()

                if value:
                    self.output.append(
                        f"            return {value}"
                    )
                else:
                    self.output.append(
                        "            return None"
                    )

            # PARAM
            elif parts[0] == "PARAM":
                argument = instruction[len("PARAM"):].strip()

                self.output.append(
                    f"            pending_params.append({argument})"
                )

                self.output.append(
                    f"            pc = {index + 1}"
                )

            # FUNCTION CALL
            elif "=" in instruction and "CALL" in instruction:

                match = re.match(
                    r"^\s*(\w+)\s*=\s*CALL\s+(\w+)\s*,\s*(\d+)\s*$",
                    instruction
                )

                if not match:
                    raise ValueError(
                        f"Invalid CALL instruction: {instruction}"
                    )

                result_variable = match.group(1)
                function_name = match.group(2)
                argument_count = int(match.group(3))

                self.output.append(
                    f"            {result_variable} = "
                    f"{function_name}(*pending_params[-{argument_count}:])"
                    if argument_count > 0
                    else f"            {result_variable} = {function_name}()"
                )

                if argument_count > 0:
                    self.output.append(
                        f"            del pending_params[-{argument_count}:]"
                    )

                self.output.append(
                    f"            pc = {index + 1}"
                )

            # Normal assignment
            elif "=" in instruction:
                self.output.append(
                    f"            {instruction}"
                )

                self.output.append(
                    f"            pc = {index + 1}"
                )

            else:
                raise ValueError(
                    f"Unsupported IR instruction: {instruction}"
                )

            self.output.append("            continue")

        self.output.append(
            '        else: raise RuntimeError("Invalid program counter")'
        )

        self.output.append("")

    def generate_code(self, instructions):
        return self.generate(instructions)


def generate_code(instructions):
    generator = CodeGenerator()
    return generator.generate(instructions)
