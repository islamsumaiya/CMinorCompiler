def add(a, b):
    pc = 0
    pending_params = []
    while True:
        if pc >= 2:
            return None
        if pc == 0:
            t1 = a + b
            pc = 1
            continue
        elif pc == 1:
            return t1
            continue
        else: raise RuntimeError("Invalid program counter")

def main():
    pc = 0
    pending_params = []
    while True:
        if pc >= 5:
            return None
        if pc == 0:
            pending_params.append(10)
            pc = 1
            continue
        elif pc == 1:
            pending_params.append(20)
            pc = 2
            continue
        elif pc == 2:
            t2 = add(*pending_params[-2:])
            del pending_params[-2:]
            pc = 3
            continue
        elif pc == 3:
            result = t2
            pc = 4
            continue
        elif pc == 4:
            return result
            continue
        else: raise RuntimeError("Invalid program counter")
