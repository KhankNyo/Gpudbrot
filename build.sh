
gcc -Wextra -Wall \
    -I"./extern/glad/include/" \
    ./OpenGL.c ./extern/glad/src/glad.c \
    -o ./main \
    -lglfw
