CCScript Compiler
Simple Integration Test Framework
---------------------------------

Contents:
 1. Overview
 2. Running Tests
 3. Output Files
 4. Test Case Syntax



1. Overview
===========

The 'tests' project is a simple program that can be used to test the operation of the CCScript compiler by inputting scripts and comparing the results against expected output. This is done by providing text files which list the scripts to be compiled and the output to compare against.

This should be used to build and run a suite of test cases that exercise as many of the compiler's features as <del>possible</del> <del>feasible</del> convenient.

If any new features are added to the language, additional test cases should be written against them. Of course, the original tests should also be run as regression tests.

Ideally, the CCScript compiler's code classes would also be augmented to provide unit tests of individual components, but we all know that's never going to happen.



2. Running Tests
================

To run the tests, simply execute the 'tests' program binary with two parameters: the path to the CCC binary to test, and the path to a text file containing a newline-separated list of test case files to run.

Example tests.txt:
--------------------------------------------------------------
  labels.ccs
  loops.ccs
  conditions.ccs
  modules.ccs
  moretests/references.ccs
  moretests/classes.ccs
--------------------------------------------------------------

NOTE: all test case files should be referenced by path relative to the location of the test list file.

Generally, each test case should compile only one script file, because the CCScript compiler does not guarantee the order in which modules are written -- only that the first module written will begin at the specified start address. Thus, simple byte-for-byte comparison of binary output isn't possible.

The next version of CCScript will have a console printing function, so we'll be able to test against console output as well, making test cases more flexible -- but until then, we'll just rely on checking the direct binary output of the compiler.



3. Output Files
===============

Generally, the CCScript compiler requires an existing output file in order to compile. The test framework will create a dummy file that is preinitialized to 0x600200 bytes of zeroes (the size of a 48-megabit ExHiROM image with an 0x200 byte copier header); this file will be used to collect compilation output. Each test case can also specify an alternate file to be used for compilation, which will be copied into the dummy file for each test run.


4. Test Case Syntax
===================

Individual test cases are just CCScript files with special metatags in comments at the top of the file. These tags must be contained in comments in the first non-blank lines of the file, preceded by three '/' characters. As soon as a non-blank line is encountered that does not begin with '///', no more tags are permitted.

A tag is declared as follows:

///@tagname: tagvalue


Valid tags are:

@name
-----
Sets the name of the test case.


@desc
-----
Sets a description for the test case.


@file
-----
Specifies a file to be used as the compilation target. By default a dummy file consisting of 0x600200 null bytes is used. This file will be copied for output; the specified file will not itself be modified.


@start
------
Specifies the location to dump compiled data. Defaults to $C00000. (0x200 file offset)


@expect
-------
Specifies the expected output. This can either be a literal value consisting of a sequence of concatenated string data in quotes (which can contain literal hex data in brackets), or the name of a binary file containing the expected result.

The value for the @expect tag can also be spread out over multiple successive '///' lines.

NOTE: Output is checked only up to the length of the provided expected data. If the beginning of the output matches the expected data up to the expected data's length, the output and expected data will be considered equal.


Following are two simple examples of test case headers:


Example The First:
--------------------------------------------------------------
///@name: Test Test
///@desc: Tests the ability to test tests.
///@expect:
/// "This is the expected output.[03 02]"
/// "More expected output.[13 02]"


/* The following code should result in exactly the output listed
   in the header. If not, 'tests' will report a failure. */

"This is the expected output." next
"More expected output" end

--------------------------------------------------------------


Example The Second:
--------------------------------------------------------------
///@name: A Larger Test
///@desc: Test writing a large amount of information to a ROM
///@file: EarthBound.smc
///@start: f00000
///@expect: CompiledEarthbound.smc

/* When the expected output is large or complicated, listing
   it all in the test case header would be insane, so we
   specify a precompiled binary file instead. */

write_a_whole_crapload_of_stuff()

--------------------------------------------------------------