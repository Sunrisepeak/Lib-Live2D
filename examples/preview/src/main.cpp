// The entry translation unit: src/app.cpp defines the Application, and this
// only runs it. `import huxerui;` rather than the header, as the framework's
// own templates do; app.cpp keeps the header form because it is the one
// source both the CMake and the mcpp build compile.

import huxerui;

int main() { return huxerui::RunApplication(); }
