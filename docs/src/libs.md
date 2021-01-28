# [Libraries](@id libraries)

Creating a library is similar to creating an app.  The main difference is the
lack of a main function in the result--it's assumed that the library will be
linked dynamically at runtime with a program written in C/C++ or another
language.

Because most libraries distributed are already dynamically linked and loaded,
on demand, to make things work, we distribute all of the libraries necessary to
run Julia.  In the end, we end up with a directory of libraries (`lib`, or `bin`
on Windows) and an `include` directory with C header files.

Of course, we also want the library to be relocatable, and all of the caveats
in the [App relocatability](@ref relocatability) section still apply.

## Creating a library

As with apps, the source of a library is a package with a project and manifest file.
The library is expected to provide C-callable functions for the functionality it
is providing.  These functions should be defined using the `Base.@ccallable` macro:

```julia
Base.@ccallable function increment(count::Cint)::Cint
    count += 1
    println("Incremented count: $count")
    return count
end
```

This function will be exported and made available to programs which link to the C
library.  A skeleton of a library to start working from can be found 
[here](https://github.com/JuliaLang/PackageCompiler.jl/tree/master/examples/MyLib).

Regarding relocatability, PackageCompiler provides a function
[`audit_app(app_dir::String)`](@ref) that tries to find common problems with
relocatability in the app.

The app is then compiled using the [`create_app`](@ref) function that takes a
path to the source code of the app and the destination where the app should be
compiled to. This will bundle all required libraries for the app to run on
another machine where the same Julia that created the app can run.  As an
example, in the code snippet below, the example app linked above is compiled and run:

```
~/PackageCompiler.jl/examples
❯ julia -q --project

julia> using PackageCompiler

julia> create_app("MyApp", "MyAppCompiled")
[ Info: PackageCompiler: creating base system image (incremental=false), this might take a while...
[ Info: PackageCompiler: creating system image object file, this might take a while...

julia> exit()

~/PackageCompiler.jl/examples
❯ MyAppCompiled/bin/MyApp
ARGS = ["foo", "bar"]
Base.PROGRAM_FILE = "MyAppCompiled/bin/MyApp"
...
Hello, World!

Running the artifact
The result of 2*5^2 - 10 == 40.000000
unsafe_string((Base.JLOptions()).image_file) = "/Users/kristoffer/PackageCompiler.jl/examples/MyAppCompiled/bin/MyApp.dylib"
Example.domath(5) = 10
```

The resulting executable is found in the `bin` folder in the compiled app
directory.  The compiled app directory `MyAppCompiled` could now be put into an
archive and sent to another machine or an installer could be wrapped around the
directory, perhaps providing a better user experience than just an archive of
files.

### Compilation of functions

In the same way as [files for precompilation could be given when creating
sysimages](@ref tracing), the same keyword arguments are used to add precompilation to apps.

### Incremental vs non-incremental sysimage

In the section about creating sysimages, there was a short discussion about
incremental vs non-incremental sysimages. In short, an incremental sysimage is
built on top of another sysimage, while a non-incremental is created from
scratch. For sysimages, it makes sense to use an incremental sysimage built on
top of Julia's default sysimage since we wanted the benefit of having a responsive
REPL that it provides.  For apps, this is no longer the case, the sysimage is
not meant to be used when working interactively, it only needs to be
specialized for the specific app.  Therefore, by default, `incremental=false` is
used for `create_app`. If, for some reason, one wants an incremental sysimage,
`incremental=true` could be passed to `create_app`.  With the example app, a
non-incremental sysimage is about 70MB smaller than the default sysimage.

### Filtering stdlibs

By default, all standard libraries are included in the sysimage.  It is
possible to only include those standard libraries that the project needs.  This
is done by passing the keyword argument `filter_stdlibs=true` to `create_app`.
This causes the sysimage to be smaller, and possibly load faster.  The reason
this is not the default is that it is possible to "accidentally" depend on a
standard library without it being reflected in the Project file.  For example,
it is possible to call `rand()` from a package without depending on Random,
even though that is where the method is defined. If Random was excluded from
the sysimage that call would then error. The same thing is true for e.g. matrix
multiplication, `rand(3,3) * rand(3,3)` requires both the standard libraries
`LinearAlgebra` and `Random` This is because these standard libraries practice
["type-piracy"](https://docs.julialang.org/en/v1/manual/style-guide/#Avoid-type-piracy), 
just loading those packages can cause code to change behavior.

Nevertheless, the option is there to use. Just make sure to properly test the
app with the resulting sysimage.

### Custom binary name

By default, the binary in the `bin` directory take the name of the project,
as defined in `Project.toml`.  If you want to change the name, you can pass
`app_name="some_app_name"` to `create_app`.

### Artifacts

The way to depend on external libraries or binaries when creating apps is by
using the [artifact system](https://julialang.github.io/Pkg.jl/v1/artifacts/).
PackageCompiler will bundle all artifacts needed by the project, and set up
things so that they can be found during runtime on other machines.

The example app uses the artifact system to depend on a very simple toy binary
that does some simple arithmetic. It is instructive to see how the [artifact
file](https://github.com/JuliaLang/PackageCompiler.jl/blob/master/examples/MyApp/Artifacts.toml)
is [used in the source](https://github.com/JuliaLang/PackageCompiler.jl/blob/d722a3d91abe328ebd239e2f45660be35263ebe1/examples/MyApp/src/MyApp.jl#L7-L8).

### Reverse engineering the compiled app

While the created app is relocatable and no source code is bundled with it,
there are still some things about the build machine and the source code that
can be "reverse engineered".

#### Absolute paths of build machine

Julia records the paths and line-numbers for methods when they are getting
compiled.  These get cached into the sysimage and can be found e.g. by dumping
all strings in the sysimage:

```
~/PackageCompiler.jl/examples/MyAppCompiled/bin
❯ strings MyApp.so | grep MyApp
MyApp
/home/kc/PackageCompiler.jl/examples/MyApp/
MyApp
/home/kc/PackageCompiler.jl/examples/MyApp/src/MyApp.jl
/home/kc/PackageCompiler.jl/examples/MyApp/src
MyApp.jl
/home/kc/PackageCompiler.jl/examples/MyApp/src/MyApp.jl
```

This is a problem that the Julia standard libraries themselves have:

```julia-repl
julia> @which rand()
rand() in Random at /buildworker/worker/package_linux64/build/usr/share/julia/stdlib/v1.3/Random/src/Random.jl:256
```

#### Using reflection and finding lowered code

There is nothing preventing someone from starting Julia with the sysimage that
comes with the app.  And while the source code is not available one can read
the "lowered code" and use reflection to find things like the name of fields in
structs and global variables etc:

```julia-repl
~/PackageCompiler.jl/examples/MyAppCompiled/bin kc/docs_apps*
❯ julia -q -JMyApp.so
julia> MyApp = Base.loaded_modules[Base.PkgId(Base.UUID("f943f3d7-887a-4ed5-b0c0-a1d6899aa8f5"), "MyApp")]
MyApp

julia> names(MyApp; all=true)
10-element Array{Symbol,1}:
 Symbol("#eval")
 Symbol("#include")
 Symbol("#julia_main")
 Symbol("#real_main")
 :MyApp
 :eval
 :include
 :julia_main
 :real_main
 :socrates

julia> @code_lowered MyApp.real_main()
CodeInfo(
1 ─ %1  = MyApp.ARGS
│         value@_2 = %1
│   %3  = Base.repr(%1)
│         Base.println("ARGS = ", %3)
│         value@_2
│   %6  = Base.PROGRAM_FILE
│         value@_3 = %6
│   %8  = Base.repr(%6)
│         Base.println("Base.PROGRAM_FILE = ", %8)
│         value@_3
│   %11 = MyApp.DEPOT_PATH
```
