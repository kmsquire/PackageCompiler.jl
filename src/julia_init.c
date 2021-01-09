#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifdef _MSC_VER
JL_DLLEXPORT char *dirname(char *);
#else
#include <libgen.h>
#endif

// Julia headers (for initialization and gc commands)
#include "uv.h"
#include "julia.h"
#include "julia_init.h"


void setup_args(int argc, char **argv)
{
    uv_setup_args(argc, argv);
    libsupport_init();
    jl_parse_opts(&argc, &argv);
}

const char *get_sysimage_path(const char *libname)
{
    if (libname == NULL)
    {
        jl_error("Please specify `libname` when requesting the sysimage path");
        exit(1);
    }

    void *handle;
    const char *libpath;

    handle = jl_load_dynamic_library(libname, JL_RTLD_DEFAULT, 0);
    libpath = jl_pathname_for_handle(handle);

    return libpath;
}

void set_depot_path(char *sysimage_path)
{
    char buf[PATH_MAX + 20];
    snprintf(buf, sizeof(buf), "JULIA_DEPOT_PATH=%s/", dirname(dirname(sysimage_path)));
    putenv(buf);
    putenv("JULIA_LOAD_PATH=@");
}

void init_julia(int argc, char **argv)
{
    setup_args(argc, argv);

    const char *sysimage_path;
    sysimage_path = get_sysimage_path(JULIAC_PROGRAM_LIBNAME);

    set_depot_path((char *)sysimage_path);

    jl_options.image_file = sysimage_path;
    julia_init(JL_IMAGE_JULIA_HOME);
}

void shutdown_julia(int retcode)
{
    jl_atexit_hook(retcode);
}
