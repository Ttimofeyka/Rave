try:
    from pycparser import c_ast, parse_file
except ModuleNotFoundError:
    print("The pyparser module is required for this script to work.")

class FuncDeclVisitor(c_ast.NodeVisitor):
    def visit_FuncDecl(self, node):
        print(node.type)
        print('%s at %s' % (node.type.declname, node.coord))

def show_func_decls(filename):
    ast = parse_file(filename)

    v = FuncDeclVisitor()
    v.visit(ast)


if __name__ == "__main__":
    show_func_decls("opengl_api.h")
