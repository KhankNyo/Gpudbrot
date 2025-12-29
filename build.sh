
gcc -Wextra -Wall \
    -I"./external/glad/include/" \
    ./OpenGL.c ./external/glad/src/glad.c ./App.c\
    -o ./main \
    -lglfw
