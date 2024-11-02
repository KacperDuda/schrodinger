Initial Commit

For SFML
https://www.sfml-dev.org/tutorials/2.6/start-linux.php

Command to compile project:
g++ -std=c++20 -Iinclude -Iexternal/imgui -Iexternal/imgui-sfml main.cpp src/*.cpp external/imgui-sfml/*.cpp external/imgui/*.cpp -o result -lsfml-graphics -lsfml-window -lsfml-system -lGL -lGLU -ldl -lpthread;