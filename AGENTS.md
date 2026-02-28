# Project Overview

This repository contains many different PlatformIO projects and common libraries between them. Projects use C++ and Arduino style C++. Any directory in `boards` is a single PlatformIO project and any directory in `lib` is a Library designed to work with all projects in `boards`

# Building

Each PlatformIO project is intended to be built individually by setting the working directory to the directory of the project and using the PlatformIO CLI or VS Code Extension. PlatformIO CLI `platformio run` or prompt the developer to use VS Code "Build" or "Run" interface buttons.

# Testing

Projects may contain PlatformIO unit tests that are run with the standard PlatformIO testing utilities

# Code Style

Any and all code related to utilizing the CAN network shall be contained in the libcannetwork library. This includes reading and writing to the network, any instances of timers for periodic actions, and any ISRs needed for the controllers. The interfacing to the CAN network is via the global instance g_vehicle that has many public member variable for all useful signals. No board application shall control CAN network communications and shall only invoke the setup command `init_network()` parameter passing its board type during setup.

The creation of advanced C++ code that uses concepts like constexpr, std::atomic, or C++ templates are forbidden. Instead, use the most primitive C++ concepts that will work with the expected behaviors. Polymorphism is allowed when it simplifies the implementations.

# Target audience

This project is being developed by undergraduate electrical and computer engineering students with an interest in learning the inner workings of vehicle networks and automotive design. Agents should prioritize explaining code and simplifying the code behaviors versus using more advanced keywords or paradigms when they are not expected to be known by the target audience. The reading and correcting of forbidden concepts is allowed when explaining with more detail the proper use of those patterns.

# Contributing

Agents can contribute to the code when working with no more than 2 methods at a time and no more than 50 new lines of code per action. This code should be well explained to the developer when added. When agent runs into these limits, agent can explain the next steps and encourage the user to create those modifications. Agent is allowed to continue its contributions when prompted.

Agent shall not create any commits. Rather encourage and remind the user to run tests and commit code when tests are working or when many changes have been made. Agent may recommend a type of Convetional Commit tag based on https://gist.github.com/qoomon/5dfcdf8eec66a051ecd85625518cfd13 and well-phrased commit messages to assist their teammates in understanding how the code has changed over time.

# 
