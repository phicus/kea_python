#include <boost/algorithm/string/predicate.hpp>
#include <hooks/hooks.h>
#include <log/message_initializer.h>
#include <log/macros.h>

#include "keapy_messages.h"

using namespace std;
using namespace isc::hooks;
using namespace isc::data;
using namespace isc::log;

extern "C" {
#include <stdlib.h>
#include <dlfcn.h>
#include <Python.h>

static isc::log::Logger logger("keapy");

static void *libpython;
static PyObject *kea_module;

static int load_libpython(const char *name) {
    libpython = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    if (!libpython) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg(string("dlopen ").append(name).append(" failed"));
        return (1);
    }
    return (0);
}

static int unload_libpython() {
    if (libpython) {
        dlclose(libpython);
        libpython = 0;
    }
    return (0);
}

void      (*dl_Py_Initialize)(void);
void      (*dl_Py_Finalize)(void);
PyObject* (*dl_PyUnicode_DecodeFSDefault)(const char *s);
PyObject* (*dl_PyImport_Import)(PyObject *name);
void      (*dl_Py_IncRef)(PyObject *);
void      (*dl_Py_DecRef)(PyObject *);
PyObject* (*dl_PyObject_GetAttrString)(PyObject *, const char *);
int       (*dl_PyList_Insert)(PyObject *, Py_ssize_t, PyObject *);

static int find_symbol(void **sym, const char *name) {
    *sym = dlsym(libpython, name);
    if (*sym == 0) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg(string("symbol ").append(name).append(" not found"));
    }
    return *sym == 0;
}

#define load_symbol(name) find_symbol((void **)&dl_ ## name, #name)

static int find_symbols() {
    if (load_symbol(Py_Initialize)
        || load_symbol(Py_Finalize)
        || load_symbol(PyUnicode_DecodeFSDefault)
        || load_symbol(PyImport_Import)
        || load_symbol(Py_IncRef)
        || load_symbol(Py_DecRef)
        || load_symbol(PyObject_GetAttrString)
        || load_symbol(PyList_Insert)) {
        return (1);
    }
    return (0);
}

static int python_initialize() {
    dl_Py_Initialize();
    return (0);
}

static int python_finalize() {
    if (dl_Py_Finalize) {
        dl_Py_Finalize();
    }
    return (0);
}

static PyObject* make_python_string(const char *str) {
    PyObject *obj = dl_PyUnicode_DecodeFSDefault(str);
    if (!obj) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg(string("PyUnicode_DecodeFSDefault(\"").append(str).append("\") failed"));
    }
    return obj;
}

static int split_module_path(const string module, string &module_path, string &module_name) {
    // extract the path portion of the module parameter
    size_t slash_pos = module.find_last_of('/');
    size_t basename_pos = 0;
    if (slash_pos != string::npos) {
        module_path = module.substr(0, slash_pos);
        basename_pos = slash_pos + 1;
    }
    // extract module basename by removing .py if it is present
    if (boost::algorithm::ends_with(module, ".py")) {
        module_name = module.substr(basename_pos, module.size() - basename_pos - 3);
    }
    else {
        module_name = module.substr(basename_pos, module.size() - basename_pos);
    }
    return (0);
}

static int insert_python_path(const string module_path) {
    int res = 1;
    PyObject *sys = 0;
    PyObject *sys_module = 0;
    PyObject *path = 0;
    PyObject *cwd = 0;

    LOG_INFO(logger, LOG_KEAPY_HOOK).arg("insert \"" + module_path + "\" into sys.path");
    sys = make_python_string("sys");
    if (!sys) {
        goto error;
    }
    sys_module = dl_PyImport_Import(sys);
    if (!sys_module) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("PyImport_Import(\"sys\") failed");
        goto error;
    }
    path = dl_PyObject_GetAttrString(sys_module, "path");
    if (!path) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("PyObject_GetAttrString(sys_module, \"path\") failed");
        goto error;
    }
    cwd = dl_PyUnicode_DecodeFSDefault(module_path.c_str());
    if (dl_PyList_Insert(path, 0, cwd) < 0) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("PyList_Insert(path, 0, cwd) failed");
        goto error;
    }
    res = 0;

error:
    dl_Py_DecRef(cwd);
    dl_Py_DecRef(path);
    dl_Py_DecRef(sys_module);
    dl_Py_DecRef(sys);
    return (res);
}

static int import_python_module(string module_name) {
    PyObject *name = 0;

    LOG_INFO(logger, LOG_KEAPY_HOOK).arg("import \"" + module_name + "\" module");
    name = make_python_string(module_name.c_str());
    if (!name) {
        return (1);
    }
    kea_module = dl_PyImport_Import(name);
    dl_Py_DecRef(name);
    if (!kea_module) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("PyImport_Import(\"" + module_name + "\") failed");
        return (1);
    }
    // if (import_kea() < 0) {
    //     return (1);
    // }
    return (0);
}

static int unload_kea_module() {
    if (kea_module) {
        dl_Py_DecRef(kea_module);
        kea_module = 0;
    }
    return (0);
}

int load(LibraryHandle &handle) {
    ConstElementPtr libpython  = handle.getParameter("libpython");
    ConstElementPtr module  = handle.getParameter("module");

    // check libpython parameter
    if (!libpython) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("missing \"libpython\" parameter");
        return (1);
    }
    if (libpython->getType() != Element::string) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("\"libpython\" parameter must be a string");
        return (1);
    }
    // check module parameter
    if (!module) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("missing \"module\" parameter");
        return (1);
    }
    if (module->getType() != Element::string) {
        LOG_ERROR(logger, LOG_KEAPY_HOOK).arg("\"module\" parameter must be a string");
        return (1);
    }
    // load libpython
    if (load_libpython(libpython->stringValue().c_str())) {
        return (1);
    }
    // find symbols needed to load kea extension module
    if (find_symbols()) {
        return (1);
    }
    python_initialize();
    // split module into path and module
    string module_path;
    string module_name;
    split_module_path(module->stringValue(), module_path, module_name);
    if (!module_path.empty() && insert_python_path(module_path)) {
        return (1);
    }
    if (import_python_module(module_name)) {
        return (1);
    }
    LOG_INFO(logger, LOG_KEAPY_HOOK).arg("keapyhook loaded");

    return (0);
}

int unload() {
    unload_kea_module();
    python_finalize();
    unload_libpython();
    LOG_INFO(logger, LOG_KEAPY_HOOK).arg("keapyhook unloaded");
    return (0);
}

}
