#include <iostream>
#include <vector>
#include <sstream>
#include <Python.h>
#include "ccc.h"

char CCC_BASENAME[] = "ccc";

static PyObject* ccc(PyObject* self, PyObject* args) {
  // The ccc function takes a list of arguments, we first need to pull
  // the list out of the args tuple
  PyObject* argList;
  if(!PyArg_ParseTuple(args, "O", &argList)) {
    return nullptr;
  }

  auto argc = static_cast<int>(PyList_Size(argList));
  if(argc < 0) {
    return nullptr;
  }

  std::vector<const char*> argv;
  argv.reserve(argc+1);
  argv.push_back(CCC_BASENAME);
  for (int i = 0; i < argc; ++i) {
    auto item = PyList_GetItem(argList, i);
    if (!item) {
      return nullptr;
    }

    auto argument = PyUnicode_AsUTF8(item);
    if (!argument) {
      return nullptr;
    }

    argv.push_back(argument);
  }

  std::stringstream buffer;
  std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
  std::streambuf* old_cerr = std::cerr.rdbuf(buffer.rdbuf());
  int return_value = run(argc + 1, argv.data());
  std::cout.rdbuf(old_cout);
  std::cerr.rdbuf(old_cerr);

  std::string compilation_log = buffer.str();

  return Py_BuildValue("(is)", return_value, compilation_log.c_str());
}

static PyMethodDef ccscript_methods[] = {
  {"ccc", ccc, METH_VARARGS, "ccc"},
  {NULL, NULL, 0, NULL}
};

struct module_state {
  PyObject *error;
};

#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))

static int ccscript_traverse(PyObject *m, visitproc visit, void *arg) {
  Py_VISIT(GETSTATE(m)->error);
  return 0;
}

static int ccscript_clear(PyObject *m) {
  Py_CLEAR(GETSTATE(m)->error);
  return 0;
}

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "ccscript",
  NULL,
  sizeof(struct module_state),
  ccscript_methods,
  NULL,
  ccscript_traverse,
  ccscript_clear,
  NULL
};

PyMODINIT_FUNC
PyInit_ccscript(void)
{
  PyObject *module = PyModule_Create(&moduledef);
  return module;
}

