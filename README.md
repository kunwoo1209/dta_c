# dta_c
Dynamic Taint Analysis for C program

The tool dynamically measures data dependencies between functions in a target program using dynamic taint analysis.

Input: target C program, system test cases

Output: Measured data dependencies between functions

OS: Ubuntu 16.04.6 LTS

![](dta.png)

## Usage

Steps

1. Preprocess a target C program using the following command: 

```sh
gcc -undef -E [target C file] > [pre-processed C file]
```

2. Put the preprocessed c file and the 5 files (combine_result.py, duchain.py, instrumentor, list_function, script.sh) in the same directory.

2. Modify script.sh (commented as "need to set") to set program name and system test cases.

3. Run script.sh

4. Check the result in [program name]_function_correlation.csv



For more information please contact kunwoo1209@gmail.com

## Development setup

Describe how to install all development dependencies and how to run an automated test-suite of some kind. Potentially do this for multiple platforms.

```sh
make install
npm test
```

## Release History

* 0.2.1
    * CHANGE: Update docs (module code remains unchanged)
* 0.2.0
    * CHANGE: Remove `setDefaultXYZ()`
    * ADD: Add `init()`
* 0.1.1
    * FIX: Crash when calling `baz()` (Thanks @GenerousContributorName!)
* 0.1.0
    * The first proper release
    * CHANGE: Rename `foo()` to `bar()`
* 0.0.1
    * Work in progress

## Meta

Your Name – [@YourTwitter](https://twitter.com/dbader_org) – YourEmail@example.com

Distributed under the XYZ license. See ``LICENSE`` for more information.

[https://github.com/yourname/github-link](https://github.com/dbader/)

## Contributing

1. Fork it (<https://github.com/yourname/yourproject/fork>)
2. Create your feature branch (`git checkout -b feature/fooBar`)
3. Commit your changes (`git commit -am 'Add some fooBar'`)
4. Push to the branch (`git push origin feature/fooBar`)
5. Create a new Pull Request

<!-- Markdown link & img dfn's -->
[npm-image]: https://img.shields.io/npm/v/datadog-metrics.svg?style=flat-square
[npm-url]: https://npmjs.org/package/datadog-metrics
[npm-downloads]: https://img.shields.io/npm/dm/datadog-metrics.svg?style=flat-square
[travis-image]: https://img.shields.io/travis/dbader/node-datadog-metrics/master.svg?style=flat-square
[travis-url]: https://travis-ci.org/dbader/node-datadog-metrics
[wiki]: https://github.com/yourname/yourproject/wiki
