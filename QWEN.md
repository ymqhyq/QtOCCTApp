# QtOCCTApp - 3D CAD Visualization Application

## Project Overview

QtOCCTApp is a C++ desktop application that combines Qt widgets with Open CASCADE Technology (OCCT) to create a 3D CAD visualization environment. The application provides an OpenGL-based 3D viewer that allows users to draw and manipulate geometric entities, specifically focusing on line creation and selection.

### Key Technologies
- **Qt Framework**: Used for the graphical user interface (widgets, menus, dock panels)
- **Open CASCADE Technology (OCCT)**: Provides 3D CAD kernel functionality for geometric modeling
- **OpenGL**: Rendering backend for 3D visualization
- **CMake**: Build system

### Architecture
The application follows a modular design with the following key components:

- **MainWindow**: Main application window that manages the UI layout, menus, and toolbars
- **OCCTWidget**: Custom OpenGL widget that integrates OCCT's 3D viewer and handles mouse interactions
- **Line**: Utility class for creating and managing line geometry in the 3D scene

## Building and Running

### Prerequisites
- C++17 compatible compiler
- CMake 3.16 or later
- Qt 6.10.1 development libraries (installed at D:/Qt/6.10.1/msvc2022_64)
- Open CASCADE Technology (OCCT) installation (built at D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64)

### Build Instructions
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Running the Application
After successful compilation, run the executable:
```bash
./QtOCCTApp
```

## Configuration Notes

### Library Paths
The project has been configured with the following library paths:
- Qt libraries: D:/Qt/6.10.1/msvc2022_64
- OCCT headers: D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64/inc
- OCCT libraries: D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64/win64/vc14

### Known Issues
During the initial build process, we encountered linking issues where the linker could not find the OCCT library files (e.g., TKernel.lib). This was resolved by updating the library path to the correct location: D:/GitHub_Ymqhyq/OCCT/build-vs2022-x64/win64/vc14/lib

Current linking issues are related to unresolved external symbols, particularly Qt MOC-generated functions (metaObject, qt_metacast, qt_metacall) and OCCT functions. These may be due to:
1. Missing Qt MOC compilation steps in the build process
2. Potential mismatch in OCCT library versions or configurations
3. Missing additional dependencies required by OCCT

## Features

### Current Functionality
- 3D OpenGL viewport with OCCT integration
- Line drawing capability through mouse clicks
- Line selection and highlighting
- Functional panel with drawing controls
- Dockable UI panels

### User Interface
- Main 3D viewing area (central widget)
- Left dock panel with functional controls
- Menu bar with basic file operations
- Status bar for application status

### Interaction Modes
- **Draw Line Mode**: Activated via the "Draw Line" button, allowing users to create lines by clicking twice in the 3D view
- **Selection Mode**: Default mode allowing users to select and highlight existing lines

## Development Conventions

### Coding Style
- Follows Qt's coding conventions with camelCase for methods and variables
- Header guards using `#ifndef CLASSNAME_H` pattern
- RAII principles applied for resource management
- Proper use of Qt's meta-object system (MOC) for signals and slots

### Project Structure
```
include/          # Header files
├── Line.h        # Line geometry class
├── MainWindow.h  # Main application window
├── OCCTWidget.h  # 3D OpenGL widget
└── json.hpp      # JSON library header

src/              # Source files
├── Line.cpp      # Line class implementation
├── MainWindow.cpp # Main window implementation
├── OCCTWidget.cpp # 3D widget implementation
└── main.cpp      # Application entry point

CMakeLists.txt    # Build configuration
```

### Dependencies
The project uses CMake to manage dependencies:
- Qt Core, Widgets, Gui, and OpenGL modules
- Open CASCADE Technology libraries including:
  - TKernel, TCollection, gp, Geom, Geom2d
  - Graphic3d, Prs3d, AIS, V3d, Aspect
  - StdPrs, StdObj, BRep, BRepPrim, BRepAlgoAPI
  - BRepBuilderAPI, StlAPI, Draw

## Extending the Application

### Adding New Geometry Types
New geometric entities can be added by creating classes similar to the `Line` class that wrap OCCT geometry types and integrate with the AIS (Application Interactive Services) framework.

### UI Extensions
Additional functionality can be added through the dockable panel system or by extending the menu/toolbar system in `MainWindow`.

### Interaction Modes
New interaction modes can be implemented in `OCCTWidget` by extending the mouse event handlers and adding corresponding UI controls.