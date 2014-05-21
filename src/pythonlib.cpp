#include <iostream>
#include <vector>
#include <boost/python.hpp>
#include "ccc.h"

namespace py = boost::python;

char CCC_BASENAME[] = "ccc";

py::tuple main_wrapper(py::list args) {
  int argc = py::extract<int>(args.attr("__len__")());
  std::vector<char*> argv;
  argv.reserve(argc+1);
  argv.push_back(CCC_BASENAME);
  for (int i = 0; i < argc; ++i) {
    argv.push_back(py::extract<char*>(args[i]));
  }

  std::stringstream buffer;
  std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
  std::streambuf* old_cerr = std::cerr.rdbuf(buffer.rdbuf());
  int return_value = main(argc + 1, &argv[0]);
  std::cout.rdbuf(old_cout);
  std::cerr.rdbuf(old_cerr);

  std::string compilation_log = buffer.str();

  return py::make_tuple(return_value, compilation_log);
} 

BOOST_PYTHON_MODULE(ccscript)
{
  def("ccc", main_wrapper);
}
