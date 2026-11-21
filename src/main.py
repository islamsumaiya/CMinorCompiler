
import sys
from pathlib import Path

from src.lexer.lexer import tokenize
from src.parser.parser import parse
from src.semantic.semantic import analyze
from src.ir.ir_generator import generate_ir
from src.backend.code_generator import generate_code


def main():
    print("====================================")
    print("          C MINOR COMPILER")
    print("====================================")

    if len(sys.argv) < 2:
        print("Usage: python -m src.main <source_file.c>")
        return

    source_path = Path(sys.argv[1])

    if not source_path.is_file():
        print(f"Error: File not found: {source_path}")
        return

    try:
        source = source_path.read_text(encoding="utf-8")

        # Phase 1: Lexical Analysis
        print("\n[1] Lexical Analysis...")
        tokens = tokenize(source, str(source_path))
        print("Lexer: SUCCESS")

        # Phase 2: Syntax Analysis
        print("\n[2] Syntax Analysis...")
        ast = parse(tokens)
        print("Parser: SUCCESS")

        # Phase 3: Semantic Analysis
        print("\n[3] Semantic Analysis...")
        analyze(ast)
        print("Semantic Analysis: SUCCESS")

        # Phase 4: Intermediate Code Generation
        print("\n[4] IR Generation...")
        ir = generate_ir(ast)
        print("IR Generation: SUCCESS")

        # Phase 5: Backend Code Generation
        print("\n[5] Backend Code Generation...")
        target_code = generate_code(ir)
        print("Code Generation: SUCCESS")

        output_path = Path("output.py")
        output_path.write_text(target_code, encoding="utf-8")

        print("\n====================================")
        print("       COMPILATION SUCCESSFUL")
        print("====================================")
        print(f"Generated code saved to: {output_path}")

    except Exception as error:
        print("\nCOMPILATION FAILED")
        print(f"Error: {error}")


if __name__ == "__main__":
    main()
