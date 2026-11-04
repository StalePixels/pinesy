// Headless mode: with any arguments, the executable runs MAME with them
// and no UI, and exits with MAME's exit code.

#pragma once

// argc and argv are main()'s. Returns MAME's exit code.
int runHeadless(int argc, char **argv);
