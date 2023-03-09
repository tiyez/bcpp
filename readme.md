
# BCPP

My version of C preprocessor with features that I would like to have in standard preprocessor.

It's a rough sketch of my ideas, but at least it capable to evaluate standard C headers on Linux and MacOS.

Additional features:
- multiline macros
- calleach directive that calls anonymous macro for each line of provided multiline macro
- #implement for including single-header library implementation
- embeded code for glsl shaders with #shader_begin and #shader_end directives. It generates static arrays for vertex and fragment shaders and groups them into programs.
- and more.. there are a lot small additions that I don't remember anymore. you can see some usage of them in test*.c files


