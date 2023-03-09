
# BCPP

My version of C preprocessor with features that I would like to have in standard preprocessor.

It's a rough sketch of my ideas, but at least it capable to evaluate standard C headers on Linux and MacOS.

Additional features:
- multiline macros (that allows usage of directives inside macro and also it allows list processing)
- #calleach directive that calls anonymous macro for each line of provided multiline macro
- mechanisms for disabling macro argument evaluation and mechanism for evaluating tokens inside macro body
- #implement for including single-header library implementation
- embeded code for glsl shaders with #shader_begin and #shader_end directives. It generates static arrays for vertex and fragment shaders and groups them into programs.

You can see some examples in test*.c files. No detailed explanation whatsoever.
