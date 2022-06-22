module llvm.functions.load;

version (LLVM_Load):

import std.traits;
import std.meta;

import link = llvm.functions.link;

private:

version(Posix)
{
    import core.sys.posix.dlfcn;

    alias SharedLibHandle = void*;

    version(OSX)
    {
        enum libPrefix = "lib";
        enum libSuffix = "dylib";
    }
    else
    {
        enum libPrefix = "lib";
        enum libSuffix = "so";
    }

    pragma(lib, "dl");
}
else version(Windows)
{
    import core.sys.windows.windows;
    import std.path : dirName;

    alias SharedLibHandle = HMODULE;

    enum libPrefix = "";
    enum libSuffix = "dll";
} else {
    static assert(0, "Unsupported operating system");
}

struct SharedLib
{
    import std.string : fromStringz, toStringz;
private:
    SharedLibHandle handle;
public:
    @disable this(this);

    this(string filename)
    {
        version(Posix)
        {
            if((handle = dlopen(filename.toStringz(), RTLD_NOW)) is null)
            {
                throw new SharedLibException("Failed to load library " ~ filename ~ ": " ~ dlerror().fromStringz().idup);
            }
        }
        else version(Windows)
        {
            if((handle = LoadLibraryA(filename.toStringz())) is null)
            {
                LPCSTR error;
                DWORD tchar_length = FormatMessageA(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    null,
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    cast(char*) &error,
                    0,
                    null);
                scope(exit) LocalFree(cast(HLOCAL) error);

                throw new SharedLibException("Failed to load library " ~ filename ~ ": " ~ error[0..tchar_length].idup);
            }
        }
        else static assert(0, "Unsupported operating system");
    }

    ~this()
    {
             version (Posix)   alias close = dlclose;
        else version (Windows) alias close = FreeLibrary;
        else static assert(0, "Unsupported operating system");

        if (handle) {
            close(handle);
            handle = null;
        }
    }

    T getSymbol(T)(string symbol)
    {
             version (Posix)   alias get = dlsym;
        else version (Windows) alias get = GetProcAddress;
        else static assert(0, "Unsupported operating system");

        return cast(T) get(handle, symbol.toStringz());
    }
}

public:

final class SharedLibException : Exception
{
    this(string msg) @safe pure nothrow
    {
        super(msg);
    }
}

private
{
    bool isSym(string m) { return m != "object" && m != "llvm" && m != "core" && m != "orEmpty"; }

    string declareSymPtr(string m) { return "typeof(link." ~ m ~ ")* " ~ m ~ ";"; }
    string getSymPtr(string m)
    {
        string code;
        code ~= "foreach (library; libraries) { ";
        code ~= m ~ " = library.getSymbol!(typeof(" ~ m ~ "))(\"" ~ m ~ "\");";
        code ~= "if (" ~ m ~ " !is null) break;";
        code ~= " }";
        return code;
    }
}

__gshared
{
    mixin ({
        string code;
        foreach (m; __traits(allMembers, link)) if (m.isSym) {
            code ~= m.declareSymPtr;
        }
        return code;
    }());
}

/// Container for holding the LLVM library and the load/unload functions.
public struct LLVM
{
    import llvm.config : LLVM_VersionString;
private:
    import std.typecons : RefCounted;

    __gshared static RefCounted!SharedLib[] libraries;

    static void getSymbols()
    {
        foreach (m; __traits(allMembers, link)) static if (m.isSym) {
            mixin (m.getSymPtr);
            debug {
                import std.stdio : stderr;
                if (!mixin(m)) stderr.writeln("Missing LLVM symbol: " ~ m);
            }
        }
    }
public:
    /// true iff the LLVM library is loaded
    static bool loaded() @property { return libraries.length > 0; }

    /// Loads the LLVM libraries using the specified filenames
    static void load(string[] filenames...)
    {
        if (filenames.length == 0) {
            filenames ~= libPrefix ~ "LLVM" ~ "." ~ libSuffix;
            filenames ~= libPrefix ~ "LTO" ~ "." ~ libSuffix;
        }

        if (loaded) unload();

        foreach (filename; filenames) {
            libraries ~= RefCounted!SharedLib(filename);
        }
        getSymbols();
    }

    /// Unloads the LLVM library
    static void unload()
    {
        libraries.length = 0;
    }
}

version (LLVM_Autoload)
{
    shared static this()
    {
        LLVM.load();
    }

    shared static ~this()
    {
        LLVM.unload();
    }
}
