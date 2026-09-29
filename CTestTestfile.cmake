# CMake generated Testfile for 
# Source directory: /Users/kshitijdubey/mlir-analysis-pass
# Build directory: /Users/kshitijdubey/mlir-analysis-pass
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("zero-analysis" "/opt/homebrew/bin/cmake" "-DMLIR_OPT=/opt/homebrew/Cellar/llvm/23.1.1/bin/mlir-opt" "-DPLUGIN=/Users/kshitijdubey/mlir-analysis-pass/ZeroAnalysis.dylib" "-DINPUT=/Users/kshitijdubey/mlir-analysis-pass/test/zero.mlir" "-DCHECKS=/Users/kshitijdubey/mlir-analysis-pass/test/zero.expected" "-P" "/Users/kshitijdubey/mlir-analysis-pass/cmake/RunTest.cmake")
set_tests_properties("zero-analysis" PROPERTIES  _BACKTRACE_TRIPLES "/Users/kshitijdubey/mlir-analysis-pass/CMakeLists.txt;136;add_test;/Users/kshitijdubey/mlir-analysis-pass/CMakeLists.txt;0;")
