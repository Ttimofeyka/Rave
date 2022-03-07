from typing import TextIO

try:
    from pycparser import c_ast, parse_file
except ModuleNotFoundError:
    print("The pyparser module is required for this script to work.")


def type_to_string(t: c_ast.Node):
    if isinstance(t, c_ast.PtrDecl):
        return type_to_string(t.type) + "*"
    elif isinstance(t, c_ast.TypeDecl):
        return ("const " if "const" in t.quals else "") + type_to_string(t.type)
    elif isinstance(t, c_ast.IdentifierType):
        return t.names[0]
    else:
        raise NotImplementedError(t.__class__.__name__)


class FuncDeclVisitor(c_ast.NodeVisitor):
    def __init__(self, out: TextIO):
        self.out = out

    def visit_FuncDecl(self, node: c_ast.FuncDecl):
        nnode = node
        while isinstance(nnode.type, c_ast.PtrDecl):
            nnode = nnode.type
        
        self.out.write(f"GLAPI {nnode.type.declname}(")
        if node.args is not None:
            for i, param in enumerate(node.args.params):
                self.out.write(param.name + ": " + type_to_string(param.type))
                if i < len(node.args.params) - 1:
                    self.out.write(", ")
        self.out.write("): ")
        self.out.write(type_to_string(node.type) + "\n")

        print('%s at %s' % (nnode.type.declname, node.coord))


def show_func_decls(filename):
    ast = parse_file(filename)

    with open("opengl.epl", "w") as f:
        v = FuncDeclVisitor(f)
        v.visit(ast)


if __name__ == "__main__":
    show_func_decls("opengl_api.h")
